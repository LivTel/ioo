/* test_setup_startup.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_generate_waveform.c,v 1.3 2011-02-07 17:07:52 cjm Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "estar_common.h"
#include "estar_config.h"
#include "ccd_dsp.h"
#include "ccd_dsp_download.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_temperature.h"
#include "ccd_setup.h"

/**
 * This program tests generation of the serial waveform using the GWF command.
 * It can optionally call CCD_Setup_Startup, which does initial configuration of the SDSU controller.
 * The generated waveform can then be read from PXL_TBL and debugged.
 * <pre>
 * test_setup_startup -i[nterface_device] &lt;pci|text&gt; -setup -pci_filename &lt;filename&gt; 
 * 	-timing_filename &lt;filename&gt; -utility_filename &lt;filename&gt; -temperature &lt;temperature&gt;
 * 	[-xb[in] &lt;binning factor&gt;][-yb[in] &lt;binning factor&gt;]
 * 	[-a[mplifier] &lt;bottomleft|bottomright|topleft|topright|all&gt;]
 * 	-t[ext_print_level] &lt;commands|replies|values|all&gt; -h[elp]
 * 	[-pta|-pixel_table_address <address>]
 * </pre>
 * @author $Author: cjm $
 * @version $Revision: 1.3 $
 */
/* hash definitions */
/**
 * Maximum length of some of the strings in this program.
 */
#define MAX_STRING_LENGTH	(256)
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
/**
 * Default pixel table address. Should be the results of <code>grep PXL_TBL tim.lod</code>.
 * Should be in timing board Y memory space.
 */
#define DEFAULT_PIXEL_TABLE_ADDRESS (0xFB)

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_generate_waveform.c,v 1.3 2011-02-07 17:07:52 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_NONE;
/**
 * Boolean, used to determine whether to call CCD_Setup_Startup to load the DSP code
 * befor calling GWF.
 */
static int Do_Setup_Startup = FALSE;
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
 * Temperature to set the CCD to. Defaults to -100.0 degrees C.
 */
static double Temperature = -110.0;
/**
 * Boolean, used to determine whether to call CCD_Setup_Dimensions to setup amplifier and binning before calling
 * GWF.
 */
static int Do_Setup_Dimensions = FALSE;
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
 * SDSU timing board Y space address of the start of PXL_TBL, the location
 * where GWF dumps the compiled serial clock waveform.
 * @see #DEFAULT_PIXEL_TABLE_ADDRESS
 */
static int Pxl_Tbl_Address = DEFAULT_PIXEL_TABLE_ADDRESS;
/**
 * The last value written to the video processor board. Needed to determine
 * state transitions (i.e. low -> high bit transitions means something).
 * Should be initialised to video value just before readout starts.
 * %0011011 (27/0x1b) is last SERIAL_IDLE video value (bif486.waveforms).
 * %0011000 (24/0x18) is last SERIALS_EXPOSE video value (bif486.waveforms).
 */
static int Last_Video_Value = 0x18;
/**
 * Filename containing clock line configuration information.
 */
static char *Clock_Config_Filename = NULL;
/**
 * The clock line configuration data.
 */
static eSTAR_Config_Properties_t Clock_Config_Data;

/* internal routines */
static int Read_PXL_TBL(CCD_Interface_Handle_T *handle,int pxl_tbl_address);
static int Decode_PXL_TBL_Value(int value);
static int Load_Clock_Config(void);
static int Print_Clock_Value(char *clock_bank_name,int clock_value);
static int Parse_Arguments(int argc, char *argv[]);
static void Help(void);

