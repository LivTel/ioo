/* test_manual_command.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_manual_command.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
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

/**
 * This program sends a manual command to a SDSU controller board.
 * <pre>
 * test_manual_command -b[oard] &lt;interface|timing|utility&gt; -i[nterface_device] &lt;pci|text&gt; 
 *      -c[ommand] &lt;command&gt; [-a[rgument] &lt;argument&gt; ...]
 * 	-t[ext_print_level] &lt;commands|replies|values|all&gt; -h[elp]
 * </pre>
 * @author $Author: cjm $
 * @version $Revision: 1.1 $
 */
/**
 * Maximum length of some of the strings in this program.
 */
#define MAX_STRING_LENGTH	(256)

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_manual_command.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
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
 * The board of the SDSU controller to query.
 */
static enum CCD_DSP_BOARD_ID Board = CCD_DSP_HOST_BOARD_ID;
/**
 * The Command.
 */
static int Command = 0;
/**
 * The list of arguments to send along with the command.
 * @see ../cdocs/ccd_pci.html#CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT
 */
int Argument_List[CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT];
/**
 * The number of arguments currently in the list.
 */
int Argument_Count = 0;

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
 * @see #Board
 * @see #Command
 * @see #Argument_List
 * @see #Argument_Count
 * @see #Parse_Arguments
 * @see #Log_Level
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_Command_Manual
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_Command_Manual_To_String
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
 * @see ../cdocs/ccd_text.html#CCD_Text_Set_Print_Level
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	char device_pathname[256];
	int retval,reply_value;

	fprintf(stdout,"Parsing Arguments.\n");
	if(!Parse_Arguments(argc,argv))
		return 1;
	CCD_Text_Set_Print_Level(Text_Print_Level);
	fprintf(stdout,"Initialise Controller:Using device %d.\n",Interface_Device);
	CCD_Global_Initialise();
	CCD_Global_Set_Log_Handler_Function(CCD_Global_Log_Handler_Stdout);
	CCD_Global_Set_Log_Filter_Function(CCD_Global_Log_Filter_Level_Absolute);
	CCD_Global_Set_Log_Filter_Level(Log_Level);

	fprintf(stdout,"Opening SDSU device.\n");
	fflush(stdout);
	switch(Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_PCI:
			strcpy(device_pathname,CCD_PCI_DEFAULT_DEVICE_ZERO);
			break;
		case CCD_INTERFACE_DEVICE_TEXT:
			strcpy(device_pathname,"o_test_manual_command.txt");
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
	/* do manual command */
	fprintf(stdout,"Sending manual command %#x to board %d with %d arguments.\n",Command,Board,Argument_Count);
	retval = CCD_DSP_Command_Manual(handle,Board,Command,Argument_List,Argument_Count,&reply_value);
	if(!retval)
	{
		CCD_Global_Error();
		return 1;
	}
	fprintf(stdout,"Returned Result = %#x\n",retval);
	fprintf(stdout,"Reply was = %#x (%s)\n",reply_value,CCD_DSP_Command_Manual_To_String(reply_value));
	fprintf(stdout,"CCD_Interface_Close\n");
	CCD_Interface_Close(&handle);
	fprintf(stdout,"CCD_Interface_Close completed.\n");
	return retval;
}

/**
 * Routine to parse command line arguments.
 * @param argc The number of arguments sent to the program.
 * @param argv An array of argument strings.
 * @return TRUE if arguments parsed successfully, FALSE if an error occured.
 * @see #Help
 * @see #Text_Print_Level
 * @see #Log_Level
 * @see #Interface_Device
 * @see #Board
 * @see #Command
 * @see #Argument_List
 * @see #Argument_Count
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_Command_String_To_Manual
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-argument")==0)||(strcmp(argv[i],"-a")==0))
		{
			if((i+1)<argc)
			{
				if(Argument_Count < CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT)
				{
					/* if numeric command is either decimal or hex number */
					if(isdigit(argv[i+1][0]))
					{
						retval = sscanf(argv[i+1],"%i",&(Argument_List[Argument_Count]));
						if(retval == 0)
						{
							fprintf(stderr,"Failed to parse argument(numeric) '%s'.\n",
								argv[i+1]);
							return FALSE;
						}
						Argument_Count++;
					}
					else
					{
						/* otherwise command is a three letter string, try parsing that */
						Command = CCD_DSP_Command_String_To_Manual(argv[i+1]);
						if(Command == 0)
						{
							fprintf(stderr,"Failed to parse command (string)'%s'.\n",
								argv[i+1]);
							CCD_Global_Error();
							return FALSE;
						}
					}
					i++;
				}
				else
				{
					fprintf(stderr,"Parse_Arguments:Too many command arguments (%d,%d).\n",
						Argument_Count,CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT);
					return FALSE;
				}
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Command requires an integer.\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-board")==0)||(strcmp(argv[i],"-b")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"interface")==0)
					Board = CCD_DSP_INTERFACE_BOARD_ID;
				else if(strcmp(argv[i+1],"timing")==0)
					Board = CCD_DSP_TIM_BOARD_ID;
				else if(strcmp(argv[i+1],"utility")==0)
					Board = CCD_DSP_UTIL_BOARD_ID;
				else
				{
					fprintf(stderr,"Parse_Arguments:Board [interface|timing|utility].\n");
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Board [interface|timing|utility].\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-command")==0)||(strcmp(argv[i],"-c")==0))
		{
			if((i+1)<argc)
			{
				/* if numeric command is either decimal or hex number */
				if(isdigit(argv[i+1][0]))
				{
					retval = sscanf(argv[i+1],"%i",&Command);
					if(retval == 0)
					{
						fprintf(stderr,"Failed to parse command (numeric)'%s'.\n",argv[i+1]);
						return FALSE;
					}
				}
				else
				{
					/* otherwise command is a three letter string, try parsing that */
					Command = CCD_DSP_Command_String_To_Manual(argv[i+1]);
					if(Command == 0)
					{
						fprintf(stderr,"Failed to parse command (string)'%s'.\n",argv[i+1]);
						CCD_Global_Error();
						return FALSE;
					}
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Command requires an integer.\n");
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
		else if((strcmp(argv[i],"-help")==0)||(strcmp(argv[i],"-h")==0))
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
	fprintf(stdout,"Test Manual Command:Help.\n");
	fprintf(stdout,"This program sends a manual command to the SDSU controller.\n");
	fprintf(stdout,"test_manual_command [-i[nterface_device] <interface device>]\n");
	fprintf(stdout,"\t[-b[oard] <controller board>][-c[ommand] <command>][-a[rgument] <argument> ...]\n");
	fprintf(stdout,"\t[-l[og_level] <0..5>\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<controller board> can be either [interface|timing|utility].\n");
	fprintf(stdout,"\t<command> is a positive integer, either decimal, hexidecimal (0x) or a three letter command.\n");
	fprintf(stdout,"\t<argument> is a positive integer, either decimal, hexidecimal (0x) or a three letter argument.\n");
}

/*
** $Log: not supported by cvs2svn $
*/
