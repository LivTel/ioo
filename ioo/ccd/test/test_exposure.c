/* test_exposure.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_exposure.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "log_udp.h"
#include "ccd_dsp.h"
#include "ccd_dsp_download.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_temperature.h"
#include "ccd_setup.h"
#include "ccd_exposure.h"
#include "fitsio.h"

/**
 * This program initialises the SDSU controller using CCD_Setup_Startup.
 * It configures the CCD using CCD_Setup_Dimensions.
 * It then calls CCD_Exposure_Expose or CCD_Exposure_Bias to do a bias,dark or exposure.
 * <pre>
 * test_exposure [-i[nterface_device] &lt;pci|text&gt;] [-device_pathname &lt;path&gt;] 
 *      [-pci_filename &lt;filename&gt;]
 * 	[-timing_filename &lt;filename&gt;][-utility_filename &lt;filename&gt;][-temperature &lt;temperature&gt;]
 * 	[-xs[ize] &lt;no. of pixels&gt;][-ys[ize] &lt;no. of pixels&gt;]
 * 	[-xb[in] &lt;binning factor&gt;][-yb[in] &lt;binning factor&gt;]
 * 	[-a[mplifier] &lt;bottomleft|bottomright|topleft|topright|all&gt;]
 * 	[-w[indow] &lt;no&gt; &lt;xstart&gt; &lt;ystart&gt; &lt;xend&gt; &lt;yend&gt;]
 * 	[-b[ias]][-d[ark] &lt;exposure length&gt;][-e[xpose] &lt;exposure length&gt;]\n");
 * 	[-f[ilename] &lt;filename&gt;]
 * 	[-t[ext_print_level] &lt;commands|replies|values|all&gt;][-h[elp]]
 * </pre>
 * @author $Author: cjm $
 * @version $Revision: 1.1 $
 */
/* hash definitions */
/**
 * Maximum length of some of the strings in this program.
 */
#define MAX_STRING_LENGTH	(256)
/**
 * Default temperature to set the CCD to.
 */
#define DEFAULT_TEMPERATURE	(-110.0)
/**
 * Default number of columns in the CCD.
 */
#define DEFAULT_SIZE_X		(4132)
/**
 * Default number of rows in the CCD.
 */
#define DEFAULT_SIZE_Y		(4096)
/**
 * Default amplifier.
 */
#define DEFAULT_AMPLIFIER	(CCD_DSP_AMPLIFIER_BOTTOM_LEFT)
/**
 * Default de-interlace type.
 */
#define DEFAULT_DEINTERLACE_TYPE (CCD_DSP_DEINTERLACE_SINGLE)

/* enums */
/**
 * Enumeration determining which command this program executes. One of:
 * <ul>
 * <li>COMMAND_ID_NONE
 * <li>COMMAND_ID_BIAS
 * <li>COMMAND_ID_DARK
 * <li>COMMAND_ID_EXPOSURE
 * </ul>
 */
enum COMMAND_ID
{
	COMMAND_ID_NONE=0,COMMAND_ID_BIAS,COMMAND_ID_DARK,COMMAND_ID_EXPOSURE
};

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_exposure.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_NONE;
/**
 * The pathname of the device to contact.
 */
static char Device_Pathname[256] = "";
/**
 * What type of board initialisation to do for the PCI board.
 * Initialised to CCD_SETUP_LOAD_ROM, so will load PCI program from ROM by default.
 * @see ../cdocs/ccd_setup.html#CCD_SETUP_LOAD_TYPE
 */
static enum CCD_SETUP_LOAD_TYPE PCI_Load_Type = CCD_SETUP_LOAD_ROM;
/**
 * The filename of the PCI .lod file to download.
 */
static char *PCI_Filename = NULL;
/**
 * What type of board initialisation to do for the timing board.
 * Initialised to CCD_SETUP_LOAD_ROM, so will load timing program from ROM by default.
 * @see ../cdocs/ccd_setup.html#CCD_SETUP_LOAD_TYPE
 */
