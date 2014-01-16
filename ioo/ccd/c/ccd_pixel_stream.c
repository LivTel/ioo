/* ccd_pixel_stream.c
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_pixel_stream.c,v 1.2 2014-01-16 15:33:26 cjm Exp $
*/
/**
 * ccd_pixel_stream.c contains routines to process the buffer of readout pixels returned by a readout call.
 * This can be byte swapped, de-interlaced (taking into account multiple readout amplifiers), and may
 * also contain dummy pixel data from dummy outputs (or real output nodes where no charge has been driven into them).
 * @author Chris Mottram
 * @version $Revision: 1.2 $
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#ifndef _POSIX_TIMERS
#include <sys/time.h>
#endif
#include "log_udp.h"
#include "ccd_dsp.h"
#include "ccd_exposure.h"
#include "ccd_global.h"
#include "ccd_interface.h"
#include "ccd_pixel_stream.h"
#include "ccd_setup.h"
#include "ccd_setup_private.h"
#ifdef CFITSIO
#include "fitsio.h"
#endif
#ifdef SLALIB
#include "slalib.h"
#endif /* SLALIB */
#ifdef NGATASTRO
#include "ngat_astro.h"
#include "ngat_astro_mjd.h"
#endif /* NGATASTRO */

/* internal hash defines */
/**
 * The maximum number of Image Data arrays we can have (16).
 */
#define PIXEL_STREAM_MAX_IMAGE_DATA_COUNT (16)
/**
 * The maximum numbers of corners (amplifiers) in one image (detector). Currently set to 4.
 */
#define PIXEL_STREAM_MAX_CORNER_COUNT     (4)

/* internal enumerations */
/**
 * Enumeration of corners. Note the values here represent the actual amplifier / ARC-45 DAC configuration
 * for the IO:O CCD.
 * For IO:O, the CCD is wired up to the SDSU controller as follows:
 * <pre>
 * Dummy Upper left not connected      -             - Dummy Upper right SXMIT number #3
 * Upper left __A/__H not connected    -+-----+-----+- Upper right __B/__G SXMIT number #2
 *                                      |     |     |
 *                                      |     |     |
 *                                      |     |     |
 *                                      |-----|-----|
 *                                      |     |     |
 *                                      |     |     |
 *                                      |     |     | 
 * Lower left __C/__E not connected    -+-----+-----+- Lower right __D/__F SXMIT number #0
 * Dummy Lower left not connected      -             - Dummy Lower right SXMIT number #1
 * </pre>
 * The corners are defined as follows:
 * <ul>
 * <li><b>CORNER_LOWER_LEFT</b> 0
 * <li><b>CORNER_LOWER_RIGHT</b> 1
 * <li><b>CORNER_UPPER_RIGHT</b> 2
 * <li><b>CORNER_UPPER_LEFT</b> 3
 * </ul>
 */
enum CORNER
{
	CORNER_LOWER_LEFT = 0, CORNER_LOWER_RIGHT = 1, CORNER_UPPER_RIGHT = 2, CORNER_UPPER_LEFT = 3
};

/* internal structure declarations */
/**
 * This structure is used to define a mapping between an amplifier setting, and a strategy for dealing contigous
 * pixels from a readout using that amplifier, into separate image sections, and specifying the de-interlacing
 * applied to each image (which corner of the array the pixel originated from).
 * <ul>
 * <li><b>Amplifier</b> Which amplifier the entry applies to.
 * <li><b>Pixel_List</b> A list of CCD_Pixel_Struct, 
 *        describing the originating corner of a pixel and it's destination image number. Currently a fixed list
 *        of length CCD_PIXEL_STREAM_MAX_PIXEL_COUNT, this cannot be of a dynamic length if we wish to statically
 *        define a list.
 * <li><b>Pixel_Count</b> The number of CCD_Pixel_Structs in the list. The sequence of pixel ordered is deemed to be 
 *                        repeated after this many pixels.
 * <li><b>Is_Split_Serial</b> A boolean, TRUE if the serial waveform is split serially.
 * </ul>
 * @see #CCD_PIXEL_STREAM_MAX_PIXEL_COUNT
 * @see #CCD_Pixel_Struct
 * @see ccd_dsp.html#CCD_DSP_AMPLIFIER
 */
struct Pixel_Stream_Entry
{
	enum CCD_DSP_AMPLIFIER Amplifier;
	struct CCD_Pixel_Struct Pixel_List[CCD_PIXEL_STREAM_MAX_PIXEL_COUNT];
	int Pixel_Count;
	int Is_Split_Serial;
};

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_pixel_stream.c,v 1.2 2014-01-16 15:33:26 cjm Exp $";

/* local variables */
/**
 * Variable holding error code of last operation performed by ccd_pixel_stream.
 */
static int Pixel_Stream_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Pixel_Stream_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";
/**
 * List of Pixel_Stream_Entry structs defining how to deal with pixel stream's depending on amplifier setting.
 * For IO:O, the CCD is wired up to the SDSU controller as follows:
 * <pre>
 * Dummy Upper left not connected      -             - Dummy Upper right SXMIT number #3
 * Upper left __A/__H not connected    -+-----+-----+- Upper right __B/__G SXMIT number #2
 *                                      |     |     |
 *                                      |     |     |
 *                                      |     |     |
 *                                      |-----|-----|
 *                                      |     |     |
 *                                      |     |     |
 *                                      |     |     | 
 * Lower left __C/__E not connected    -+-----+-----+- Lower right __D/__F SXMIT number #0
 * Dummy Lower left not connected      -             - Dummy Lower right SXMIT number #1
 * </pre>
 * @see #Pixel_Stream_Entry
 */