/**
 * Main program.
 * @param argc The number of arguments to the program.
 * @param argv An array of argument strings.
 * @return This function returns 0 if the program succeeds, and a positive integer if it fails.
 * @see #Text_Print_Level
 * @see #Interface_Device
 * @see #PCI_Load_Type
 * @see #PCI_Filename
 * @see #Timing_Load_Type
 * @see #Timing_Filename
 * @see #Utility_Load_Type
 * @see #Utility_Filename
 * @see #Temperature
 * @see #Size_X
 * @see #Size_Y
 * @see #Bin_X
 * @see #Bin_Y
 * @see #Amplifier
 * @see #DeInterlace_Type
 * @see #Window_Flags
 * @see #Window_List
 * @see #Pxl_Tbl_Address
 * @see #Load_Clock_Config
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	char device_pathname[256];
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
/* open SDSU connection */
	fprintf(stdout,"Opening SDSU device.\n");
	switch(Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_PCI:
			strcpy(device_pathname,CCD_PCI_DEFAULT_DEVICE_ZERO);
			break;
		case CCD_INTERFACE_DEVICE_TEXT:
			strcpy(device_pathname,"frodospec_ccd_test_setup_startup.txt");
			break;
		default:
			fprintf(stderr,"Illegal interface device %d.\n",Interface_Device);
			break;
	}
	retval = CCD_Interface_Open(Interface_Device,device_pathname,&handle);
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
		fprintf(stdout,"Temperature:%.2f\n",Temperature);
		if(!CCD_Setup_Startup(handle,PCI_Load_Type,PCI_Filename,Timing_Load_Type,0,Timing_Filename,
				      Utility_Load_Type,0,Utility_Filename,Temperature,CCD_DSP_GAIN_ONE,TRUE,TRUE))
		{
			CCD_Global_Error();
			return 3;
		}
		fprintf(stdout,"CCD_Setup_Startup completed.\n");
	} /* end if Do_Setup_Startup */
	else
		fprintf(stdout,"CCD_Setup_Startup was NOT called.\n");
	if(Do_Setup_Dimensions)
	{
		/* call CCD_Setup_Dimensions */
		fprintf(stdout,"Calling CCD_Setup_Dimensions:\n");
		fprintf(stdout,"Chip Size:(%d,%d)\n",Size_X,Size_Y);
		fprintf(stdout,"Binning:(%d,%d)\n",Bin_X,Bin_Y);
		fprintf(stdout,"Amplifier:%d:De-Interlace:%d\n",Amplifier,DeInterlace_Type);
		fprintf(stdout,"Window Flags:%d\n",Window_Flags);
		if(!CCD_Setup_Dimensions(handle,Size_X,Size_Y,Bin_X,Bin_Y,Amplifier,DeInterlace_Type,
					 Window_Flags,Window_List))
		{
			CCD_Global_Error();
			return 5;
		}
		fprintf(stdout,"CCD_Setup_Dimensions completed\n");
	}
	else
		fprintf(stdout,"CCD_Setup_Dimensions was NOT called.\n");
/* call generate serial waveform */
	fprintf(stdout,"Calling GWF:\n");
	if(!CCD_DSP_Command_GWF(handle))
	{
		CCD_Global_Error();
		return 4;
	}
	fprintf(stdout,"Calling GWF completed.\n");
	/* Load clock config */
	Load_Clock_Config();
	/* read PXL_TBL */
	if(!Read_PXL_TBL(handle,Pxl_Tbl_Address))
	{
		return 5;
	}
/* close interface to SDSU controller */
	fprintf(stdout,"CCD_Interface_Close\n");
	CCD_Interface_Close(&handle);
	fprintf(stdout,"CCD_Interface_Close completed.\n");
	return 0;
}

/**
 * Routine to read the PXL_TBL data from the SDSU board.
 * @param handle The handle to talk to the SDSU controller.
 * @param pxl_tbl_address The address of the pixel table.
 * @return The routine returns TRUE on success and FALSE on failure.
 * @see #Decode_PXL_TBL_Value
 */
static int Read_PXL_TBL(CCD_Interface_Handle_T *handle,int pxl_tbl_address)
{
	int waveform_word_count,retval,i,address,value;

	/* read number of words in waveform at PXL_TBL */
	fprintf(stdout,"Reading number of words in waveform from PXL_TBL at address %#x.\n",pxl_tbl_address);
	retval = CCD_DSP_Command_RDM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,pxl_tbl_address);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		CCD_Global_Error();
		return FALSE;
	}
	waveform_word_count = retval;
	if((waveform_word_count < 0) || (waveform_word_count > 1024))
	{
		fprintf(stderr,"Waveform word count %d out of expected range 0..1024.\n");
		return FALSE;
	}
	fprintf(stdout,"Number of words in waveform:%d.\n",waveform_word_count);
	/* first entry in pxl_tbl is number of waveforms.
	** actual waveforms start at pxl_tbl +1 */
	for(i=1;i < (waveform_word_count+1); i ++)
	{
		address = pxl_tbl_address+i;
		retval = CCD_DSP_Command_RDM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,address);
		if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
		{
			CCD_Global_Error();
			return FALSE;
		}
		value = retval;
		fprintf(stdout,"Waveform Index %d, Value %#x:",i,value);
		if(!Decode_PXL_TBL_Value(value))
			return FALSE;
		fprintf(stdout,"\n");
	}/* end for on i (index into waveform) */
	return TRUE;
}

/**
 * Print out the meaning of the PXL_TBL value.
 * @param value The value.
 * @return The routine returns TRUE on success and FALSE on failure. 
 * @see #Last_Video_Value
 */