static enum CCD_SETUP_LOAD_TYPE Timing_Load_Type = CCD_SETUP_LOAD_ROM;
/**
 * The filename of the Timing .lod file to download.
 */
static char *Timing_Filename = NULL;
/**
 * What type of board initialisation to do for the utility board.
 * Initialised to CCD_SETUP_LOAD_ROM, so will load utility program from ROM by default.
 * @see ../cdocs/ccd_setup.html#CCD_SETUP_LOAD_TYPE
 */
static enum CCD_SETUP_LOAD_TYPE Utility_Load_Type = CCD_SETUP_LOAD_ROM;
/**
 * The filename of the utility .lod file to download.
 */
static char *Utility_Filename = NULL;
/**
 * Temperature to set the CCD to.
 * @see #DEFAULT_TEMPERATURE
 */
static double Temperature = DEFAULT_TEMPERATURE;
/**
 * Whether to call CCD_Setup_Startup.
 */
static int Do_Setup_Startup = FALSE;
/**
 * Whether the CCD_Exposure_Expose should call a CLR (Clear) command.
 * Defaults to TRUE.
 */
static int Do_Clear_Array = TRUE;
/**
 * The number of columns in the CCD.
 * @see #DEFAULT_SIZE_X
 */
static int Size_X = DEFAULT_SIZE_X;
/**
 * The number of rows in the CCD.
 * @see #DEFAULT_SIZE_Y
 */
static int Size_Y = DEFAULT_SIZE_Y;
/**
 * The number binning factor in columns.
 */
static int Bin_X = 1;
/**
 * The number binning factor in rows.
 */
static int Bin_Y = 1;
/**
 * The de-interlace type to use.
 */
static enum CCD_DSP_DEINTERLACE_TYPE DeInterlace_Type = DEFAULT_DEINTERLACE_TYPE;
/**
 * The amplifier to use.
 */
static enum CCD_DSP_AMPLIFIER Amplifier = DEFAULT_AMPLIFIER;
/**
 * Window flags specifying which window to use.
 */
static int Window_Flags = 0;
/**
 * Window data.
 */
static struct CCD_Setup_Window_Struct Window_List[CCD_SETUP_WINDOW_COUNT];
/**
 * Which type ef exposure command to call.
 * @see #COMMAND_ID
 */
static enum COMMAND_ID Command = COMMAND_ID_NONE;
/**
 * If doing a dark or exposure, the exposure length.
 */
static int Exposure_Length = 0;
/**
 * Filename to store resultant fits image in. Unwindowed only - windowed get more complex.
 */
static char *Filename = NULL;
/**
 * Log level to use.
 */
static int Log_Filter_Level = LOG_VERBOSITY_VERY_VERBOSE;
/**
 * Gain to use in CCD_Setup_Startup.
 */
static int Gain = CCD_DSP_GAIN_ONE;
/**
 * Gain Speed (integrator speed) to use in CCD_Setup_Startup.
 * A boolean, TRUE is fast and FALSE is slow.
 */
static int Gain_Speed = TRUE;

/* internal routines */
static int Parse_Arguments(int argc, char *argv[]);
static void Help(void);
static int Test_Save_Fits_Headers(int exposure_time,int ncols,int nrows,char *filename);
static void Test_Fits_Header_Error(int status);

/**
 * Main program.
 * @param argc The number of arguments to the program.
 * @param argv An array of argument strings.
 * @return This function returns 0 if the program succeeds, and a positive integer if it fails.
 * @see #Text_Print_Level
 * @see #Interface_Device
 * @see #Do_Setup_Startup
 * @see #Device_Pathname
 * @see #PCI_Load_Type
 * @see #PCI_Filename
 * @see #Timing_Load_Type
 * @see #Timing_Filename
 * @see #Utility_Load_Type
 * @see #Utility_Filename
 * @see #Temperature
 * @see #Gain
 * @see #Gain_Speed
 * @see #Size_X
 * @see #Size_Y
 * @see #Bin_X
 * @see #Bin_Y
 * @see #Amplifier
 * @see #DeInterlace_Type
 * @see #Window_Flags
 * @see #Window_List
 * @see #Command
 * @see #Exposure_Length
 * @see #Filename
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	struct timespec start_time;
	char buff[256];
	char command_buffer[256];
	char *filename_list[CCD_SETUP_WINDOW_COUNT];
	int filename_count,i;
	int retval;
	int value,bit_value;

/* parse arguments */
	fprintf(stdout,"Parsing Arguments.\n");
	if(!Parse_Arguments(argc,argv))
		return 1;