static struct Pixel_Stream_Entry Pixel_Stream_List[] = 
{
	/* This is the natural ordering of the image, where pixels from the bottom left corner are put in the
	** bottom left corner of the image. However this doesn't work for IO:O on sky orientation, we think
	** we need to put pixels from the bottom right of the CCD into the bottom left corner of the image array. i.e.
	** implying a flip in X.
	** The dummy pixels are currently put into the same side of the dummy image as the image pixels.
	{CCD_DSP_AMPLIFIER_TOP_LEFT,          {{0,3}},1,FALSE},
	{CCD_DSP_AMPLIFIER_TOP_RIGHT,         {{0,2}},1,FALSE},
	{CCD_DSP_AMPLIFIER_BOTTOM_LEFT,       {{0,0}},1,FALSE},
	{CCD_DSP_AMPLIFIER_BOTTOM_RIGHT,      {{0,1}},1,FALSE},
	{CCD_DSP_AMPLIFIER_BOTH_RIGHT,        {{0,1},{0,2}},2,FALSE},
	{CCD_DSP_AMPLIFIER_ALL,               {{0,0},{0,1},{0,2},{0,3}},4,TRUE},
	{CCD_DSP_AMPLIFIER_DUMMY_TOP_LEFT,    {{1,3},{0,3}},2,FALSE},  dummy and image pixels reversed due to SXMIT restriction 
	{CCD_DSP_AMPLIFIER_DUMMY_TOP_RIGHT,   {{0,2},{1,2}},2,FALSE},
	{CCD_DSP_AMPLIFIER_DUMMY_BOTTOM_LEFT, {{0,0},{1,0}},2,FALSE},
	{CCD_DSP_AMPLIFIER_DUMMY_BOTTOM_RIGHT,{{0,1},{1,1}},2,FALSE},
	{CCD_DSP_AMPLIFIER_DUMMY_BOTH_LEFT,   {{0,0},{1,0},{1,3},{0,3}},4,FALSE},
	{CCD_DSP_AMPLIFIER_DUMMY_BOTH_RIGHT,  {{1,1},{0,1},{0,2},{1,2}},4,FALSE}
	*/
	/* This ordered flips the image orientation in X. According to the old de-interlace code, this should put the
	** image in the correct orientation for IO:O .
	** The dummy pixels are currently put into the same side of the dummy image than the image pixels */
	/*{CCD_DSP_AMPLIFIER_TOP_LEFT,          {{0,2}},1,FALSE},*/
	{CCD_DSP_AMPLIFIER_TOP_RIGHT,         {{0,3}},1,FALSE},
	/*{CCD_DSP_AMPLIFIER_BOTTOM_LEFT,       {{0,1}},1,FALSE},*/
	{CCD_DSP_AMPLIFIER_BOTTOM_RIGHT,      {{0,0}},1,FALSE},
	{CCD_DSP_AMPLIFIER_BOTH_RIGHT,        {{0,0},{-1,-1},{0,3}},3,FALSE},
	/*{CCD_DSP_AMPLIFIER_ALL,               {{0,1},{0,0},{0,3},{0,2}},4,TRUE},
	  {CCD_DSP_AMPLIFIER_DUMMY_TOP_LEFT,    {{1,2},{0,2}},2,FALSE},*/ /* dummy and image pixels reversed due to SXMIT restriction */
	{CCD_DSP_AMPLIFIER_DUMMY_TOP_RIGHT,   {{0,3},{1,3}},2,FALSE},
	/*{CCD_DSP_AMPLIFIER_DUMMY_BOTTOM_LEFT, {{0,1},{1,1}},2,FALSE},*/
	{CCD_DSP_AMPLIFIER_DUMMY_BOTTOM_RIGHT,{{0,0},{1,0}},2,FALSE},
	/*{CCD_DSP_AMPLIFIER_DUMMY_BOTH_LEFT,   {{0,1},{1,1},{1,2},{0,2}},4,FALSE},*/
	{CCD_DSP_AMPLIFIER_DUMMY_BOTH_RIGHT,  {{0,0},{1,0},{0,3},{1,3}},4,FALSE}
};
/**
 * The number of Pixel_Stream_Entry's in Pixel_Stream_List.
 * @see #Pixel_Stream_List
 */
static int Pixel_Stream_Count = (sizeof(Pixel_Stream_List)/sizeof(Pixel_Stream_List[0]));
/**
 * An array of pointers to Image Data (malloced unsigned short data arrays).
 * The maximum number of image data arrays to be allocated is PIXEL_STREAM_MAX_IMAGE_DATA_COUNT.
 * @see #PIXEL_STREAM_MAX_IMAGE_DATA_COUNT
 */
static unsigned short *Image_Data_List[PIXEL_STREAM_MAX_IMAGE_DATA_COUNT];
/**
 * The actual number of Image_Data arrays allocated memory in Image_Data_List.
 * @see #Image_Data_Count
 */
static int Image_Data_Count;

/* internal functions */
/* we should provide an alternative for this routine if the library is not using short ints. */
#if CCD_GLOBAL_BYTES_PER_PIXEL == 2
static void Pixel_Stream_Byte_Swap(unsigned short *svalues,long nvals);
#else
#error CCD_GLOBAL_BYTES_PER_PIXEL uses illegal value.
#endif
static int Pixel_Stream_Entry_Get(enum CCD_DSP_AMPLIFIER amplifier,struct Pixel_Stream_Entry *pixel_stream_entry);
static int Pixel_Stream_Save(char *filename,unsigned short *exposure_data_list[],int exposure_data_count,
			     int ncols,int nrows,struct timespec start_time);
static void Pixel_Stream_TimeSpec_To_Date_String(struct timespec time,char *time_string);
static void Pixel_Stream_TimeSpec_To_Date_Obs_String(struct timespec time,char *time_string);
static void Pixel_Stream_TimeSpec_To_UtStart_String(struct timespec time,char *time_string);
static int Pixel_Stream_TimeSpec_To_Mjd(struct timespec time,int leap_second_correction,double *mjd);
static int fexist(char *filename);

/* ------------------------------------------------------------------
**	External Functions
** ------------------------------------------------------------------ */
/**
 * This routine sets up ccd_pixel_stream internal variables.
 * It should be called at startup.
 */
void CCD_Pixel_Stream_Initialise(void)
{
	Pixel_Stream_Error_Number = 0;
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Pixel_Stream_Initialise:%s.\n",rcsid);
}

/**
 * Post-Readout operations on a full frame exposure,
 * <ul>
 * <li>The number of columns and rows are retrieved from setup.
 * <li>The data is de-interlaced using Pixel_Stream_DeInterlace.
 * <li>The data is saved to disc using Pixel_Stream_Save.
 * </ul>
 * If an error occurs BEFORE saving the read out frame to disk, CCD_Pixel_Stream_Delete_Fits_Images is called
 * to delete any 'blank' FITS files.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param exposure_data The data read out from the CCD.
 * @param filename The FITS filename (which should already contain relevant headers), in which to write 
 *        the image data.
 * @return The routine returns TRUE if it suceeded, and FALSE if it fails.
 * @see #Image_Data_List
 * @see #Image_Data_Count
 * @see #Pixel_Stream_Entry_Get
 * @see #Pixel_Stream_Save
 * @see #CCD_Pixel_Stream_Delete_Fits_Images
 * @see #PIXEL_STREAM_MAX_CORNER_COUNT
 * @see ccd_dsp.html#CCD_DSP_IS_DUMMY_AMPLIFIER
 * @see ccd_exposure.html#CCD_Exposure_Get_Exposure_Start_Time
 * @see ccd_setup.html#CCD_Setup_Get_Binned_NCols
 * @see ccd_setup.html#CCD_Setup_Get_Binned_NRows
 * @see ccd_setup.html#CCD_Setup_Get_Readout_Pixel_Count
 * @see ccd_setup.html#CCD_Setup_Get_Amplifier
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Pixel_Stream_Post_Readout_Full_Frame(CCD_Interface_Handle_T* handle,unsigned short *exposure_data,
					     char *filename)
{
	struct Pixel_Stream_Entry pixel_stream_entry;
	enum CCD_DSP_AMPLIFIER amplifier;
	struct timespec exposure_start_time;
	char *filename_list[1];
	int corner_pixel_index[PIXEL_STREAM_MAX_IMAGE_DATA_COUNT][PIXEL_STREAM_MAX_CORNER_COUNT];
	int binned_ncols,binned_split_ncols,binned_nrows,is_dummy,i,j,pixel_stream_entry_pixel_index;
	int exposure_data_pixel_count,exposure_data_pixel_index,image_index,corner_index,image_data_x,image_data_y;
	int image_data_pixel_offset;

/* get setup details */
	binned_ncols = CCD_Setup_Get_Binned_NCols(handle);
	binned_nrows = CCD_Setup_Get_Binned_NRows(handle);
