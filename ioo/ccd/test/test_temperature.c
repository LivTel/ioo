/* test_temperature.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_temperature.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
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
 * This program allows the user to:
 * <ul>
 * <li>Get the current CCD temperature (in the dewar).
 * <li>Set the current CCD temperature (in the dewar).
 * <li>Get the heater adus (the amount of heat being put into the dewar).
 * <li>Get the utility board adus (a measure of how hot the utility board electronics are).
 * </ul>
 * <pre>
 * test_temperature -i[nterface_device] &lt;pci|text&gt; -g[et] -s[et] &lt;temperature (degrees C)&gt; -heater[_adus] 
 * 	-u[tility_board] -t[ext_print_level] &lt;commands|replies|values|all&gt; -h[elp]
 * </pre>
 * @author $Author: cjm $
 * @version $Revision: 1.1 $
 */
/* hash definitions */
/**
 * Maximum length of some of the strings in this program.
 */
#define MAX_STRING_LENGTH	(256)

/* enums */
/**
 * Enumeration determining which command this program executes. One of:
 * <ul>
 * <li>COMMAND_ID_NONE
 * <li>COMMAND_ID_GET
 * <li>COMMAND_ID_GET_UTILITY_BOARD
 * <li>COMMAND_ID_SET
 * <li>COMMAND_ID_HEATER_ADUS
 * </ul>
 */
enum COMMAND_ID
{
	COMMAND_ID_NONE=0,COMMAND_ID_GET,COMMAND_ID_GET_UTILITY_BOARD,COMMAND_ID_SET,COMMAND_ID_HEATER_ADUS
};

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_temperature.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_PCI;
/**
 * Which SDSU command to call.
 * @see #COMMAND_ID
 */
static enum COMMAND_ID Command = COMMAND_ID_NONE;
/**
 * The target temperature to use when setting the temperature.
 */
static double Target_Temperature = 0.0;

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
 * @see #Command
 * @see ../cdocs/ccd_global.html#CCD_Global_Initialise
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Handler_Function
 * @see ../cdocs/ccd_global.html#CCD_Global_Log_Handler_Stdout
 * @see ../cdocs/ccd_global.html#CCD_Global_Error
 * @see ../cdocs/ccd_interface.html#CCD_INTERFACE_DEVICE
 * @see ../cdocs/ccd_interface.html#CCD_Interface_Open
 * @see ../cdocs/ccd_interface.html#CCD_Interface_Close
 * @see ../cdocs/ccd_pci.html#CCD_PCI_DEFAULT_DEVICE_ZERO
 * @see ../cdocs/ccd_temperature.html#CCD_Temperature_Get
 * @see ../cdocs/ccd_temperature.html#CCD_Temperature_Get_Utility_Board_ADU
 * @see ../cdocs/ccd_temperature.html#CCD_Temperature_Set
 * @see ../cdocs/ccd_temperature.html#CCD_Temperature_Get_Heater_ADU
 * @see ../cdocs/ccd_temperature.html#CCD_Temperature_Heater_ADU_To_Power
 * @see ../cdocs/ccd_text.html#CCD_Text_Set_Print_Level
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	char device_pathname[256];
	int retval;
	int adu;
	double temperature,power;

	fprintf(stdout,"Parsing Arguments.\n");
	if(!Parse_Arguments(argc,argv))
		return 1;
	CCD_Text_Set_Print_Level(Text_Print_Level);
	fprintf(stdout,"Initialise Controller:Using device %d.\n",Interface_Device);
	CCD_Global_Initialise();
	CCD_Global_Set_Log_Handler_Function(CCD_Global_Log_Handler_Stdout);
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
		return 1;
	}
	fprintf(stdout,"SDSU device opened.\n");
	switch(Command)
	{
		case COMMAND_ID_GET:
			fprintf(stdout,"Calling CCD_Temperature_Get.\n");
			retval = CCD_Temperature_Get(handle,&temperature);
			if(retval == FALSE)
			{
				CCD_Global_Error();
				return 2;
			}
			fprintf(stdout,"The current temperature is %.2f degrees centigrade.\n",temperature);
			break;
		case COMMAND_ID_GET_UTILITY_BOARD:
			fprintf(stdout,"Calling CCD_Temperature_Get_Utility_Board_ADU.\n");
			retval = CCD_Temperature_Get_Utility_Board_ADU(handle,&adu);
			if(retval == FALSE)
			{
				CCD_Global_Error();
				return 2;
			}
			fprintf(stdout,"The current utility board temperature is %#x ADUs.\n",
				temperature);
			break;
		case COMMAND_ID_SET:
			fprintf(stdout,"Calling CCD_Temperature_Set.\n");
			retval = CCD_Temperature_Set(handle,Target_Temperature);
			if(retval == FALSE)
			{
				CCD_Global_Error();
				return 3;
			}
			fprintf(stdout,"The temperature has been set to %.2f.\n",Target_Temperature);
			break;
		case COMMAND_ID_HEATER_ADUS:
			fprintf(stdout,"Calling CCD_Temperature_Get_Heater_ADU.\n");
			retval = CCD_Temperature_Get_Heater_ADU(handle,&adu);
			if(retval == FALSE)
			{
				CCD_Global_Error();
				return 4;
			}
			fprintf(stdout,"The current heater ADUS are:%d.\n",adu);
			retval = CCD_Temperature_Heater_ADU_To_Power(adu,&power);
			if(retval == FALSE)
			{
				CCD_Global_Error();
				return 6;
			}
			fprintf(stdout,"The current heater power is: %.6f Watts.\n",power);

			break;
		case COMMAND_ID_NONE:
			fprintf(stdout,"Please select a command to execute:"
				"(-g[et] | -set | -heater[_adus] | -u[tility_board]).\n");
			Help();
			exit(5);
	}
	fprintf(stdout,"Command Completed.\n");
	fprintf(stdout,"CCD_Interface_Close\n");
	CCD_Interface_Close(&handle);
	fprintf(stdout,"CCD_Interface_Close completed.\n");
	return retval;
}