/* set text/interface options */
	CCD_Text_Set_Print_Level(Text_Print_Level);
	fprintf(stdout,"Initialise Controller:Using device %d.\n",Interface_Device);
	CCD_Global_Initialise();
	CCD_Global_Set_Log_Handler_Function(CCD_Global_Log_Handler_Stdout);
	CCD_Global_Set_Log_Filter_Level(Log_Filter_Level);
/* open SDSU connection */
	fprintf(stdout,"Opening SDSU device.\n");
	if(strlen(Device_Pathname) == 0)
	{
		fprintf(stdout,"Selecting default device path.\n");
		switch(Interface_Device)
		{
			case CCD_INTERFACE_DEVICE_PCI:
				strcpy(Device_Pathname,CCD_PCI_DEFAULT_DEVICE_ZERO);
				break;
			case CCD_INTERFACE_DEVICE_TEXT:
				strcpy(Device_Pathname,"frodospec_ccd_text_exposure.txt");
				break;
			default:
				fprintf(stderr,"Illegal interface device %d.\n",Interface_Device);
				break;
		}
	}
	retval = CCD_Interface_Open(Interface_Device,Device_Pathname,&handle);
	if(retval == FALSE)
	{
		CCD_Global_Error();
		return 2;
	}
	fprintf(stdout,"SDSU device opened.\n");
/* call CCD_Setup_Startup */
	if(Do_Setup_Startup)
	{
		fprintf(stdout,"Calling CCD_Setup_Startup:\n");
		if(PCI_Filename != NULL)
			fprintf(stdout,"PCI Type:%d:Filename:%s\n",PCI_Load_Type,PCI_Filename);
		else
			fprintf(stdout,"PCI Type:%d:Filename:NULL\n",PCI_Load_Type);
		if(Timing_Filename != NULL)
			fprintf(stdout,"Timing Type:%d:Filename:%s\n",Timing_Load_Type,Timing_Filename);
		else
			fprintf(stdout,"Timing Type:%d:Filename:NULL\n",Timing_Load_Type);
		if(Utility_Filename != NULL)
			fprintf(stdout,"Utility Type:%d:Filename:%s\n",Utility_Load_Type,Utility_Filename);
		else
			fprintf(stdout,"Utility Type:%d:Filename:NULL\n",Utility_Load_Type);
		fprintf(stdout,"Gain:%d : Gain_Speed:%d\n",Gain,Gain_Speed);
		fprintf(stdout,"Temperature:%.2f\n",Temperature);
		if(!CCD_Setup_Startup(handle,PCI_Load_Type,PCI_Filename,Timing_Load_Type,0,Timing_Filename,
				      Utility_Load_Type,0,Utility_Filename,Temperature,Gain,Gain_Speed,TRUE))
		{
			CCD_Global_Error();
			return 3;
		}
		fprintf(stdout,"CCD_Setup_Startup completed\n");
	}
	else
	{
		fprintf(stdout,"CCD_Setup_Startup was NOT called.\n");
		fprintf(stdout,"So we need to call CCD_Interface_Memory_Map to see the readout data.\n");
		if(!CCD_Interface_Memory_Map(handle,CCD_SETUP_MEMORY_BUFFER_SIZE))
		{
			CCD_Global_Error();
			return 4;
		}
	}
