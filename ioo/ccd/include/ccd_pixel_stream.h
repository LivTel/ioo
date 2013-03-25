/* ccd_pixel_stream.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_pixel_stream.h,v 1.1 2013-03-25 15:26:42 cjm Exp $
*/
#ifndef CCD_PIXEL_STREAM_H
#define CCD_PIXEL_STREAM_H

#include "ccd_interface.h"

/* #defines */
/**
 * The maximum number of Pixel_Structs that can be one pixel stream list, currently 16.
 */
#define CCD_PIXEL_STREAM_MAX_PIXEL_COUNT      (16)

/* external structure declarations */
/**
 * This structure is used as part of a pixel stream entry. It is used to define which corner
 * and which image a pixel in the stream originates from. 
 * For IO:O, the CCD is wired up to the SDSU controller as follows:
 * <pre>
 * Upper left __A SXMIT number #3 +-----+-----+ Upper right __B SXMIT number #2
 *                                |     |     |
 *                                |     |     |
 *                                |     |     |
 *                                |-----|-----|
 *                                |     |     |
 *                                |     |     |
 *                                |     |     |
 * Lower left __C SXMIT number #0 +-----+-----+ Lower right __D SXMIT number #1
 * </pre>
 * Normally, the whole image will be readout into image 0, however if we are using spare outputs as 'dummy' outputs
 * these can be written to image 1.
 * <ul>
 * <li><b>Image_Number</b> Which image this pixel belongs to, a zero-based integer. Each image is written to
 *                         a separate FITS image extension.
 * <li><b>Corner_Number</b> Which corner of the image this pixel originated from. A zero-based number from 0 to 3.
 * </ul>
 */
struct CCD_Pixel_Struct
{
	int Image_Number;
	int Corner_Number;
};



extern void CCD_Pixel_Stream_Initialise(void);
extern int CCD_Pixel_Stream_Post_Readout_Full_Frame(CCD_Interface_Handle_T* handle,unsigned short *exposure_data,
						    char *filename);
extern int CCD_Pixel_Stream_Post_Readout_Window(CCD_Interface_Handle_T* handle,unsigned short *exposure_data,
						char *filename_list[],int filename_count);
extern int CCD_Pixel_Stream_Delete_Fits_Images(char **filename_list,int filename_count);
extern int CCD_Pixel_Stream_Parse_Pixel_List(char *pixel_list_string,struct CCD_Pixel_Struct *pixel_list,
					     int *pixel_count);
extern int CCD_Pixel_Stream_Set_Pixel_Stream_Entry(enum CCD_DSP_AMPLIFIER Amplifier,
						   struct CCD_Pixel_Struct *pixel_list,
						   int pixel_count,int is_split_serial);
extern int CCD_Pixel_Stream_Get_Error_Number(void);
extern void CCD_Pixel_Stream_Error(void);
extern void CCD_Pixel_Stream_Error_String(char *error_string);
extern void CCD_Pixel_Stream_Warning(void);

#endif