/* number of binned_cols must be a positive number */
	if(binned_ncols <= 0)
	{
		filename_list[0] = filename;
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
		Pixel_Stream_Error_Number = 1;
		sprintf(Pixel_Stream_Error_String,
			"CCD_Pixel_Stream_Post_Readout_Full_Frame:Illegal binned_ncols '%d'.",binned_ncols);
		return FALSE;
	}
/* number of binned_rows must be a positive number */
	if(binned_nrows <= 0)
	{
		filename_list[0] = filename;
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
		Pixel_Stream_Error_Number = 2;
		sprintf(Pixel_Stream_Error_String,
			"CCD_Pixel_Stream_Post_Readout_Full_Frame:Illegal binned_nrows '%d'.",binned_nrows);
		return FALSE;
	}
	/* get how many pixels we think are in the input pixel stream. */
	exposure_data_pixel_count = CCD_Setup_Get_Readout_Pixel_Count(handle);
	amplifier = CCD_Setup_Get_Amplifier(handle);
	is_dummy = CCD_DSP_IS_DUMMY_AMPLIFIER(amplifier);
	/* get how the pixels in the pixel stream are ordered */
	if(!Pixel_Stream_Entry_Get(amplifier,&pixel_stream_entry))
	{
		filename_list[0] = filename;
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
		return FALSE;
	}
	/* How many images are we creating, for IO:O if we have dummy pixels 2, else 1 */
	if(is_dummy)
		Image_Data_Count = 2;
	else
		Image_Data_Count = 1;
	/* check the retrieved pixel stream entry contains legal values */
	for(pixel_stream_entry_pixel_index = 0; pixel_stream_entry_pixel_index < pixel_stream_entry.Pixel_Count; 
	    pixel_stream_entry_pixel_index++)
	{
		/* check image_number is in range */
		if((pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Image_Number < -1)||
		   (pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Image_Number >= Image_Data_Count))
		{
			filename_list[0] = filename;
			CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
			Pixel_Stream_Error_Number = 3;
			sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Full_Frame:"
				"Illegal pixel stream entry for amplifier '%s':Pixel_Index= '%d':Image_Number = %d:"
				"Image_Data_Count = %d.",CCD_DSP_Command_Manual_To_String(amplifier),
				pixel_stream_entry_pixel_index,
				pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Image_Number,
				Image_Data_Count);
			return FALSE;
		}
		/* check corner is in range */
		if((pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Corner_Number < -1)||
		   (pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Corner_Number >= 
		    PIXEL_STREAM_MAX_CORNER_COUNT))
		{
			filename_list[0] = filename;
			CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
			Pixel_Stream_Error_Number = 24;
			sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Full_Frame:"
				"Illegal pixel stream entry for amplifier '%s':Pixel_Index= '%d':Corner_Number = %d:"
				"MAX_CORNER_COUNT = %d.",CCD_DSP_Command_Manual_To_String(amplifier),
				pixel_stream_entry_pixel_index,
				pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Corner_Number,
				PIXEL_STREAM_MAX_CORNER_COUNT);
			return FALSE;
		}
	}
	/* create Image_Data arrays */
	for(i=0; i< Image_Data_Count; i++)
	{
		Image_Data_List[i] = (unsigned short*)malloc(binned_ncols*binned_nrows*sizeof(unsigned short));
		if(Image_Data_List[i] == NULL)
		{
			filename_list[0] = filename;
			CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
			Pixel_Stream_Error_Number = 25;
			sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Full_Frame:"
				"Failed to allocate image data index %d of size (%d,%d).",i,binned_ncols,binned_nrows);
			return FALSE;
		}
	}
/* byte swap to get into right order */
#ifdef CCD_EXPOSURE_BYTE_SWAP
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			      "CCD_Pixel_Stream_Post_Readout_Full_Frame(handle=%p):Byte swapping.",handle);
#endif
	Pixel_Stream_Byte_Swap(exposure_data,expected_pixel_count);
#endif
	/* calculate ncols to use based on whether the amplifier setting is a split serial one */
	if(pixel_stream_entry.Is_Split_Serial)
		binned_split_ncols = binned_ncols / 2;
	else
		binned_split_ncols = binned_ncols;
	/* initialise corner indexes */
	for(i=0; i < PIXEL_STREAM_MAX_IMAGE_DATA_COUNT; i++)
	{
		for(j=0; j < PIXEL_STREAM_MAX_CORNER_COUNT; j++)
		{
			corner_pixel_index[i][j] = 0;
		}
	}
	/* initialise pixel_stream_entry's pixel_index */
	pixel_stream_entry_pixel_index = 0;
	/* loop over input pixels, transfering pixels to output image data */
#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Post_Readout_Full_Frame:De-Interlacing.");
#endif
	exposure_data_pixel_index = 0;
	while(exposure_data_pixel_index < exposure_data_pixel_count)
	{
		/* which image and corner does this pixel belong to */
		image_index = pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Image_Number;
		corner_index = pixel_stream_entry.Pixel_List[pixel_stream_entry_pixel_index].Corner_Number;
#if LOGGING > 11
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Pixel_Stream_Post_Readout_Full_Frame:"
				      "stream index %d with value %d belongs to image index %d corner %d "
				      "with corner pixel index %d.",
				      exposure_data_pixel_index,exposure_data[exposure_data_pixel_index],
				      image_index,corner_index,corner_pixel_index[image_index][corner_index]);
#endif
		/* check whether this pixel should be dropped */
		if((image_index >-1)&&(corner_index> -1))
		{
			/* calculate the pixel offset into the output image data array */
			switch(corner_index)
			{
				case CORNER_LOWER_LEFT:
					image_data_x = corner_pixel_index[image_index][corner_index] % 
						binned_split_ncols;
					image_data_y = corner_pixel_index[image_index][corner_index] / 
						binned_split_ncols;
					break;
				case CORNER_LOWER_RIGHT:
					image_data_x = (binned_ncols-1)-(corner_pixel_index[image_index][corner_index] 
									 % binned_split_ncols);
					image_data_y = corner_pixel_index[image_index][corner_index] / 
						binned_split_ncols;
					break;
				case CORNER_UPPER_RIGHT:
					image_data_x =  (binned_ncols-1)-(corner_pixel_index[image_index][corner_index]
									  % binned_split_ncols);
					image_data_y = (binned_nrows-1)-(corner_pixel_index[image_index][corner_index]
									 / binned_split_ncols);
					break;
				case CORNER_UPPER_LEFT:
					image_data_x = corner_pixel_index[image_index][corner_index] %
						binned_split_ncols;
					image_data_y = (binned_nrows-1)-(corner_pixel_index[image_index][corner_index]
									 / binned_split_ncols);
					break;
			}
#if LOGGING > 11
			CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Pixel_Stream_Post_Readout_Full_Frame:"
					      "stream index %d has position (%d,%d).",
					      exposure_data_pixel_index,image_data_x,image_data_y);
#endif
			image_data_pixel_offset = image_data_x+(image_data_y*binned_ncols);
