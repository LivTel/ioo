/* test_dsp_download.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/test_dsp_download.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
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
 * This program downloads the specified .lod file to the specified board.
 * <pre>
 * test_dsp_download -b[oard] &lt;interface|timing|utility&gt; -f[ile] &lt;filename&gt; 
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
/**
 * The bit on the timing board, X memory space, location 0, which is set when the 
 * timing board is idling between exposures (idle clocking the CCD).
 * Copied from ccd_setup.c , which usually deals with this.
 */
#define SETUP_TIMING_IDLMODE	(1<<2)

/* internal variables */
/**
 * Revision control system identifier.
 */
static char rcsid[] = "$Id: test_dsp_download.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
/**
 * How much information to print out when using the text interface.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_ALL;
/**
 * Which interface to communicate with the SDSU controller with.
 */
static enum CCD_INTERFACE_DEVICE_ID Interface_Device = CCD_INTERFACE_DEVICE_NONE;
/**
 * The board of the SDSU controller to query.
 */
static enum CCD_DSP_BOARD_ID Board = CCD_DSP_HOST_BOARD_ID;
/**
 * The filename of the .lod file to download.
 */
static char *Filename = NULL;

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
 * @see #Filename
 * @see #Board
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
			strcpy(device_pathname,"frodospec_ccd_test_dsp_download.txt");
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
/* if we are downloading to the timing board, and if we are currently in IDL mode - come out of this mode */
	if(Board == CCD_DSP_TIM_BOARD_ID)
	{
		fprintf(stdout,"Downloading to Timing board:Checking whether we are idling the CCD.\n");
		value = CCD_DSP_Command_RDM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_X,0);
		fprintf(stdout,"RDM(Timing,X,0) returned:%#x , checking whether bit 2 set (IDLMODE).\n",value);
		bit_value = value & SETUP_TIMING_IDLMODE;
		if(bit_value > 0)
		{
			fprintf(stdout,"Sending STP to Timing board.\n");
			if(CCD_DSP_Command_STP(handle)!= CCD_DSP_DON)
			{
				fprintf(stderr,"Timing Board:Failed to load filename '%s':STP failed.",
					Filename);
				CCD_Global_Error();
				return 3;
			}
		}/* end if bit_value set */
	}/* end if board is timing */
/* download filename to controller board */
	if(Filename != NULL)
		fprintf(stdout,"Downloading %s to %d.\n",Filename,Board);
	else
		fprintf(stdout,"Downloading NULL to %d.\n",Board);
	fflush(stdout);
	retval = CCD_DSP_Download(handle,Board,Filename);
	if(retval == FALSE)
	{
		CCD_Global_Error();
		return 4;
	}
	fprintf(stdout,"Download Completed.\n");
/* if neccessary restart IDL mode. Note voodoo does not do this! */
	if(Board == CCD_DSP_TIM_BOARD_ID)
	{
		if(bit_value > 0)
		{
			fprintf(stdout,"Sending IDL to Timing board.\n");
			if(CCD_DSP_Command_IDL(handle)!=CCD_DSP_DON)
			{
				fprintf(stderr,"Timing Board:Failed to load filename '%s':IDL failed.",
					Filename);
				CCD_Global_Error();
				return 5;
			}
		}/* end if bit_value set */
	}/* end if board is timing */
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
 * @see #Board
 * @see #Filename
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-filename")==0)||(strcmp(argv[i],"-f")==0))
		{
			if((i+1)<argc)
			{
				Filename = argv[i+1];
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Filename required.\n");
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
	fprintf(stdout,"Test DSP Download:Help.\n");
	fprintf(stdout,"This program downloads the specified .lod file to the specified board..\n");
	fprintf(stdout,"test_dsp_download [-i[nterface_device] <interface device>]\n");
	fprintf(stdout,"\t[-b[oard] <controller board>][-f[ilename] <filename>]\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<controller board> can be either [interface|timing|utility].\n");
	fprintf(stdout,"\t<filename> should be a valid .lod file pathname.\n");
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.4  2008/11/20 11:34:58  cjm
** *** empty log message ***
**
** Revision 1.3  2006/11/06 16:52:49  eng
** Added includes to fix implicit function declarations.
**
** Revision 1.2  2006/05/16 18:18:25  cjm
** gnuify: Added GNU General Public License.
**
** Revision 1.1  2002/11/07 19:18:22  cjm
** Initial revision
**
*/