/* call CCD_Setup_Dimensions */
	fprintf(stdout,"Calling CCD_Setup_Dimensions:\n");
	fprintf(stdout,"Chip Size:(%d,%d)\n",Size_X,Size_Y);
	fprintf(stdout,"Binning:(%d,%d)\n",Bin_X,Bin_Y);
	fprintf(stdout,"Amplifier:%#x(%s):De-Interlace:%d(%s)\n",Amplifier,CCD_DSP_Command_Manual_To_String(Amplifier),
		DeInterlace_Type,CCD_DSP_Print_DeInterlace(DeInterlace_Type));
	fprintf(stdout,"Window Flags:%d\n",Window_Flags);
	if(!CCD_Setup_Dimensions(handle,Size_X,Size_Y,Bin_X,Bin_Y,Amplifier,DeInterlace_Type,Window_Flags,Window_List))
	{
		CCD_Global_Error();
		return 3;
	}
	fprintf(stdout,"CCD_Setup_Dimensions completed\n");
/* save fits headers */
	if(Window_Flags > 0 )
	{
		filename_count = 0;
		if(strchr(Filename,'.') != NULL)
			Filename[strchr(Filename,'.')-Filename] = '\0';
		for(i=0;i<CCD_SETUP_WINDOW_COUNT;i++)
		{
			/* relies on values in CCD_SETUP_WINDOW_ONE... */
			if(Window_Flags & (1<<i))
			{
				sprintf(buff,"%sw%d.fits",Filename,i);
				filename_list[filename_count] = strdup(buff);
				/* binning? */
				if(!Test_Save_Fits_Headers(Exposure_Length,CCD_Setup_Get_Window_Width(handle,i),
							   CCD_Setup_Get_Window_Height(handle,i),
							   filename_list[filename_count]))
				{
					fprintf(stdout,"Saving FITS window headers (%s,%d,%d) failed.\n",
						filename_list[filename_count],CCD_Setup_Get_Window_Width(handle,i),
						CCD_Setup_Get_Window_Height(handle,i));
					return 4;
				}
				filename_count++;
			}/* end if window active */
		}/* end for */
	}
	else
	{
		filename_list[0] = Filename;
		filename_count = 1;
		if(!Test_Save_Fits_Headers(Exposure_Length,Size_X/Bin_X,Size_Y/Bin_Y,Filename))
		{
			fprintf(stdout,"Saving FITS headers failed.\n");
			return 4;
		}
	}
/* do command */
	start_time.tv_sec = 0;
	start_time.tv_nsec = 0;
	switch(Command)
	{
		case COMMAND_ID_BIAS:
			fprintf(stdout,"Calling CCD_Exposure_Bias.\n");
			retval = CCD_Exposure_Bias(handle,Filename);
			break;
		case COMMAND_ID_DARK:
			fprintf(stdout,"Calling CCD_Exposure_Expose with open_shutter FALSE.\n");
			retval = CCD_Exposure_Expose(handle,Do_Clear_Array,FALSE,start_time,Exposure_Length,
						     filename_list,filename_count);
			break;
		case COMMAND_ID_EXPOSURE:
			fprintf(stdout,"Calling CCD_Exposure_Expose with open_shutter TRUE.\n");
			retval = CCD_Exposure_Expose(handle,Do_Clear_Array,TRUE,start_time,Exposure_Length,
						     filename_list,filename_count);
			break;
		case COMMAND_ID_NONE:
			fprintf(stdout,"Please select a command to execute (-bias | -dark | -expose).\n");
			Help();
			return 5;
	}
	if(retval == FALSE)
	{
		CCD_Global_Error();
		sprintf(command_buffer,"rm %s",Filename);
		system(command_buffer);
		return 6;
	}
	fprintf(stdout,"Command Completed.\n");
/* close interface to SDSU controller */
	fprintf(stdout,"CCD_Interface_Close\n");
	CCD_Interface_Close(&handle);
	fprintf(stdout,"CCD_Interface_Close completed.\n");
	return 0;
}