#if LOGGING > 11
			CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Pixel_Stream_Post_Readout_Full_Frame:"
					      "stream index %d has image data pixel offset %d.",
					      exposure_data_pixel_index,image_data_pixel_offset);
#endif
			/* copy pixel data from input stream to output image */
			(*((Image_Data_List[image_index])+image_data_pixel_offset)) = 
				exposure_data[exposure_data_pixel_index];
			/* move to next pixel for specified image/corner */
			(corner_pixel_index[image_index][corner_index])++;
		}/* end if pixel is NOT dropped */
		/* prepare to decode the next pixel's image and corner data */
		pixel_stream_entry_pixel_index++;
		/* if we have reached the end of the pixel_stream_entry Pixel List reset the index */
		if(pixel_stream_entry_pixel_index >= pixel_stream_entry.Pixel_Count)
			pixel_stream_entry_pixel_index = 0;
		/* look at the next input pixel in exposure_data */
		exposure_data_pixel_index++;
	}/* end while on pixels in exposure_data (exposure_data_pixel_index) */
	/* if we have aborted stop and return */
	if(CCD_DSP_Get_Abort())
	{
		filename_list[0] = filename;
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,1);
		for(i=0; i< Image_Data_Count; i++)
			free(Image_Data_List[i]);
		Pixel_Stream_Error_Number = 4;
		sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Full_Frame:Aborted.");
		return FALSE;
	}
/* save the resultant image to disk */
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Post_Readout_Full_Frame:"
			      "Saving to filename %s.",filename);
#endif
	exposure_start_time = CCD_Exposure_Get_Exposure_Start_Time(handle);
	if(!Pixel_Stream_Save(filename,Image_Data_List,Image_Data_Count,binned_ncols,binned_nrows,exposure_start_time))
	{
		for(i=0; i< Image_Data_Count; i++)
			free(Image_Data_List[i]);
		/* Pixel_Stream_Save can fail but still have saved the exposure_data to disk OK */
		return FALSE;
	}
	/* free allocated image data */
	for(i=0; i< Image_Data_Count; i++)
	{
		free(Image_Data_List[i]);
		Image_Data_List[i] = NULL;
	}
	return TRUE; 
}

/**
 * Post-Readout operations on a windowed exposure.
 * <ul>
 * <li>We get necessary setup data (window flags).
 * <li>We go though the list of windows, looking for active windows.
 * <li>We retrieve setup data for active windows (width,height and pixel_count).
 * <li>We allocate space for a subimage array of the required size, and copy the relevant exposure data
 *     (applying the necessary exposure data index offset) into it.
 * <li>We call Pixel_Stream_DeInterlace to de-interlace the sub-image.
 * <li>We check whether we should be aborting.
 * <li>We save the sub-image to the relevant filename.
 * <li>We increment the exposure data index offset by the number of pixels in the sub-image.
 * <li>We increment the filename index.
 * <li>We free the sub-image data.
 * </ul>
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param exposure_data The data read out from the CCD.
 * @param filename_list The list of FITS filenames (which should already contain relevant headers), in which to write 
 *        the image data. Each window of data is saved in a separate file.
 * @return The routine returns TRUE if it suceeded, and FALSE if it fails.
 * @see #Pixel_Stream_DeInterlace
 * @see #Pixel_Stream_Save
 * @see ccd_exposure.html#CCD_Exposure_Get_Exposure_Start_Time
 * @see ccd_setup.html#CCD_SETUP_WINDOW_COUNT
 * @see ccd_setup.html#CCD_Setup_Get_Window_Flags
 * @see ccd_setup.html#CCD_Setup_Get_DeInterlace_Type
 * @see ccd_setup.html#CCD_Setup_Get_Window_Width
 * @see ccd_setup.html#CCD_Setup_Get_Window_Height
 * @see ccd_setup.html#CCD_Setup_Get_Window_Pixel_Count
 * @see ccd_global.html#CCD_GLOBAL_BYTES_PER_PIXEL
 * @see ccd_dsp.html#CCD_DSP_IS_DEINTERLACE_TYPE
 * @see ccd_dsp.html#CCD_DSP_Get_Abort
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Pixel_Stream_Post_Readout_Window(CCD_Interface_Handle_T* handle,unsigned short *exposure_data,
						char *filename_list[],int filename_count)
{
	struct timespec exposure_start_time;
	unsigned short *subimage_data_list[1];
	unsigned short *subimage_data = NULL;
	int exposure_data_index = 0;
	int window_number,window_flags,filename_index;
	int ncols,nrows,pixel_count;

	/* get setup data */
	window_flags = CCD_Setup_Get_Window_Flags(handle);
/* byte swap to get into right order */
#ifdef CCD_EXPOSURE_BYTE_SWAP
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			      "CCD_Pixel_Stream_Post_Readout_Window(handle=%p):Byte swapping.",handle);
#endif
	Pixel_Stream_Byte_Swap(exposure_data,expected_pixel_count);
#endif
	/* go through list of windows */
	exposure_data_index = 0;
	filename_index = 0;
	for(window_number = 0;window_number < CCD_SETUP_WINDOW_COUNT; window_number++)
	{
		/* look for windows that have been read out (are in use). */
		/* Note, relies on CCD_SETUP_WINDOW_ONE == (1<<0),
		** CCD_SETUP_WINDOW_TWO	== (1<<1),
		** CCD_SETUP_WINDOW_THREE == (1<<2) and
		** CCD_SETUP_WINDOW_FOUR == (1<<3) */
		if(window_flags&(1<<window_number))
		{
			ncols = CCD_Setup_Get_Window_Width(handle,window_number);
			nrows = CCD_Setup_Get_Window_Height(handle,window_number);
			pixel_count = CCD_Setup_Get_Window_Pixel_Count(handle,window_number);
			if(filename_index >= filename_count)
			{
				Pixel_Stream_Error_Number = 6;
				sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Window:"
					"Filename index %d greater than count %d.",filename_index,filename_count);
				return FALSE;
			}
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Post_Readout_Window:"
			      "Window %d(%s) active:ncols = %d,nrows = %d,pixel_count = %d.",
					      window_number,filename_list[filename_index],ncols,nrows,pixel_count);
#endif
			subimage_data = (unsigned short*)malloc(pixel_count*CCD_GLOBAL_BYTES_PER_PIXEL);
			if(subimage_data == NULL)
			{
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list+filename_index,
								   filename_count-filename_index);
				Pixel_Stream_Error_Number = 7;
				sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Window:"
					"SubImage Data was NULL (%d,%d).",window_number,pixel_count);
				return FALSE;
			}
			memcpy(subimage_data,exposure_data+exposure_data_index,pixel_count*CCD_GLOBAL_BYTES_PER_PIXEL);
#if LOGGING > 4
			CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,
				       "CCD_Pixel_Stream_Post_Readout_Window:De-Interlacing.");
#endif
			/* diddly rewrite needed here
			if(!Pixel_Stream_DeInterlace(ncols,nrows,subimage_data,deinterlace_type))
			{
				free(subimage_data);
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list+filename_index,
								   filename_count-filename_index);
				return FALSE;
			}
			*/
