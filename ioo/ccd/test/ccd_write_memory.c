/* ccd_write_memory.c
 * $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/ccd_write_memory.c,v 1.1 2011-11-23 11:03:02 cjm Exp $
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
 * This program writes a memory location to the CCD camera.
 * <pre>
 * ccd_write_memory -b[oard] &lt;interface|timing|utility&gt; -d[evice_pathname] &lt;path&gt; -s[pace] &lt;x|y&gt; 
 * 	-a[ddress] &lt;address&gt; -v[alue] &lt;value&gt; -i[nterface_device] &lt;pci|text&gt; 
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
static char rcsid[] = "$Id: ccd_write_memory.c,v 1.1 2011-11-23 11:03:02 cjm Exp $";
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
 * The board of the SDSU controller.
 */
static enum CCD_DSP_BOARD_ID Board = CCD_DSP_HOST_BOARD_ID;
/**
 * Which part of DSP memory.
 */
static enum CCD_DSP_MEM_SPACE Memory_Space = 0;
/**
 * The address in memory.
 */
static int Memory_Address = 0;
/**
 * The to set the memory address to.
 */
static int Value = 0;
/**
 * The pathname of the device to contact.
 */
static char Device_Pathname[256] = "";

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
 * @see #Device_Pathname
 * @see #Board
 * @see #Memory_Sapce
 * @see #Memory_Address
 * @see #Log_Level
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_Command_WRM
 * @see ../cdocs/ccd_dsp.html#CCD_DSP_Command_Manual_To_String
 * @see ../cdocs/ccd_global.html#CCD_Global_Initialise
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Handler_Function
 * @see ../cdocs/ccd_global.html#CCD_Global_Log_Handler_Stdout
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Filter_Function
 * @see ../cdocs/ccd_global.html#CCD_Global_Log_Filter_Level_Absolute
 * @see ../cdocs/ccd_global.html#CCD_Global_Set_Log_Filter_Level
 * @see ../cdocs/ccd_interface.html#CCD_INTERFACE_DEVICE
 * @see ../cdocs/ccd_interface.html#CCD_Interface_Open
 * @see ../cdocs/ccd_interface.html#CCD_Interface_Close
 */
int main(int argc, char *argv[])
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

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
	if(strlen(Device_Pathname) == 0)
	{
		fprintf(stdout,"Selecting default device path.\n");
		switch(Interface_Device)
		{
			case CCD_INTERFACE_DEVICE_PCI:
				strcpy(Device_Pathname,CCD_PCI_DEFAULT_DEVICE_ZERO);
				break;
			case CCD_INTERFACE_DEVICE_TEXT:
				strcpy(Device_Pathname,"frodospec_ccd_write_memory.txt");
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
		return 1;
	}
	fprintf(stdout,"SDSU device opened.\n");
	fflush(stdout);
	fprintf(stdout,"Writing %#x at address %d:%#x.\n",Value,Memory_Space,Memory_Address);
	if(CCD_DSP_Command_WRM(handle,Board,Memory_Space,Memory_Address,Value) != CCD_DSP_DON)
	{
		CCD_Global_Error();
		return 1;
	}
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
 * @see #Log_Level
 * @see #Interface_Device
 * @see #Board
 * @see #Memory_Space
 * @see #Memory_Address
 * @see #Value
 * @see #Device_Pathname
 */
static int Parse_Arguments(int argc, char *argv[])
{
	int i,retval;

	for(i=1;i<argc;i++)
	{
		if((strcmp(argv[i],"-address")==0)||(strcmp(argv[i],"-a")==0))
		{
			if((i+1)<argc)
			{
				sscanf(argv[i+1],"%i",&Memory_Address);
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:"
					"Address requires a positive integer.\n");
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
		else if((strcmp(argv[i],"-device_pathname")==0)||(strcmp(argv[i],"-d")==0))
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
		else if((strcmp(argv[i],"-space")==0)||(strcmp(argv[i],"-s")==0))
		{
			if((i+1)<argc)
			{
				if(strcmp(argv[i+1],"x")==0)
					Memory_Space = CCD_DSP_MEM_SPACE_X;
				else if(strcmp(argv[i+1],"y")==0)
					Memory_Space = CCD_DSP_MEM_SPACE_Y;
				else
				{
					fprintf(stderr,"Parse_Arguments:Memory Space [x|y].\n");
					return FALSE;
				}
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:Memory Space [x|y].\n");
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
		else if((strcmp(argv[i],"-value")==0)||(strcmp(argv[i],"-v")==0))
		{
			if((i+1)<argc)
			{
				sscanf(argv[i+1],"%i",&Value);
				i++;
			}
			else
			{
				fprintf(stderr,"Parse_Arguments:"
					"Value requires an integer.\n");
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
	fprintf(stdout,"CCD Write Memory:Help.\n");
	fprintf(stdout,"CCD Write Memory writes a value to a controller memory location.\n");
	fprintf(stdout,"ccd_write_memory [-i[nterface_device] <interface device>]\n");
	fprintf(stdout,"\t[-d[evice_pathname] <path>]\n");
	fprintf(stdout,"\t[-b[oard] <controller board>][-s[pace] <memory space>]\n");
	fprintf(stdout,"\t[-a[ddress] <memory address>][-v[alue] <value>]\n");
	fprintf(stdout,"\t[-l[og_level] <0..5>\n");
	fprintf(stdout,"\t[-t[ext_print_level] <commands|replies|values|all>][-h[elp]]\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t-interface_device selects the device to communicate with the SDSU controller.\n");
	fprintf(stdout,"\t-device_pathname can select which controller (/dev/astropci[0|1]) or text output file.\n");
	fprintf(stdout,"\t-help prints out this message and stops the program.\n");
	fprintf(stdout,"\n");
	fprintf(stdout,"\t<interface device> can be either [pci|text].\n");
	fprintf(stdout,"\t<controller board> can be either [interface|timing|utility].\n");
	fprintf(stdout,"\t<memory space> can be either [x|y].\n");
	fprintf(stdout,"\t<memory address> is a positive integer, and can be decimal or hexidecimal(0x).\n");
	fprintf(stdout,"\t<value> is an integer, and can be decimal or hexidecimal (0x).\n");
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.5  2008/11/20 11:34:58  cjm
** *** empty log message ***
**
** Revision 1.4  2006/11/06 16:52:49  eng
** Added includes to fix implicit function declarations.
**
** Revision 1.3  2006/05/16 18:18:20  cjm
** gnuify: Added GNU General Public License.
**
** Revision 1.2  2002/11/07 19:18:22  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 1.1  2001/01/18 10:51:26  cjm
** Initial revision
**
**
*/
