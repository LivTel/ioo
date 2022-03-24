/* test_data_link_multi.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_data_link_multi.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
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
 * This program tests the data link to a board in the SDSU controller. The TDL command is executed many times.
 * <pre>
 * test_data_link_multi -b[oard] &lt;interface|timing|utility&gt; -v[alue] &lt;test data value&gt; -c[ount] &lt;n&gt;
 * 	-i[nterface_device] &lt;pci|text&gt; -t[ext_print_level] &lt;commands|replies|values|all&gt; -h[elp]
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
static char rcsid[] = "$Id: test_data_link_multi.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_PCI;
/**
 * The board of the SDSU controller to query.
 */
static enum CCD_DSP_BOARD_ID Board = CCD_DSP_HOST_BOARD_ID;
/**
 * The test data link start value.
 */
static int Start_Value = 0;
/**
 * The number of times to send the TDL.
 */
static int Count = 1;
/**
 * How many times the TDL reply was received successfully.
 */
static int Success_Count = 0;
/**
 * How many times the TDL failed.
 */
static int Fail_Count = 0;

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
 * @see #Start_Value
 * @see #Count
 * @see #Success_Count
 * @see #Fail_Count
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	char device_pathname[256];
	int value,retval,i;

	fprintf(stdout,"Parsing Arguments.\n");
	if(!Parse_Arguments(argc,argv))
		return 1;
	CCD_Text_Set_Print_Level(Text_Print_Level);
	fprintf(stdout,"Initialise Controller:Using device %d.\n",Interface_Device);
	CCD_Global_Initialise();

	fprintf(stdout,"Opening SDSU device.\n");
	fflush(stdout);
	switch(Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_PCI:
			strcpy(device_pathname,CCD_PCI_DEFAULT_DEVICE_ZERO);
			break;
		case CCD_INTERFACE_DEVICE_TEXT:
			strcpy(device_pathname,"frodospec_ccd_test_data_link_multi.txt");
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
	value = Start_Value;
	Success_Count = 0;
	Fail_Count = 0;
	for(i=0; i < Count; i++)
	{
		fprintf(stdout,"Attempt %d : Testing data link to %d with  %#x.\n",i,Board,value);
		retval = CCD_DSP_Command_TDL(handle,Board,value);
		if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
		{
			CCD_Global_Error();
			Fail_Count++;
		}
		else 
			Success_Count++;
		value++;
		fprintf(stdout,"Result = %#x\n",retval);
	}/* for */
	fprintf(stdout,"TDL sent to board %d %d times, %d successes, %d failures.\n",Board,Count,
		Success_Count,Fail_Count++);
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
 * @see #Board
 * @see #Start_Value
 * @see #Count
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-board")==0)||(strcmp(argv[i],"-b")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"interface")==0)
					Board = CCD_DSP_INTERFACE_BOARD_ID;
				else if(strcmp(argv[i+1],"pci")==0)
					Board = CCD_DSP_INTERFACE_BOARD_ID;
				else if(strcmp(argv[i+1],"timing")==0)
					Board = CCD_DSP_TIM_BOARD_ID;
				else if(strcmp(argv[i+1],"utility")==0)
					Board = CCD_DSP_UTIL_BOARD_ID;
				else
				{
					fprintf(stderr,"Parse_Arguments:Board [interface|pci|timing|utility].\n");
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Board [interface|pci|timing|utility].\n");
				return FALSE;
			}
		}
		else if((strcmp(argv[i],"-count")==0)||(strcmp(argv[i],"-c")==0))
		{
			if((i+1)<argc)
			{
				sscanf(argv[i+1],"%i",&Count);
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:count requires an integer.\n");
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
		else if((strcmp(argv[i],"-value")==0)||(strcmp(argv[i],"-v")==0))
		{
			if((i+1)<argc)
			{
				sscanf(argv[i+1],"%i",&Start_Value);
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:value requires an integer.\n");
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
	fprintf(stdout,"Test Data Link Multi:Help.\n");
	fprintf(stdout,"This program tests the data link to a board in the SDSU controller multiple times.\n");
	fprintf(stdout,"test_data_link_multi [-i[nterface_device] <interface device>]\n");
	fprintf(stdout,"\t[-b[oard] <controller board>][-v[alue] <value>][-c[ount] <n>]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<controller board> can be either [interface|timing|utility].\n");
	fprintf(stdout,"\t<value> is a positive integer, either decimal or hexidecimal (0x), as is the start value to send.\n");
	fprintf(stdout,"\t<n> is a positive integer, and is the number of tests to attempt.\n");
}

/*
** $Log: not supported by cvs2svn $
*/