static int Decode_PXL_TBL_Value(int value)
{
	int byte_list[3];
	int delay,board,start_ad,end_ad,clock_value,video_value,amplifier_circuit,i;

	/* convert to bytes */
	byte_list[0] = value & 0xFF;
	byte_list[1] = (value>>8) & 0xFF;
	byte_list[2] = (value>>16) & 0xFF;
	/* delay bits */
	delay = 0;
	if(((value>>23)&0x1) == 1)/* bit 23 set? */
	{
		/* bits 22-16 specify 640 nsec delay */
		delay = (byte_list[2]&0x7F) * 640;
	}
	else
	{
		/* bits 22-16 specify 40 nsec delay */
		delay = (byte_list[2]&0x7F) * 40;
	}
	/* get board settings  - bits 15-12*/
	board = (value >> 12)&0xF;
	fprintf(stdout,"Board=%d:",board);
	if(board == 0xF)/* SXMIT */
	{
		start_ad = value & 0x3f;/* bits 5-0 are start A/D */
		end_ad = (value>>6) & 0x3f; /* bits 6-10 are end A/D */
		fprintf(stdout,"Delay %d nsec:SXMIT %d to %d",delay,start_ad,end_ad);
	}
	else if(board == 0)/* video */
	{
		video_value = (value&0x7f); /* bits 6-0 are video values */
		fprintf(stdout,"Delay %d nsec:video value %d:",delay,video_value);
		/* display bitwise */
		fprintf(stdout,"%");
		for(i=6;i > -1; i--)
		{
			if(((video_value>>i) & 0x1) == 1)
				fprintf(stdout,"1");
			else
				fprintf(stdout,"0");
		}
		fprintf(stdout,":");
		/* display explanation as text */
		if((video_value & 0x1) == 0x0)
		{
			fprintf(stdout,"Reset the integrator:");
		}
		if(((video_value>>1) & 0x1) == 0x0)
		{
			fprintf(stdout,"Dcc clamp (clamp video signal to ground):");
		}
		amplifier_circuit = ((video_value>>2) & 0x3);
		if(amplifier_circuit == 0x1)
		{
			fprintf(stdout,"Amplifier non-inverting:");
		}
		else if(amplifier_circuit == 0x2)
		{
			fprintf(stdout,"Amplifier inverting:");
		}
		else
			fprintf(stdout,"Unknown amplifier value %d:",amplifier_circuit);
		if(((video_value>>4) & 0x1) == 0x0)
		{
			fprintf(stdout,"Integrate:");
		}
		if((((video_value>>5) & 0x1) == 0x1)&&(((Last_Video_Value>>5) & 0x1) == 0x0))
		{
			fprintf(stdout,"Start A/D sample hold:");
		}
		if((((video_value>>6) & 0x1) == 0x1)&&(((Last_Video_Value>>6) & 0x1) == 0x0))
		{
			fprintf(stdout,"Transfer A/D output to latch:");
		}
		/* keep hold of last value to check for bit transitions */
		Last_Video_Value = video_value;
	}
	else if(board == 0x2)/* clk2 (clock driver lower half) serial */
	{
		clock_value = (value&0xfff);/* bits 11-0 are clock values */
		fprintf(stdout,"Delay %d nsec:",delay);
		if(!Print_Clock_Value("clk2",clock_value))
			return FALSE;
	}
	else if(board == 0x3)/* clk3 (clock driver upper half) parallel */
	{
		clock_value = (value&0xfff);/* bits 11-0 are clock values */
		fprintf(stdout,"Delay %d nsec:",delay);
		if(!Print_Clock_Value("clk3",clock_value))
			return FALSE;
	}
	else
	{
		fprintf(stdout,"Unknown board %d");
		return FALSE;
	}
	return TRUE;
}
/**
 * Print the high/low values of the clock bits.
 * @param clock_bank_name The name of the clock bank, clk2 or clk3.
 * @param clock_value The clock bits.
 * @see #Clock_Config_Data
 */
static int Print_Clock_Value(char *clock_bank_name,int clock_value)
{
	char keyword[16];
	char *tmp_string = NULL;
	char clock_name[16];
	int bit,bit_value,retval;

	fprintf(stdout,"%s:",clock_bank_name);
	/* clock bits 0..11 are the clock values */
	for(bit=0;bit<12;bit++)
	{
		bit_value=(clock_value>>bit)&0x1;
		if(bit_value)
		{
			sprintf(keyword,"%s.%d",clock_bank_name,(1<<bit));
			/*fprintf(stderr,"clock keyword for bit %d = %s\n",bit,keyword);*/
			retval = eSTAR_Config_Get_String(&Clock_Config_Data,keyword,&tmp_string);
			if(retval == ESTAR_TRUE)
			{
				if(tmp_string != NULL)
				{
					strncpy(clock_name,tmp_string,4);
					clock_name[4] = '\0';
					free(tmp_string);
				}
				else
				{
					sprintf(clock_name,"bit%d",bit);
				}
			}
			else
			{
				sprintf(clock_name,"bit%d",bit);
			}
		}
		else
		{
			strcpy(clock_name,"0000");
		}
		fprintf(stdout,"%s,",clock_name);
	}
	return TRUE;
}