/**
 * Routine to parse command line arguments.
 * @param argc The number of arguments sent to the program.
 * @param argv An array of argument strings.
 * @see #Help
 * @see #Text_Print_Level
 * @see #Interface_Device
 * @see #Command
 * @see #COMMAND_ID
 * @see #Target_Temperature
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-get")==0)||(strcmp(argv[i],"-g")==0))
		{
			Command = COMMAND_ID_GET;
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
		else if((strcmp(argv[i],"-heater_adus")==0)||(strcmp(argv[i],"-heater")==0))
		{
			Command = COMMAND_ID_HEATER_ADUS;
		}
		else if((strcmp(argv[i],"-help")==0)||(strcmp(argv[i],"-h")==0))
		{
			Help();
			exit(0);
		}
		else if((strcmp(argv[i],"-set")==0)||(strcmp(argv[i],"-s")==0))
		{
			if((i+1)<argc)
			{
				Command = COMMAND_ID_SET;
				retval = sscanf(argv[i+1],"%lf",&Target_Temperature);
				if(retval != 1)
				{
					fprintf(stderr,
						"Parse_Arguments:Set temperature:Parsing temperature %s failed.\n",
						argv[i+1]);
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Set requires a temperature.\n");
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
		else if((strcmp(argv[i],"-utility_board")==0)||(strcmp(argv[i],"-u")==0))
		{
			Command = COMMAND_ID_GET_UTILITY_BOARD;
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
	fprintf(stdout,"Test Temperature:Help.\n");
	fprintf(stdout,"This program allows the user to set/get the current CCD temperature, and get the heater adus.\n");
	fprintf(stdout,"test_temperature [-i[nterface_device] <pci|text>]\n");
	fprintf(stdout,"\t[-g[et]] [-u[tility_board]] [-s[et] <temperature>] [-heater[_adus]]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-text_print_level selects how much data the text interface device prints out.\n");
	fprintf(stdout,"\t-get Gets the current CCD temperature, in degrees centigrade.\n");
	fprintf(stdout,"\t-utility_board Gets the current utility board temperature ADUs.\n");
	fprintf(stdout,"\t-set Sets the current CCD temperature. The parameter is in degrees centigrade.\n");
	fprintf(stdout,"\t-heater_adus Gets the amount of power (adus) going to the heater to control the CCD temperature.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
}

/*
** $Log: not supported by cvs2svn $
*/