/* if we have aborted stop and return */
			if(CCD_DSP_Get_Abort())
			{
				free(subimage_data);
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list+filename_index,
								   filename_count-filename_index);
				Pixel_Stream_Error_Number = 8;
				sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Post_Readout_Window:Aborted.");
				return FALSE;
			}
/* save the resultant image to disk */
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Post_Readout_Window:"
			      "Saving to filename %s.",filename_list[filename_index]);
#endif
			exposure_start_time = CCD_Exposure_Get_Exposure_Start_Time(handle);
			subimage_data_list[0] = subimage_data;
			if(!Pixel_Stream_Save(filename_list[filename_index],subimage_data_list,1,ncols,nrows,
					      exposure_start_time))
			{
				free(subimage_data);
				/* Pixel_Stream_Save can fail but still have saved the exposure_data to disk OK */
				return FALSE;
			}
			/* increment index into exposure data to start of next window. */
			exposure_data_index += pixel_count;
			/* increment index iff this window is active - only active window filenames in filename_list */
			filename_index++;
			/* free subimage */
			free(subimage_data);
		}
	}
	return TRUE;
}

/**
 * Flip the image data in the X direction.
 * @param ncols The number of columns on the CCD.
 * @param nrows The number of rows on the CCD.
 * @param exposure_data The image data received from the CCD. The data in this array is flipped in the X direction.
 * @return If everything was successful TRUE is returned, otherwise FALSE is returned.
 */
int CCD_Pixel_Stream_Flip_X(int ncols,int nrows,unsigned short *exposure_data)
{
	int x,y;
	unsigned short int tempval;

	/* for each row */
	for(y=0;y<nrows;y++)
	{
		/* for the first half of the columns.
		** Note the middle column will be missed, this is OK as it
		** does not need to be flipped if it is in the middle */
		for(x=0;x<(ncols/2);x++)
		{
			/* Copy exposure_data[x,y] to tempval */
			tempval = *(exposure_data+(y*ncols)+x);
			/* Copy exposure_data[ncols-(x+1),y] to exposure_data[x,y] */
			*(exposure_data+(y*ncols)+x) = *(exposure_data+(y*ncols)+(ncols-(x+1)));
			/* Copy tempval = exposure_data[ncols-(x+1),y] */
			*(exposure_data+(y*ncols)+(ncols-(x+1))) = tempval;
		}
	}
	return TRUE;
}

/**
 * Flip the image data in the Y direction.
 * @param ncols The number of columns on the CCD.
 * @param nrows The number of rows on the CCD.
 * @param exposure_data The image data received from the CCD. The data in this array is flipped in the Y direction.
 * @return If everything was successful TRUE is returned, otherwise FALSE is returned.
 */
int CCD_Pixel_Stream_Flip_Y(int ncols,int nrows,unsigned short *exposure_data)
{
	int x,y;
	unsigned short int tempval;

	/* for the first half of the rows.
	** Note the middle row will be missed, this is OK as it
	** does not need to be flipped if it is in the middle */
	for(y=0;y<(nrows/2);y++)
	{
		/* for each column */
		for(x=0;x<ncols;x++)
		{
			/* Copy exposure_data[x,y] to tempval */
			tempval = *(exposure_data+(y*ncols)+x);
			/* Copy exposure_data[x,nrows-(y+1)] to exposure_data[x,y] */
			*(exposure_data+(y*ncols)+x) = *(exposure_data+(((nrows-(y+1))*ncols)+x));
			/* Copy tempval = exposure_data[x,nrows-(y+1)] */
			*(exposure_data+(((nrows-(y+1))*ncols)+x)) = tempval;
		}
	}
	return TRUE;
}

/**
 * Routine used to delete any of the filenames specified in filename_list, if they exist on disk.
 * This is done as part of aborting or when an error occurs during an exposure sequence.
 * This stops FITS images being left on disk with blank image data within them, which the data pipeline
 * does not like.
 * @param filename_list A list of strings, containing filenames to delete. These filenames should be
 *        a list of FITS images passed to the CCD_Exposure_Expose routine (a list of windows to readout to).
 * @param filename_count The number of filenames in filename_list.
 * @return The routine returns TRUE if it succeeded, FALSE if it fails. 
 * @see ccd_exposure.html#CCD_Exposure_Expose
 * @see #fexist
 */
int CCD_Pixel_Stream_Delete_Fits_Images(char **filename_list,int filename_count)
{
	int i,retval,local_errno;

#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Delete_Fits_Images:Started.");
#endif
	for(i=0;i<filename_count; i++)
	{
		if(fexist(filename_list[i]))
		{
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Delete_Fits_Images:"
					      "Removing file %s (index %d).",filename_list[i],i);
#endif
			retval = remove(filename_list[i]);
			local_errno = errno;
			if(retval != 0)
			{
				Pixel_Stream_Error_Number = 9;
				sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Delete_Fits_Images: "
					"remove failed(%s,%d,%d,%s).",filename_list[i],retval,local_errno,
					strerror(local_errno));
				return FALSE;
			}
		}/* end if exist */
		else
		{
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Delete_Fits_Images:"
					      "file %s (index %d) does not exist?",filename_list[i],i);
#endif
		}
	}/* end for */
#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Pixel_Stream_Delete_Fits_Images:Finished.");
#endif
	return TRUE;
}

/**
 * Routine to parse a pixel list string into an array of CCD_Pixel_Structs, describing the ordering of a pixel
 * stream of data read out from the CCD.
 * @param pixel_list_string The string containing the string representation of the list, of the form:
 *        [I|i]&lt;image number&gt;[C|c]&lt;corner number&gt;[[I|i]&lt;image number&gt;[C|c]&lt;corner number&gt;...]
 * @param pixel_list An already allocated array of list of CCD_Pixel_Struct's to store the parsed data. This can be
 *        a static array of length CCD_PIXEL_STREAM_MAX_PIXEL_COUNT, 
 *        as this is the maximum number we can have in the list.
 * @param pixel_count The address of an integer, on return
 * @see #CCD_PIXEL_STREAM_MAX_PIXEL_COUNT
 * @see #CCD_Pixel_Struct
 */
