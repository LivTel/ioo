/* test_filter_wheel.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_filter_wheel.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ccd_dsp.h"
#include "ccd_global.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_filter_wheel.h"
#include "ccd_setup.h"

/**
 * This program tests the operation of the filter wheel.
 * @author $Author: cjm $
 * @version $Revision: 1.1 $
 */
/* hash definitions */
/**
 * Maximum length of some of the strings in this program.
 */
#define MAX_STRING_LENGTH	(256)

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_filter_wheel.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * The log level to use.
 */
static int Log_Level = 0;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_PCI;
/**
 * What type of board initialisation to do for the PCI board.
 * Initialised to CCD_SETUP_LOAD_ROM, so will load PCI program from ROM by default.
 * @see ../cdocs/ccd_setup.html#CCD_SETUP_LOAD_TYPE
 */
static enum CCD_SETUP_LOAD_TYPE PCI_Load_Type = CCD_SETUP_LOAD_ROM;
/**
 * A DSP '.lod' file containing the SDSU PCI board DSP program.
 */
static char PCI_Filename[MAX_STRING_LENGTH] = "pci.lod";
/**
 * What type of board initialisation to do for the timing board.
 * Initialised to CCD_SETUP_LOAD_ROM, so will load timing program from ROM by default.
 * @see ../cdocs/ccd_setup.html#CCD_SETUP_LOAD_TYPE
 */
static enum CCD_SETUP_LOAD_TYPE Timing_Load_Type = CCD_SETUP_LOAD_ROM;
/**
 * A DSP '.lod' file containing the SDSU timing board DSP program.
 */
static char Timing_Filename[MAX_STRING_LENGTH] = "tim.lod";
/**
 * What type of board initialisation to do for the utility board.
 * Initialised to CCD_SETUP_LOAD_ROM, so will load utility program from ROM by default.
 * @see ../cdocs/ccd_setup.html#CCD_SETUP_LOAD_TYPE
 */
static enum CCD_SETUP_LOAD_TYPE Utility_Load_Type = CCD_SETUP_LOAD_ROM;
/**
 * A DSP '.lod' file containing the SDSU utility board DSP program.
 */
static char Utility_Filename[MAX_STRING_LENGTH] = "util.lod";
/**
 * A boolean, TRUE if we want to setup the SDSU controller.
 */
static int Setup = TRUE;
/**
 * A boolean, TRUE if we want to Reset the filter wheel.
 */
static int Reset = FALSE;
/**
 * A boolean, TRUE if we want to Move the filter wheel.
 */
static int Move = FALSE;
/**
 * A boolean, TRUE if we want to Abort a filter wheel operation in progress.
 */
static int Abort = FALSE;
/**
 * A number describing the position to move the filter wheel to.
 */
static int Position = 0;

/**
 * A boolean, TRUE if we want to shutdown the controller.
 */
static int Shutdown = TRUE;

/* internal routines */
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
 * @see #Setup
 * @see #Abort
 * @see #Move
 * @see #Reset
 * @see #Position
 * @see #Shutdown
 * @see #Log_Level
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_GAIN
 * @see ../cdocs/ccd_filter_wheel.html#CCD_Filter_Wheel_Abort
 * @see ../cdocs/ccd_filter_wheel.html#CCD_Filter_Wheel_Move
 * @see ../cdocs/ccd_global.html#CCD_Global_Initialise
 * @see ../cdocs/ccd_global.html#CCD_Global_Error
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Handler_Function
 * @see ../cdocs/ccd_global.html#CCD_Global_Log_Handler_Stdout
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Filter_Function
 * @see ../cdocs/ccd_global.html#CCD_Global_Log_Filter_Level_Absolute
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Filter_Level
 * @see ../cdocs/ccd_interface.html#CCD_INTERFACE_DEVICE
 * @see ../cdocs/ccd_interface.html#CCD_Interface_Open
 * @see ../cdocs/ccd_interface.html#CCD_Interface_Close
 * @see ../cdocs/ccd_setup.html#CCD_Setup_Startup
 * @see ../cdocs/ccd_setup.html#CCD_Setup_Shutdown
 * @see ../cdocs/ccd_text.html#CCD_TEXT_PRINT_LEVEL_COMMANDS
 * @see ../cdocs/ccd_text.html#CCD_Text_Set_Print_Level
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	char device_pathname[256];
	int retval;

	fprintf(stdout,"Test filter wheel\n");
	fprintf(stdout,"Parsing Arguments.\n");
	if(!Parse_Arguments(argc,argv))
		return 1;
