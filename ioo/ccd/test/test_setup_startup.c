/* test_setup_startup.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_setup_startup.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ccd_dsp.h"
#include "ccd_dsp_download.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_temperature.h"
#include "ccd_setup.h"

/**
 * This program tests CCD_Setup_Startup, which does initial configuration of the SDSU controller.
 * <pre>
 * test_setup_startup -i[nterface_device] &lt;pci|text&gt; -pci_filename &lt;filename&gt; 
 * 	-timing_filename &lt;filename&gt; -utility_filename &lt;filename&gt; -temperature &lt;temperature&gt;
 * 	-t[ext_print_level] &lt;commands|replies|values|all&gt; -h[elp]
 * </pre>
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
static char rcsid[] = "$Id: test_setup_startup.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_NONE;
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
 * @see #Temperature
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
	fprintf(stdout,"CCD_Setup_Startup completed\n");
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
 * @see #PCI_Load_Type
 * @see #PCI_Filename
 * @see #Timing_Load_Type
 * @see #Timing_Filename
 * @see #Utility_Load_Type
 * @see #Utility_Filename
 * @see #Temperature
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-interface_device")==0)||(strcmp(argv[i],"-i")==0))
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
	fprintf(stdout,"Test Setup Startup:Help.\n");
	fprintf(stdout,"This program tests the CCD_Setup_Startup routine, which initialises the SDSU controller.\n");
	fprintf(stdout,"test_setup_startup [-i[nterface_device] <interface device>]\n");
	fprintf(stdout,"\t[-pci_filename <filename>][-timing_filename <filename>][-utility_filename <filename>]\n");
	fprintf(stdout,"\t[-temperature <temperature>]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<filename> should be a valid .lod file pathname.\n");
	fprintf(stdout,"\t<temperature> should be a valid double, a temperature in degrees Celcius.\n");
}

/*
** $Log: not supported by cvs2svn $
*/