int CCD_Pixel_Stream_Parse_Pixel_List(char *pixel_list_string,struct CCD_Pixel_Struct *pixel_list,int *pixel_count)
{
	int string_index,retval,increment,parsed_image,parsed_count;

	if(pixel_list_string == NULL)
	{
		Pixel_Stream_Error_Number = 5;
		sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:pixel_list_string was NULL.");
		return FALSE;
	}
	if(pixel_list == NULL)
	{
		Pixel_Stream_Error_Number = 10;
		sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:pixel_list was NULL.");
		return FALSE;
	}
	if(pixel_count == NULL)
	{
		Pixel_Stream_Error_Number = 11;
		sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:pixel_count was NULL.");
		return FALSE;
	}
	(*pixel_count) = 0;
	string_index = 0;
	parsed_image = FALSE;
	parsed_count = FALSE;
	while(string_index < strlen(pixel_list_string))
	{
		switch(pixel_list_string[string_index])
		{
			case 'i':
			case 'I':
				if((*pixel_count) >= CCD_PIXEL_STREAM_MAX_PIXEL_COUNT)
				{
					Pixel_Stream_Error_Number = 15;
					sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:"
						"Too many pixels in list: %d vs %d.",
						(*pixel_count),CCD_PIXEL_STREAM_MAX_PIXEL_COUNT);
					return FALSE;
				}
				retval = sscanf(pixel_list_string+string_index+1,"%d%n",
						&(pixel_list[(*pixel_count)].Image_Number),&increment);
				if((retval < 1)||(retval > 2))
				{
					Pixel_Stream_Error_Number = 12;
					sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:"
						"Failed to parse '%s' at index %d:sscanf returned %d.",
						pixel_list_string,string_index+1,retval);
					return FALSE;
				}
				string_index += increment;
				parsed_image = TRUE;
				break;
			case 'c':
			case 'C':
				if((*pixel_count) >= CCD_PIXEL_STREAM_MAX_PIXEL_COUNT)
				{
					Pixel_Stream_Error_Number = 16;
					sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:"
						"Too many pixels in list: %d vs %d.",
						(*pixel_count),CCD_PIXEL_STREAM_MAX_PIXEL_COUNT);
					return FALSE;
				}
				retval = sscanf(pixel_list_string+string_index+1,"%d%n",
						&(pixel_list[(*pixel_count)].Corner_Number),&increment);
				if((retval < 1)||(retval > 2))
				{
					Pixel_Stream_Error_Number = 13;
					sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:"
						"Failed to parse '%s' at index %d:sscanf returned %d.",
						pixel_list_string,string_index+1,retval);
					return FALSE;
				}
				string_index += increment;
				parsed_count = TRUE;
				/* If a complete pixel has been parsed, move to the next pixel */
				if(parsed_image && parsed_count)
				{
					(*pixel_count)++;
					parsed_image = FALSE;
					parsed_count = FALSE;
				}
				break;
			default:
				Pixel_Stream_Error_Number = 14;
				sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Parse_Pixel_List:"
					"Failed to parse '%s' at index %d:Found illegal character %c.",
					pixel_list_string,string_index,pixel_list_string[string_index]);
				return FALSE;
		}
		/* parse next character */
		string_index++;
	}
	return TRUE;
}

/**
 * Routine to set the pixel list and other settings for the specified parameter.
 * @param amplifier The amplifier setting to modify.
 * @param pixel_list A list of CCD_Pixel_Structs describing the ordering of pixels received from the SDSU controller,
 *        in terms of the originating corner of the CCD and which image it belongs to (real or dummy).
 * @param pixel_count The number of pixels in the list.
 * @param is_split_serial A boolean. If TRUE, the pixel stream is split in a serial direction, and hence a different
 *        calculation is needed for determine the pixel's position in the de-interlaced image.
 * @see #Pixel_Stream_List
 * @see #Pixel_Stream_Count
 */
int CCD_Pixel_Stream_Set_Pixel_Stream_Entry(enum CCD_DSP_AMPLIFIER amplifier,struct CCD_Pixel_Struct *pixel_list,
					    int pixel_count,int is_split_serial)
{
	int found, pixel_stream_index,i;

	/* find index of current settings in the list */
	found = FALSE;
	pixel_stream_index = 0;
	while((pixel_stream_index < Pixel_Stream_Count)&&(found == FALSE))
	{
		if(Pixel_Stream_List[pixel_stream_index].Amplifier == amplifier)
		{
			found = TRUE;
		}
		else
			pixel_stream_index++;
	}
	if(found == FALSE)
	{
		Pixel_Stream_Error_Number = 33;
		sprintf(Pixel_Stream_Error_String,"CCD_Pixel_Stream_Set_Pixel_Stream_Entry: "
			"Amplifier '%s' not found in  Pixel_Stream_List of length %d.",
			CCD_DSP_Command_Manual_To_String(amplifier),Pixel_Stream_Count);
		return FALSE;
	}
	/* update settings */
	Pixel_Stream_List[pixel_stream_index].Pixel_Count = pixel_count;
	for(i=0;i<pixel_count;i++)
	{
		Pixel_Stream_List[pixel_stream_index].Pixel_List[i] = pixel_list[i];
	}
	Pixel_Stream_List[pixel_stream_index].Is_Split_Serial = is_split_serial;
	return TRUE;
}

/**
 * Get the current value of ccd_pixel_stream's error number.
 * @return The current value of ccd_pixel_stream's error number.
 */
int CCD_Pixel_Stream_Get_Error_Number(void)
{
	return Pixel_Stream_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_pixel_stream in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Pixel_Stream_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Pixel_Stream_Error_Number == 0)
		sprintf(Pixel_Stream_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Pixel_Stream:Error(%d) : %s\n",time_string,Pixel_Stream_Error_Number,
		Pixel_Stream_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_pixel_stream in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 *        being passed to this routine. The routine will try to concatenate it's error string onto the end
 *        of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Pixel_Stream_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Pixel_Stream_Error_Number == 0)
		sprintf(Pixel_Stream_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Pixel_Stream:Error(%d) : %s\n",time_string,
		Pixel_Stream_Error_Number,Pixel_Stream_Error_String);
}

/**
 * The warning routine that reports any warnings occuring in ccd_setup in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Pixel_Stream_Warning(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an warning message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an warning to display */
	if(Pixel_Stream_Error_Number == 0)
		sprintf(Pixel_Stream_Error_String,"Logic Error:No Warning defined");
	fprintf(stderr,"%s CCD_Pixel_Stream:Warning(%d) : %s\n",time_string,Pixel_Stream_Error_Number,
		Pixel_Stream_Error_String);
}


/* ------------------------------------------------------------------
**	Internal Functions
** ------------------------------------------------------------------ */
#if CCD_GLOBAL_BYTES_PER_PIXEL == 2
/**
 * Swap the bytes in the input unsigned short integers: ( 0 1 -> 1 0 ).
 * Based on CFITSIO's ffswap2 routine. This routine only works for CCD_GLOBAL_BYTES_PER_PIXEL == 2.
 * @param svalues A list of unsigned short values to byte swap.
 * @param nvals The number of values in svalues.
 */
static void Pixel_Stream_Byte_Swap(unsigned short *svalues,long nvals)
{
	register char *cvalues;
	register long i;

/* equivalence an array of 2 bytes with a short */
	union u_tag
	{
		char cvals[2];
		unsigned short sval;
	} u;
#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"Pixel_Stream_Byte_Swap:Started.");
#endif
/* copy the initial pointer value */
	cvalues = (char *) svalues;

	for (i = 0; i < nvals;)
	{
	/* copy next short to temporary buffer */
		u.sval = svalues[i++];
	/* copy the 2 bytes to output in turn */
		*cvalues++ = u.cvals[1];
		*cvalues++ = u.cvals[0];
	}
#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"Pixel_Stream_Byte_Swap:Completed.");
#endif
}
#else
#error Pixel_Stream_Byte_Swap not defined for this value of CCD_GLOBAL_BYTES_PER_PIXEL.
#endif