/* set text/interface options */
	CCD_Text_Set_Print_Level(Text_Print_Level);
	fprintf(stdout,"Initialise Controller:Using device %d.\n",Interface_Device);
	CCD_Global_Initialise();
	CCD_Global_Set_Log_Handler_Function(CCD_Global_Log_Handler_Stdout);
	CCD_Global_Set_Log_Filter_Function(CCD_Global_Log_Filter_Level_Absolute);
	CCD_Global_Set_Log_Filter_Level(Log_Level);

	fprintf(stdout,"Test Filter Wheel:Opening SDSU device.\n");
	fprintf(stdout,"Opening SDSU device.\n");
	fflush(stdout);
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
		return 1;
	}
	fprintf(stdout,"SDSU device opened.\n");
	fflush(stdout);
	if(Setup)
	{
		fprintf(stdout,"Test Filter Wheel:Initialising controller:\n");
		fprintf(stdout,"\tUsing PCI filename:%s\n",PCI_Filename);
		fprintf(stdout,"\tUsing Timing filename:%s\n",Timing_Filename);
		fprintf(stdout,"\tUsing Utility filename:%s\n",Utility_Filename);
		if(!CCD_Setup_Startup(handle,PCI_Load_Type,PCI_Filename,
			Timing_Load_Type,0,Timing_Filename,
			Utility_Load_Type,1,Utility_Filename,-60.0,
			CCD_DSP_GAIN_ONE,TRUE,TRUE))
		{
			CCD_Global_Error();
			if(!CCD_Setup_Shutdown(handle))
				CCD_Global_Error();
			return 2;
		}
		fprintf(stdout,"Test Filter Wheel:Initialising controller completed.\n");
	}/* end if Setup */
	if(Abort)
	{
		fprintf(stdout,"Test Filter Wheel:Aborting Filter Wheel.\n");
		if(!CCD_Filter_Wheel_Abort(handle))
		{
			CCD_Global_Error();
			return 3;
		}
		fprintf(stdout,"Test Filter Wheel:Filter Wheel Abort completed.\n");
	}
	if(Reset)
	{
		fprintf(stdout,"Test Filter Wheel:Filter Wheel Reset.\n");
		if(!CCD_Filter_Wheel_Reset(handle))
		{
			CCD_Global_Error();
			return 5;
		}
		fprintf(stdout,"Test Filter Wheel:Filter Wheel Reset completed.\n");
	}
	if(Move)
	{
		fprintf(stdout,"Test Filter Wheel:Filter Wheel Move.\n");
		fprintf(stdout,"\tUsing Position:%d.\n",Position);
		if(!CCD_Filter_Wheel_Move(handle,Position))
		{
			CCD_Global_Error();
			return 5;
		}
		fprintf(stdout,"Test Filter Wheel:Filter Wheel Move completed.\n");
	}
	if(Shutdown)
	{
		fprintf(stdout,"Test Filter Wheel:Setup Shutdown\n");
		if(!CCD_Setup_Shutdown(handle))
			CCD_Global_Error();
		fprintf(stdout,"Test Filter Wheel:Setup Shutdown completed.\n");
	}
	fprintf(stdout,"Test Filter Wheel:CCD_Interface_Close\n");
	CCD_Interface_Close(&handle);
	fprintf(stdout,"Test Filter Wheel:CCD_Interface_Close completed.\n");

	fprintf(stdout,"Test Filter Wheel:Finished Test ...\n");
	return 0;
}