/**
 * Load clock config filename.
 * @see #Clock_Config_Filename
 * @see #Clock_Config_Data
 */
static int Load_Clock_Config(void)
{
	if(Clock_Config_Filename != NULL)
	{
		if(!eSTAR_Config_Parse_File(Clock_Config_Filename,&Clock_Config_Data))
		{
			eSTAR_Config_Print_Error();
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Routine to parse command line arguments.
 * @param argc The number of arguments sent to the program.
 * @param argv An array of argument strings.
 * @see #Help
 * @see #Text_Print_Level
 * @see #Interface_Device
 * @see #Do_Setup_Startup
 * @see #PCI_Load_Type
 * @see #PCI_Filename
 * @see #Timing_Load_Type
 * @see #Timing_Filename
 * @see #Utility_Load_Type
 * @see #Utility_Filename
 * @see #Temperature
 * @see #Do_Setup_Dimensions
 * @see #Size_X
 * @see #Size_Y
 * @see #Bin_X
 * @see #Bin_Y
 * @see #Amplifier
 * @see #DeInterlace_Type
 * @see #Window_Flags
 * @see #Window_List
 * @see #Pxl_Tbl_Address
 * @see #Clock_Config_Filename
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

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
		else if((strcmp(argv[i],"-clock_config")==0)||(strcmp(argv[i],"-cc")==0))
		{
			if((i+1)<argc)
			{
				Clock_Config_Filename = argv[i+1];
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Clock Config filename requires a filename.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-dimensions")==0)||(strcmp(argv[i],"-d")==0))
		{
			Do_Setup_Dimensions = TRUE;
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
		else if((strcmp(argv[i],"-help")==0)||(strcmp(argv[i],"-h")==0))
		{
			Help();
			exit(0);
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
 		else if((strcmp(argv[i],"-pixel_table_address")==0)||(strcmp(argv[i],"-pta")==0))
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%i",&Pxl_Tbl_Address);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Parsing Pixel Table Address %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:address required.\n");
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
	fprintf(stdout,"Test Generate Waveform:Help.\n");
	fprintf(stdout,"This program tests the generation of the serial readout waveform using the GWF command.\n");
	fprintf(stdout,"test_generate_waveform [-i[nterface_device] <interface device>]\n");
	fprintf(stdout,"\t[-setup]\n");
	fprintf(stdout,"\t[-pci_filename <filename>][-timing_filename <filename>][-utility_filename <filename>]\n");
	fprintf(stdout,"\t[-temperature <temperature>]\n");
	fprintf(stdout,"\t[-dimensions]\n");
	fprintf(stdout,"\t[-xs[ize] <no. of pixels>][-ys[ize] <no. of pixels>]\n");
	fprintf(stdout,"\t[-xb[in] <binning factor>][-yb[in] <binning factor>]\n");
	fprintf(stdout,"\t[-a[mplifier] <bottomleft|bottomright|topleft|topright|all>]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\t[-pta|-pixel_table_address <address>]\n");
	fprintf(stdout,"\t[-clock_config|-cc <filename> ]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-setup determines whether CCD_Setup_Startup is called to initialise the DSP code.\n");
	fprintf(stdout,"\t\tIf so -pci_filename|-timing_filename|-utility_filename|-temperature are needed.\n");
	fprintf(stdout,"\t-dimensions determines whether CCD_Setup_Dimensions is called to initialise the binning and amplier.\n");
	fprintf(stdout,"\t\tIf so -xsize|-ysize|-xbin|-ybin|-amplifier are needed.\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<filename> should be a valid .lod file pathname.\n");
	fprintf(stdout,"\t<temperature> should be a valid double, a temperature in degrees Celcius.\n");
	fprintf(stdout,"\t<address> should be a valid integer, can be represented in hex.\n");
	fprintf(stdout,"\t\tThe default value is %#x, the correct value can be found by 'grep PXL_TBL tim.lod'.\n",
		DEFAULT_PIXEL_TABLE_ADDRESS);

}

/*
** $Log: not supported by cvs2svn $
** Revision 1.2  2009/10/05 11:07:00  cjm
** Added bitwise video output print.
**
** Revision 1.1  2009/10/02 16:42:50  cjm
** Initial revision
**
*/