/**
 * Find the pixel stream entry associated with the specified amplifier in the Pixel_Stream_List.
 * @param amplifier The amplifier to search for, of type enum CCD_DSP_AMPLIFIER.
 * @param pixel_stream_entry The address of a Pixel_Stream_Entry structure to return the found pixel stream entry.
 * @return The routine returns TRUE on sucess and FALSE on failure.
 * @see #Pixel_Stream_List
 * @see #Pixel_Stream_Count
 * @see ccd_dsp.h#CCD_DSP_Command_Manual_To_String
 */
static int Pixel_Stream_Entry_Get(enum CCD_DSP_AMPLIFIER amplifier,struct Pixel_Stream_Entry *pixel_stream_entry)
{
	int pixel_stream_index,found;

	if(pixel_stream_entry == NULL)
	{
		Pixel_Stream_Error_Number = 26;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Entry_Get: pixel_stream_entry was NULL.");
		return FALSE;
	}
	found = FALSE;
	pixel_stream_index = 0;
	while((pixel_stream_index < Pixel_Stream_Count)&&(found == FALSE))
	{
		if(Pixel_Stream_List[pixel_stream_index].Amplifier == amplifier)
		{
			(*pixel_stream_entry) = Pixel_Stream_List[pixel_stream_index];
			found = TRUE;
		}
		else
			pixel_stream_index++;
	}
	if(found == FALSE)
	{
		Pixel_Stream_Error_Number = 31;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Entry_Get: "
			"Amplifier '%s' not found in  Pixel_Stream_List of length %d.",
			CCD_DSP_Command_Manual_To_String(amplifier),Pixel_Stream_Count);
	}
	return found;
}

#ifdef CFITSIO
/**
 * This routine takes some image data and saves it in a file on disc. It also updates the 
 * DATE-OBS FITS keyword to the value saved just before the SEX command was sent to the controller.
 * @param filename The filename to save the data into.
 * @param exposure_data_list A list of allocated data memory to save.
 * @param exposure_data_count The number of image data areas in exposure_data_list.
 * @param ncols The number of columns in the image data.
 * @param nrows The number of rows in the image data.
 * @param start_time The start time of the exposure.
 * @return Returns TRUE if the image is saved successfully, FALSE if it fails.
 * @see #Pixel_Stream_TimeSpec_To_Date_String
 * @see #Pixel_Stream_TimeSpec_To_Date_Obs_String
 * @see #Pixel_Stream_TimeSpec_To_UtStart_String
 * @see #Pixel_Stream_TimeSpec_To_Mjd
 */
static int Pixel_Stream_Save(char *filename,unsigned short *exposure_data_list[],int exposure_data_count,
			     int ncols,int nrows,struct timespec start_time)
{
	fitsfile *fp = NULL;
	int retval=0,status=0,i;
	char buff[32]; /* fits_get_errstatus returns 30 chars max */
	long axes_list[2];
	char exposure_start_time_string[64];
	double mjd;

#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"Pixel_Stream_Save:Started.");
#endif
	/* try to open file */
	retval = fits_open_file(&fp,filename,READWRITE,&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		Pixel_Stream_Error_Number = 17;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: File open failed(%s,%d,%s).",filename,status,
			buff);
		return FALSE;
	}
	/* flip the data QUAD requires a flip in X, BOTHRIGHT requires a flip in Y (now corrected in DeInterlace) */
	/*CCD_Pixel_Stream_Flip_X(ncols,nrows,exposure_data);*/
	/* write the data */
	retval = fits_write_img(fp,TUSHORT,1,ncols*nrows,exposure_data_list[0],&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		fits_close_file(fp,&status);
		Pixel_Stream_Error_Number = 18;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: File write failed(%s,%d,%s).",filename,status,
			buff);
		return FALSE;
	}
/* update DATE keyword */
	Pixel_Stream_TimeSpec_To_Date_String(start_time,exposure_start_time_string);
	retval = fits_update_key(fp,TSTRING,"DATE",exposure_start_time_string,NULL,&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		fits_close_file(fp,&status);
		Pixel_Stream_Error_Number = 19;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: Updating DATE failed(%s,%d,%s).",filename,status,
			buff);
		return FALSE;
	}
/* update DATE-OBS keyword */
	Pixel_Stream_TimeSpec_To_Date_Obs_String(start_time,exposure_start_time_string);
	retval = fits_update_key(fp,TSTRING,"DATE-OBS",exposure_start_time_string,NULL,&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		fits_close_file(fp,&status);
		Pixel_Stream_Error_Number = 20;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: Updating DATE-OBS failed(%s,%d,%s).",filename,
			status,buff);
		return FALSE;
	}
/* update UTSTART keyword */
	Pixel_Stream_TimeSpec_To_UtStart_String(start_time,exposure_start_time_string);
	retval = fits_update_key(fp,TSTRING,"UTSTART",exposure_start_time_string,NULL,&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		fits_close_file(fp,&status);
		Pixel_Stream_Error_Number = 21;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: Updating UTSTART failed(%s,%d,%s).",filename,
			status,buff);
		return FALSE;
	}
/* update MJD keyword */
/* note leap second correction not implemented yet (always FALSE). */
	if(!Pixel_Stream_TimeSpec_To_Mjd(start_time,FALSE,&mjd))
		return FALSE;
	retval = fits_update_key_fixdbl(fp,"MJD",mjd,6,NULL,&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		fits_close_file(fp,&status);
		Pixel_Stream_Error_Number = 22;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: Updating MJD failed(%.2f,%s,%d,%s).",mjd,
			filename,status,buff);
		return FALSE;
	}
	/* write any extra image extensions needed */
	for(i=1; i < exposure_data_count; i++)
	{
		/* create a new HDU, with an image the same size as the primary one */
		axes_list[0] = ncols;
		axes_list[1] = nrows;
		retval = fits_create_img(fp,TUSHORT,2,axes_list,&status);
		if(retval)
		{
			fits_get_errstatus(status,buff);
			fits_report_error(stderr,status);
			fits_close_file(fp,&status);
			Pixel_Stream_Error_Number = 28;
			sprintf(Pixel_Stream_Error_String,
				"Pixel_Stream_Save:Creating image extension %d failed(%s,%d,%s).",i,filename,status,
				buff);
			return FALSE;
		}
		/* move to HDU i+1. HDU 1 is the primary image data HDU. */
		retval = fits_movabs_hdu(fp,i+1,NULL,&status);
		if(retval)
		{
			fits_get_errstatus(status,buff);
			fits_report_error(stderr,status);
			fits_close_file(fp,&status);
			Pixel_Stream_Error_Number = 29;
			sprintf(Pixel_Stream_Error_String,
				"Pixel_Stream_Save: Moving to extension HDU %d failed(%s,%d,%s).",i+1,filename,
				status,buff);
			return FALSE;
		}
		/* write image data */
		retval = fits_write_img(fp,TUSHORT,1,ncols*nrows,exposure_data_list[i],&status);
		if(retval)
		{
			fits_get_errstatus(status,buff);
			fits_report_error(stderr,status);
			fits_close_file(fp,&status);
			Pixel_Stream_Error_Number = 30;
			sprintf(Pixel_Stream_Error_String,
				"Pixel_Stream_Save: File write failed(%s,%d,%d,%s).",filename,i,status,buff);
			return FALSE;
		}
	}/* end for on i : 1 -> exposure_data_count */
