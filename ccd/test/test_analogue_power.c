/* test_analogue_power.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_analogue_power.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
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
 * This program allows the user to turn on and off the SDSU analogue power supply.
 * <pre>
 * test_analogue_power -i[nterface_device] &lt;pci|text&gt; -o[n] -off|-f 
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

/* enums */
/**
 * Enumeration determining which command this program executes. One of:
 * <ul>
 * <li>COMMAND_ID_NONE
 * <li>COMMAND_ID_POWER_ON
 * <li>COMMAND_ID_POWER_OFF
 * </ul>
 */
enum COMMAND_ID
{
	COMMAND_ID_NONE=0,COMMAND_ID_POWER_OFF,COMMAND_ID_POWER_ON
};

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_analogue_power.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_NONE;
/**
 * Which SDSU command to call.
 * @see #COMMAND_ID
 */
static enum COMMAND_ID Command = COMMAND_ID_NONE;

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
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	char device_pathname[256];
	int retval;

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
			strcpy(device_pathname,"frodospec_ccd_test_analogue_power.txt");
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
		case COMMAND_ID_POWER_ON:
			fprintf(stdout,"Calling CCD_DSP_Command_PON.\n");
			retval = CCD_DSP_Command_PON(handle);
			break;
		case COMMAND_ID_POWER_OFF:
			fprintf(stdout,"Calling CCD_DSP_Command_POF.\n");
			retval = CCD_DSP_Command_POF(handle);
			break;
		case COMMAND_ID_NONE:
			fprintf(stdout,"Please select a command to execute (-o[n] | -off | -f).\n");
			Help();
			exit(1);
	}
	if(retval == FALSE)
	{
		CCD_Global_Error();
		return 1;
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
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i;

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
		else if((strcmp(argv[i],"-on")==0)||(strcmp(argv[i],"-o")==0))
		{
			Command = COMMAND_ID_POWER_ON;
		}
		else if((strcmp(argv[i],"-off")==0)||(strcmp(argv[i],"-f")==0))
		{
			Command = COMMAND_ID_POWER_OFF;
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
	fprintf(stdout,"Test Analogue Power control:Help.\n");
	fprintf(stdout,"This program allows the user to turn on and off the SDSU analogue power supply.\n");
	fprintf(stdout,"test_analogue_power [-i[nterface_device] <pci|text>]\n");
	fprintf(stdout,"\t[-o[n][-off|-f]]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.4  2008/11/20 11:34:58  cjm
** *** empty log message ***
**
** Revision 1.3  2006/11/06 16:52:49  eng
** Added includes to fix implicit function declarations.
**
** Revision 1.2  2006/05/16 18:18:22  cjm
** gnuify: Added GNU General Public License.
**
** Revision 1.1  2002/11/07 19:18:22  cjm
** Initial revision
**
*/
