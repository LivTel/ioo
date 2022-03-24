/* test_gain.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_gain.c,v 1.1 2012-05-10 10:08:01 cjm Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log_udp.h"
#include "ccd_dsp.h"
#include "ccd_global.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_setup.h"

/**
 * This program allows the user to set the gain parameter using the SGN command.
 * <pre>
 * test_gain -i[nterface_device] &lt;pci|text&gt; -gain <1|2|4|9> -gain_speed <true|false> 
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
static char rcsid[] = "$Id: test_gain.c,v 1.1 2012-05-10 10:08:01 cjm Exp $";
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

/**
 * Main program.
 * @param argc The number of arguments to the program.
 * @param argv An array of argument strings.
 * @return This function returns 0 if the program succeeds, and a positive integer if it fails.
 * @see #Text_Print_Level
 * @see #Interface_Device
 * @see #Gain
 * @see #Gain_Speed
 * @see #Log_Filter_Level
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_Command_SGN
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

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
				strcpy(Device_Pathname,"io_ccd_text_exposure.txt");
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
/* call CCD_DSP_Command_SGN */
	fprintf(stdout,"Calling CCD_DSP_Command_SGN(gain+%d,Gain_Speed=%d).\n",Gain,Gain_Speed);
	retval = CCD_DSP_Command_SGN(handle,Gain,Gain_Speed);
	if(retval != CCD_DSP_DON)
	{
		CCD_Global_Error();
		CCD_Interface_Close(&handle);
		return 3;
	}
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
 * @see #Device_Pathname
 * @see #Gain
 * @see #Gain_Speed
 * @see #Log_Filter_Level
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-gain")==0)||(strcmp(argv[i],"-g")==0))
		{
			if((i+1)<argc)
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
				i += 1;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Gain requires a value <1|2|4|9>.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-gain_speed")==0)||(strcmp(argv[i],"-gs")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"true")==0)
				{
					Gain_Speed = TRUE;
				}
				else if(strcmp(argv[i+1],"false")==0)
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
				i += 1;
			}
		}
		else if((strcmp(argv[i],"-help")==0)||(strcmp(argv[i],"-h")==0))
		{
			Help();
			exit(0);
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
	fprintf(stdout,"Test Gain:Help.\n");
	fprintf(stdout,"This program calls CCD_DSP_Command_SGN to set the Gain.\n");
	fprintf(stdout,"test_gain [-i[nterface_device] <pci|text>]\n");
	fprintf(stdout,"\t[-device_pathname <path>]\n");
	fprintf(stdout,"\t[-temperature <temperature>]\n");
	fprintf(stdout,"\t[-g[ain] <1|2|4|9>] [-gain_speed <true|false>]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\t[-log_level <bit number>]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-device_pathname can select which controller (/dev/astropci[0|1]) or text output file.\n");
	fprintf(stdout,"\t-gain takes a value and a speed (true for fast, false for slow).\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<log level> is the verbosity between 0 and 5 inclusive.\n");
}

/*
** $Log: not supported by cvs2svn $
*/