/**
 * Routine to parse command line arguments.
 * @param argc The number of arguments sent to the program.
 * @param argv An array of argument strings.
 * @see #Help
 * @see #Text_Print_Level
 * @see #Interface_Device
 * @see #Do_Setup_Startup
 * @see #Do_Clear_Array
 * @see #Device_Pathname
 * @see #PCI_Load_Type
 * @see #PCI_Filename
 * @see #Timing_Load_Type
 * @see #Timing_Filename
 * @see #Utility_Load_Type
 * @see #Utility_Filename
 * @see #Temperature
 * @see #Gain
 * @see #Gain_Speed
 * @see #Size_X
 * @see #Size_Y
 * @see #Bin_X
 * @see #Bin_Y
 * @see #Amplifier
 * @see #DeInterlace_Type
 * @see #Window_Flags
 * @see #Window_List
 * @see #Command
 * @see #Exposure_Length
 * @see #Filename
 * @see #Log_Filter_Level
 */
static int Parse_Arguments(int argc, char *argv[])
{
	struct CCD_Setup_Window_Struct window;
	int i,retval,window_number;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-amplifier")==0)||(strcmp(argv[i],"-a")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"bottomleft")==0)
				{
					Amplifier = CCD_DSP_AMPLIFIER_BOTTOM_LEFT;
					DeInterlace_Type = CCD_DSP_DEINTERLACE_SINGLE;
				}
				else if(strcmp(argv[i+1],"bottomright")==0)
				{
					Amplifier = CCD_DSP_AMPLIFIER_BOTTOM_RIGHT;
					DeInterlace_Type = CCD_DSP_DEINTERLACE_FLIP_X;
				}
				else if(strcmp(argv[i+1],"topleft")==0)
				{
					Amplifier = CCD_DSP_AMPLIFIER_TOP_LEFT;
					DeInterlace_Type = CCD_DSP_DEINTERLACE_FLIP_Y;
				}
				else if(strcmp(argv[i+1],"topright")==0)
				{
					Amplifier = CCD_DSP_AMPLIFIER_TOP_RIGHT;
					DeInterlace_Type = CCD_DSP_DEINTERLACE_FLIP_XY;
				}
				else if(strcmp(argv[i+1],"all")==0)
				{
					Amplifier = CCD_DSP_AMPLIFIER_ALL;
					DeInterlace_Type = CCD_DSP_DEINTERLACE_SPLIT_QUAD;
				}
				else
				{
					fprintf(stderr,"Parse_Arguments:Illegal Amplifier '%s', "
						"<bottomleft|bottomright|topleft|topright|all> required.\n",argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Amplifier requires <left|right|both>.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-bias")==0)||(strcmp(argv[i],"-b")==0))
		{
			Command = COMMAND_ID_BIAS;
		}
		else if((strcmp(argv[i],"-dark")==0)||(strcmp(argv[i],"-d")==0))
		{
			Command = COMMAND_ID_DARK;
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Exposure_Length);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing exposure length %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:dark requires exposure length.\n");
				return FALSE;
			}

		}
		else if((strcmp(argv[i],"-device_pathname")==0))
		{
			if((i+1)<argc)
			{
				strncpy(Device_Pathname,argv[i+1],255);
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Device Pathname requires a device.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-expose")==0)||(strcmp(argv[i],"-e")==0))
		{
			Command = COMMAND_ID_EXPOSURE;
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Exposure_Length);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing exposure length %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:exposure requires exposure length.\n");
				return FALSE;
			}

		}
		else if((strcmp(argv[i],"-filename")==0)||(strcmp(argv[i],"-f")==0))
		{
			if((i+1)<argc)
			{
				Filename = argv[i+1];
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:filename required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-interface_device")==0)||(strcmp(argv[i],"-i")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"text")==0)
					Interface_Device = CCD_INTERFACE_DEVICE_TEXT;
				else if(strcmp(argv[i+1],"pci")==0)
					Interface_Device = CCD_INTERFACE_DEVICE_PCI;
				else
				{
					fprintf(stderr,"Parse_Arguments:Illegal Interface Device '%s'.\n",argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Interface Device requires a device.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-gain")==0)||(strcmp(argv[i],"-g")==0))
		{
			if((i+2)<argc)
			{
				if(strcmp(argv[i+1],"1")==0)
				{
					Gain = CCD_DSP_GAIN_ONE;
				}
				else if(strcmp(argv[i+1],"2")==0)
				{
					Gain = CCD_DSP_GAIN_TWO;
				}
				else if(strcmp(argv[i+1],"4")==0)
				{
					Gain = CCD_DSP_GAIN_FOUR;
				}
				else if(strcmp(argv[i+1],"9")==0)
				{
					Gain = CCD_DSP_GAIN_NINE;
				}
				else
				{
					fprintf(stderr,"Parse_Arguments:Illegal Gain '%s' (should be 1|2|4|9).\n",
						argv[i+1]);
					return FALSE;
				}
				if(strcmp(argv[i+2],"true")==0)
				{
					Gain_Speed = TRUE;
				}
				else if(strcmp(argv[i+2],"false")==0)
				{
					Gain_Speed = FALSE;
				}
				else
				{
					fprintf(stderr,
						"Parse_Arguments:Illegal Gain_Speed '%s' (should be true|false).\n",
						argv[i+1]);
					return FALSE;
				}
				i += 2;
			}
		}
		else if((strcmp(argv[i],"-help")==0)||(strcmp(argv[i],"-h")==0))
		{
			Help();
			exit(0);
		}
		else if(strcmp(argv[i],"-log_level")==0)
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Log_Filter_Level);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing Log Level %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Log Level required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-noclear")==0)||(strcmp(argv[i],"-nc")==0))
		{
			Do_Clear_Array = FALSE;
		}
		else if(strcmp(argv[i],"-pci_filename")==0)
		{
			if((i+1)<argc)
			{
				PCI_Load_Type = CCD_SETUP_LOAD_FILENAME;
				PCI_Filename = argv[i+1];
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:PCI Filename required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-setup")==0)||(strcmp(argv[i],"-s")==0))
		{
			Do_Setup_Startup = TRUE;
		}
		else if(strcmp(argv[i],"-temperature")==0)
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%lf",&Temperature);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing temperature %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:temperature required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-text_print_level")==0)||(strcmp(argv[i],"-t")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"commands")==0)
					Text_Print_Level = CCD_TEXT_PRINT_LEVEL_COMMANDS;
				else if(strcmp(argv[i+1],"replies")==0)
					Text_Print_Level = CCD_TEXT_PRINT_LEVEL_REPLIES;
				else if(strcmp(argv[i+1],"values")==0)
					Text_Print_Level = CCD_TEXT_PRINT_LEVEL_VALUES;
				else if(strcmp(argv[i+1],"all")==0)
					Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
				else
				{
					fprintf(stderr,"Parse_Arguments:Illegal Text Print Level '%s'.\n",argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Text Print Level requires a level.\n");
				return FALSE;
			}
		}
		else if(strcmp(argv[i],"-timing_filename")==0)
		{
			if((i+1)<argc)
			{
				Timing_Load_Type = CCD_SETUP_LOAD_FILENAME;
				Timing_Filename = argv[i+1];
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Timing Filename required.\n");
				return FALSE;
			}
		}
		else if(strcmp(argv[i],"-utility_filename")==0)
		{
			if((i+1)<argc)
			{
				Utility_Load_Type = CCD_SETUP_LOAD_FILENAME;
				Utility_Filename = argv[i+1];
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Utility Filename required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-window")==0)||(strcmp(argv[i],"-w")==0))
		{
			if((i+5)<argc)
			{
				/* window number */
				retval = sscanf(argv[i+1],"%d",&window_number);
				if(retval != 1)
				{
					fprintf(stderr,"Parse_Arguments:Parsing Window Number failed.\n",argv[i+1]);
					return FALSE;
				}
				switch(window_number)
				{
				case 1:
					Window_Flags |= CCD_SETUP_WINDOW_ONE;
					break;
				case 2:
					Window_Flags |= CCD_SETUP_WINDOW_TWO;
					break;
				case 3:
					Window_Flags |= CCD_SETUP_WINDOW_THREE;
					break;
				case 4:
					Window_Flags |= CCD_SETUP_WINDOW_FOUR;
					break;
				default:
					fprintf(stderr,"Parse_Arguments:Window Number %d out of range(1..4).\n",
						window_number);
					return FALSE;
				}
				/* x start */
				retval = sscanf(argv[i+2],"%d",&(window.X_Start));
				if(retval != 1)
				{
					fprintf(stderr,"Parse_Arguments:Parsing Window Start X (%s) failed.\n",
						argv[i+2]);
					return FALSE;
				}
				/* y start */
				retval = sscanf(argv[i+3],"%d",&(window.Y_Start));
				if(retval != 1)
				{
					fprintf(stderr,"Parse_Arguments:Parsing Window Start Y (%s) failed.\n",
						argv[i+3]);
					return FALSE;
				}
				/* x end */
				retval = sscanf(argv[i+4],"%d",&(window.X_End));
				if(retval != 1)
				{
					fprintf(stderr,"Parse_Arguments:Parsing Window End X (%s) failed.\n",
						argv[i+4]);
					return FALSE;
				}
				/* y end */
				retval = sscanf(argv[i+5],"%d",&(window.Y_End));
				if(retval != 1)
				{
					fprintf(stderr,"Parse_Arguments:Parsing Window End Y (%s) failed.\n",
						argv[i+5]);
					return FALSE;
				}
				/* put window into correct list index 1..4 -> 0..3 */
				Window_List[window_number-1] = window;
				i+= 5;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:-window requires 5 argument:%d supplied.\n",argc-i);
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-xsize")==0)||(strcmp(argv[i],"-xs")==0))
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Size_X);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing X Size %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:size required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-ysize")==0)||(strcmp(argv[i],"-ys")==0))
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Size_Y);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing Y Size %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:size required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-xbin")==0)||(strcmp(argv[i],"-xb")==0))
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Bin_X);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing X Bin %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:bin required.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-ybin")==0)||(strcmp(argv[i],"-yb")==0))
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Bin_Y);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing Y Bin %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:bin required.\n");
				return FALSE;
			}
		}
		else
		{
			fprintf(stderr,"Parse_Arguments:argument '%s' not recognized.\n",argv[i]);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Help routine.
 */
static void Help(void)
{
	fprintf(stdout,"Test Exposure:Help.\n");
	fprintf(stdout,"This program calls CCD_Setup_Dimensions to set up the SDSU controller dimensions.\n");
	fprintf(stdout,"It then calls either CCD_Exposure_Bias or CCD_Exposure_Expose to perform an exposure.\n");
	fprintf(stdout,"test_exposure [-i[nterface_device] <pci|text>]\n");
	fprintf(stdout,"\t[-device_pathname <path>][-setup]\n");
	fprintf(stdout,"\t[-pci_filename <filename>][-timing_filename <filename>][-utility_filename <filename>]\n");
	fprintf(stdout,"\t[-temperature <temperature>]\n");
	fprintf(stdout,"\t[-g[ain] <1|2|4|9> <true|false>]\n");
	fprintf(stdout,"\t[-xs[ize] <no. of pixels>][-ys[ize] <no. of pixels>]\n");
	fprintf(stdout,"\t[-xb[in] <binning factor>][-yb[in] <binning factor>]\n");
	fprintf(stdout,"\t[-a[mplifier] <bottomleft|bottomright|topleft|topright|all>]\n");
	fprintf(stdout,"\t[-w[indow] <no> <xstart> <ystart> <xend> <yend>]\n");
	fprintf(stdout,"\t[-f[ilename] <filename>][-noclear|-nc]\n");
	fprintf(stdout,"\t[-b[ias]][-d[ark] <exposure length>][-e[xpose] <exposure length>]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\t[-log_level <bit number>]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-device_pathname can select which controller (/dev/astropci[0|1]) or text output file.\n");
	fprintf(stdout,"\t-gain takes a value and a speed (true for fast, false for slow).\n");
	fprintf(stdout,"\t-setup calls CCD_Setup_Startup, which needs the "
		"-pci_filename|-timing_filename|-utility_filename|-temperature|-gain arguments.\n");
	fprintf(stdout,"\t-noclear does NOT clea the array before starting an exposure.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<filename> should be a valid .lod file pathname.\n");
	fprintf(stdout,"\t<temperature> should be a valid double, a temperature in degrees Celcius.\n");
	fprintf(stdout,"\t<exposure length> is a positive integer in milliseconds.\n");
	fprintf(stdout,"\t<no. of pixels> and <binning factor> is a positive integer.\n");
	fprintf(stdout,"\t<log level> is the verbosity between 0 and 5 inclusive.\n");
}

/**
 * Internal routine that saves some basic FITS headers to the relevant filename.
 * This is needed as CCD_Exposure_Expose/Bias routines need saved FITS headers to
 * not give an error.
 * @param exposure_time The amount of time, in milliseconds, of the exposure.
 * @param ncols The number of columns in the FITS file.
 * @param nrows The number of rows in the FITS file.
 * @param filename The filename to save the FITS headers in.
 */
static int Test_Save_Fits_Headers(int exposure_time,int ncols,int nrows,char *filename)
{
	static fitsfile *fits_fp = NULL;
	int status = 0,retval,ivalue;
	double dvalue;

/* open file */
	if(fits_create_file(&fits_fp,filename,&status))
	{
		Test_Fits_Header_Error(status);
		return FALSE;
	}
/* SIMPLE keyword */
	ivalue = TRUE;
	retval = fits_update_key(fits_fp,TLOGICAL,(char*)"SIMPLE",&ivalue,NULL,&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* BITPIX keyword */
	ivalue = 16;
	retval = fits_update_key(fits_fp,TINT,(char*)"BITPIX",&ivalue,NULL,&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* NAXIS keyword */
	ivalue = 2;
	retval = fits_update_key(fits_fp,TINT,(char*)"NAXIS",&ivalue,NULL,&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* NAXIS1 keyword */
	ivalue = ncols;
	retval = fits_update_key(fits_fp,TINT,(char*)"NAXIS1",&ivalue,NULL,&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* NAXIS2 keyword */
	ivalue = nrows;
	retval = fits_update_key(fits_fp,TINT,(char*)"NAXIS2",&ivalue,NULL,&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* BZERO keyword */
	dvalue = 32768.0;
	retval = fits_update_key_fixdbl(fits_fp,(char*)"BZERO",dvalue,6,
		(char*)"Number to offset data values by",&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* BSCALE keyword */
	dvalue = 1.0;
	retval = fits_update_key_fixdbl(fits_fp,(char*)"BSCALE",dvalue,6,
		(char*)"Number to multiply data values by",&status);
	if(retval != 0)
	{
		Test_Fits_Header_Error(status);
		fits_close_file(fits_fp,&status);
		return FALSE;
	}
/* close file */
	if(fits_close_file(fits_fp,&status))
	{
		Test_Fits_Header_Error(status);
		return FALSE;
	}
	return TRUE;
}

/**
 * Internal routine to write the complete CFITSIO error stack to stderr.
 * @param status The status returned by CFITSIO.
 */
static void Test_Fits_Header_Error(int status)
{
	/* report the whole CFITSIO error message stack to stderr. */
	fits_report_error(stderr, status);
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.6  2009/02/05 11:40:55  cjm
** Swapped Bitwise for Absolute logging levels.
**
** Revision 1.5  2008/11/20 11:34:58  cjm
** *** empty log message ***
**
** Revision 1.4  2006/05/16 18:18:25  cjm
** gnuify: Added GNU General Public License.
**
** Revision 1.3  2004/11/04 16:03:05  cjm
** Added DeInterlace_Type = CCD_DSP_DEINTERLACE_FLIP for right amplifier.
**
** Revision 1.2  2003/03/26 15:51:55  cjm
** Changed for windowing API change.
**
** Revision 1.1  2002/11/07 19:18:22  cjm
** Initial revision
**
*/