/**
 * Routine to parse command line arguments.
 * @param argc The number of arguments sent to the program.
 * @param argv An array of argument strings.
 * @see #Help
 * @see #Interface_Device
 * @see #Log_Level
 * @see #PCI_Load_Type
 * @see #PCI_Filename
 * @see #Timing_Load_Type
 * @see #Timing_Filename
 * @see #Utility_Load_Type
 * @see #Utility_Filename
 * @see #Setup
 * @see #Abort
 * @see #Move
 * @see #Reset
 * @see #Position
 * @see #Shutdown
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

	for(i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"-abort")==0)
		{
			Abort = TRUE;
		}
		else if(strcmp(argv[i],"-interface_device")==0)
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"text")==0)
					Interface_Device = CCD_INTERFACE_DEVICE_TEXT;
				else if(strcmp(argv[i+1],"pci")==0)
					Interface_Device = CCD_INTERFACE_DEVICE_PCI;
				else
					fprintf(stderr,"Test Filter Wheel:Parse_Arguments:"
						"Illegal Interface Device '%s'.\n",argv[i+1]);
				i++;
			}
			else
				fprintf(stderr,"Test Filter Wheel:Parse_Arguments:"
					"Interface Device requires a device.\n");
		}
		else if(strcmp(argv[i],"-help")==0)
		{
			Help();
			exit(0);
		}
		else if((strcmp(argv[i],"-log_level")==0)||(strcmp(argv[i],"-l")==0))
		{
			if((i+1)<argc)
			{
				retval = sscanf(argv[i+1],"%d",&Log_Level);
				if(retval != 1)
				{
					fprintf(stderr,"Parse_Arguments:Log Level was not an integer:%s.\n",argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Log level requires a non-negative integer.\n");
				return FALSE;
			}
		}
		else if(strcmp(argv[i],"-move")==0)
		{
			Move = TRUE;
		}
		else if(strcmp(argv[i],"-nosetup")==0)
		{
			Setup = FALSE;
		}
		else if(strcmp(argv[i],"-noshutdown")==0)
		{
			Shutdown = FALSE;
		}
		else if(strcmp(argv[i],"-pci_filename")==0)
		{
			if((i+1)<argc)
			{
				PCI_Load_Type = CCD_SETUP_LOAD_FILENAME;
				strcpy(PCI_Filename,argv[i+1]);
				i++;
			}
			else
				fprintf(stderr,"Test Filter Wheel:Parse_Arguments:"
					"PCI filename requires a filename.\n");
		}
		else if(strcmp(argv[i],"-position")==0)
		{
			if((i+1)<argc)
			{
				Position = atoi(argv[i+1]);
				i++;
			}
			else
				fprintf(stderr,"Test Filter Wheel:Parse_Arguments:"
					"Position requires a positive integer.\n");
		}
		else if(strcmp(argv[i],"-reset")==0)
		{
			Reset = TRUE;
		}
		else if(strcmp(argv[i],"-timing_filename")==0)
		{
			if((i+1)<argc)
			{
				Timing_Load_Type = CCD_SETUP_LOAD_FILENAME;
				strcpy(Timing_Filename,argv[i+1]);
				i++;
			}
			else
				fprintf(stderr,"Test Filter Wheel:Parse_Arguments:"
					"Timing filename requires a filename.\n");
		}
		else if(strcmp(argv[i],"-utility_filename")==0)
		{
			if((i+1)<argc)
			{
				Utility_Load_Type = CCD_SETUP_LOAD_FILENAME;
				strcpy(Utility_Filename,argv[i+1]);
				i++;
			}
			else
				fprintf(stderr,"Test Filter Wheel:Parse_Arguments:"
					"Utility filename requires a filename.\n");
		}
		else
		{
			fprintf(stderr,"Test Filter Wheel:Parse_Arguments:argument '%s' not recognized.\n",argv[i]);
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
	fprintf(stdout,"Test Filter Wheel:Help.\n");
	fprintf(stdout,"Test Filter Wheel tests filter wheel operation.\n");
	fprintf(stdout,"test_filter_wheel [-abort][-interface_device <interface device>][-help]\n");
	fprintf(stdout,"\t[-move][-nosetup][-noshutdown][-pci_filename <filename>][-position <position>]\n");
	fprintf(stdout,"\t[-reset][-timing_filename <filename>][-utility_filename <filename>]\n");
	fprintf(stdout,"\t[-l[og_level] <0..5>\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-abort aborts a filter wheel operation that is already occuring.\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\t-move uses the -position argument to move the wheel to a new position.\n");
	fprintf(stdout,"\t-nosetup switches off the initialisation of the SDSU controller, "
		"it assumes this has already been done.\n");
	fprintf(stdout,"\t-noshutdown switches off the shutdown of the SDSU controller, "
		"so we can re-use the setup next time we call this program.\n");
	fprintf(stdout,"\t-pci_filename,-timing_filename,and -utility_filename select DSP files to "
		"download to the SDSU controller boards.\n");
	fprintf(stdout,"\t-position selects how many positions to drive the filter wheel in when moving it.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<filename> is a valid DSP .lod file.\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<position> is a positive integer, greater than zero and usually less than twelve.\n");
}

/*
** $Log: not supported by cvs2svn $
*/