/* close file */
	retval = fits_close_file(fp,&status);
	if(retval)
	{
		fits_get_errstatus(status,buff);
		fits_report_error(stderr,status);
		Pixel_Stream_Error_Number = 23;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_Save: File close failed(%s,%d,%s).",filename,status,
			buff);
		return FALSE;
	}
#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"Pixel_Stream_Save:Completed.");
#endif
	return TRUE;
}
#else
#error "CFITSIO not defined."
#endif

/**
 * Routine to convert a timespec structure to a DATE sytle string to put into a FITS header.
 * This uses gmtime and strftime to format the string. The resultant string is of the form:
 * <b>CCYY-MM-DD</b>, which is equivalent to %Y-%m-%d passed to strftime.
 * @param time The time to convert.
 * @param time_string The string to put the time representation in. The string must be at least
 * 	12 characters long.
 */
static void Pixel_Stream_TimeSpec_To_Date_String(struct timespec time,char *time_string)
{
	struct tm *tm_time = NULL;

	tm_time = gmtime(&(time.tv_sec));
	strftime(time_string,12,"%Y-%m-%d",tm_time);
}

/**
 * Routine to convert a timespec structure to a DATE-OBS sytle string to put into a FITS header.
 * This uses gmtime and strftime to format most of the string, and tags the milliseconds on the end.
 * The resultant form of the string is <b>CCYY-MM-DDTHH:MM:SS.sss</b>.
 * @param time The time to convert.
 * @param time_string The string to put the time representation in. The string must be at least
 * 	24 characters long.
 * @see ccd_global.html#CCD_GLOBAL_ONE_MILLISECOND_NS
 */
static void Pixel_Stream_TimeSpec_To_Date_Obs_String(struct timespec time,char *time_string)
{
	struct tm *tm_time = NULL;
	char buff[32];
	int milliseconds;

	tm_time = gmtime(&(time.tv_sec));
	strftime(buff,32,"%Y-%m-%dT%H:%M:%S.",tm_time);
	milliseconds = (((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
	sprintf(time_string,"%s%03d",buff,milliseconds);
}

/**
 * Routine to convert a timespec structure to a UTSTART sytle string to put into a FITS header.
 * This uses gmtime and strftime to format most of the string, and tags the milliseconds on the end.
 * @param time The time to convert.
 * @param time_string The string to put the time representation in. The string must be at least
 * 	14 characters long.
 * @see ccd_global.html#CCD_GLOBAL_ONE_MILLISECOND_NS
 */
static void Pixel_Stream_TimeSpec_To_UtStart_String(struct timespec time,char *time_string)
{
	struct tm *tm_time = NULL;
	char buff[16];
	int milliseconds;

	tm_time = gmtime(&(time.tv_sec));
	strftime(buff,16,"%H:%M:%S.",tm_time);
	milliseconds = (((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
	sprintf(time_string,"%s%03d",buff,milliseconds);
}

/**
 * Routine to convert a timespec structure to a Modified Julian Date (decimal days) to put into a FITS header.
 * <p>If SLALIB is defined, this uses slaCldj to get the MJD for zero hours, 
 * and then adds hours/minutes/seconds/milliseconds on the end as a decimal.
 * <p>If NGATASTRO is defined, this uses NGAT_Astro_Timespec_To_MJD to get the MJD.
 * <p>If neither SLALIB or NGATASTRO are defined at compile time, this routine should throw an error
 * when compiling.
 * <p>This routine is still wrong for last second of the leap day, as gmtime will return 1st second of the next day.
 * Also note the passed in leap_second_correction should change at midnight, when the leap second occurs.
 * None of this should really matter, 1 second will not affect the MJD for several decimal places.
 * @param time The time to convert.
 * @param leap_second_correction A number representing whether a leap second will occur. This is normally zero,
 * 	which means no leap second will occur. It can be 1, which means the last minute of the day has 61 seconds,
 *	i.e. there are 86401 seconds in the day. It can be -1,which means the last minute of the day has 59 seconds,
 *	i.e. there are 86399 seconds in the day.
 * @param mjd The address of a double to store the calculated MJD.
 * @return The routine returns TRUE if it succeeded, FALSE if it fails. 
 *         slaCldj and NGAT_Astro_Timespec_To_MJD can fail.
 */
static int Pixel_Stream_TimeSpec_To_Mjd(struct timespec time,int leap_second_correction,double *mjd)
{
#ifdef SLALIB
	struct tm *tm_time = NULL;
	int year,month,day;
	double seconds_in_day = 86400.0;
	double elapsed_seconds;
	double day_fraction;
#endif
	int retval;

#ifdef SLALIB
/* check leap_second_correction in range */
/* convert time to ymdhms*/
	tm_time = gmtime(&(time.tv_sec));
/* convert tm_time data to format suitable for slaCldj */
	year = tm_time->tm_year+1900; /* tm_year is years since 1900 : slaCldj wants full year.*/
	month = tm_time->tm_mon+1;/* tm_mon is 0..11 : slaCldj wants 1..12 */
	day = tm_time->tm_mday;
/* call slaCldj to get MJD for 0hr */
	slaCldj(year,month,day,mjd,&retval);
	if(retval != 0)
	{
		Pixel_Stream_Error_Number = 27;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_TimeSpec_To_Mjd:slaCldj(%d,%d,%d) failed(%d).",year,month,
			day,retval);
		return FALSE;
	}
/* how many seconds were in the day */
	seconds_in_day = 86400.0;
	seconds_in_day += (double)leap_second_correction;
/* calculate the number of elapsed seconds in the day */
	elapsed_seconds = (double)tm_time->tm_sec + (((double)time.tv_nsec) / 1.0E+09);
	elapsed_seconds += ((double)tm_time->tm_min) * 60.0;
	elapsed_seconds += ((double)tm_time->tm_hour) * 3600.0;
/* calculate day fraction */
	day_fraction = elapsed_seconds / seconds_in_day;
/* add day_fraction to mjd */
	(*mjd) += day_fraction;
#else
#ifdef NGATASTRO
	retval = NGAT_Astro_Timespec_To_MJD(time,leap_second_correction,mjd);
	if(retval == FALSE)
	{
		Pixel_Stream_Error_Number = 32;
		sprintf(Pixel_Stream_Error_String,"Pixel_Stream_TimeSpec_To_Mjd:NGAT_Astro_Timespec_To_MJD failed.\n");
		/* concatenate NGAT Astro library error onto Pixel_Stream_Error_String */
		NGAT_Astro_Error_String(Pixel_Stream_Error_String+strlen(Pixel_Stream_Error_String));
		return FALSE;
	}
#else
#error Neither NGATASTRO or SLALIB are defined: No library defined for MJD calculation.
#endif
#endif
	return TRUE;
}

/**
 * Return whether the specified filename exists or not.
 * @param filename A string representing the filename to test.
 * @return The routine returns TRUE if the filename exists, and FALSE if it does not exist. 
 */
static int fexist(char *filename)
{
	FILE *fptr = NULL;

	fptr = fopen(filename,"r");
	if(fptr == NULL )
		return FALSE;
	fclose(fptr);
	return TRUE;
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.1  2013/03/25 15:15:03  cjm
** Initial revision
**
*/
