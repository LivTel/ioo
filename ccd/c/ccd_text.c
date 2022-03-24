/* ccd_text.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_text.c,v 1.1 2011-11-23 10:59:52 cjm Exp $
*/
/**
 * ccd_text.c implements a virtual interface that prints out all commands that are sent to the SDSU CCD Controller
 * and emulates appropriate replies to requests.
 * @author SDSU, Chris Mottram
 * @version $Revision: 1.1 $
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for nanosleep.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for nanosleep.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#ifndef _POSIX_TIMERS
#include <sys/time.h>
#endif
#include "ccd_global.h"
#include "ccd_exposure.h"
#include "ccd_dsp.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_interface_private.h"

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_text.c,v 1.1 2011-11-23 10:59:52 cjm Exp $";

/* #defines */
/**
 * Number of PCI argument registers.
 * @see ccd_pci.html#CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT
 */
#define TEXT_ARGUMENT_COUNT	(CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT)
/*#define TEXT_ARGUMENT_COUNT	(5)*/

/**
 * The number of nanoseconds in one microsecond.
 */
#define TEXT_ONE_MICROSECOND_NS		(1000)
/**
 * Maximum length of the filename specifying the output text file.
 */
#define TEXT_MAX_FILENAME_LENGTH        (256)

/**
 * The default value to set the Controller_Config to. This is the default value of the timing boards
 * CONFIG (controller configuration) word in the DSP code.
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_CCD_REV3B
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_TIM_REV4B
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_UTILITY_REV3
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_SHUTTER
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_NONLINEAR_TEMP_CONV
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_SUBARRAY
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_BINNING
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_CONFIG_BIT_SERIAL_SPLIT
 */
#define TEXT_DEFAULT_CONTROLLER_CONFIG	(CCD_DSP_CONTROLLER_CONFIG_BIT_CCD_REV3B| \
	CCD_DSP_CONTROLLER_CONFIG_BIT_TIM_REV4B|CCD_DSP_CONTROLLER_CONFIG_BIT_UTILITY_REV3| \
	CCD_DSP_CONTROLLER_CONFIG_BIT_SHUTTER|CCD_DSP_CONTROLLER_CONFIG_BIT_NONLINEAR_TEMP_CONV| \
	CCD_DSP_CONTROLLER_CONFIG_BIT_SUBARRAY|CCD_DSP_CONTROLLER_CONFIG_BIT_BINNING| \
	CCD_DSP_CONTROLLER_CONFIG_BIT_SERIAL_SPLIT)

/* structures */
/**
 * Internal handle data structure.
 * <dl>
 * <dt>Text_Device_Filename</dt> <dd>Filename of file to write text data to.</dd>
 * <dt>Text_File_Ptr</dt> <dd>FILE pointer to open text file to write to.</dd>
 * </dl>
 * File pointer to where the prints should be sent to.
 * @see #TEXT_MAX_FILENAME_LENGTH
 */
struct CCD_Text_Handle_Struct
{
	char Text_Device_Filename[TEXT_MAX_FILENAME_LENGTH+1];
	FILE *Text_File_Ptr;
};

/**
 * Structure holding data that the PCI interface would normally know about. This includes
 * the driver request being processed, the HCVR value, values held in the argument registers.
 * <dl>
 * <dt>Ioctl_Request</dt> <dd>The ioctl request.</dd>
 * <dt>HCVR_Command</dt> <dd>The last value put in the HCVR.</dd>
 * <dt>HCTR_Register</dt> <dd>The last value put in the HCTR.</dd>
 * <dt>HSTR_Register</dt> <dd>The last value put in the Host Status Transfer Register.</dd>
 * <dt>Destination</dt> <dd>The last destination put into the board destination register. Note
 * 	this does not include the number of arguments, see below.</dd>
 * <dt>Argument_List</dt> <dd>The current values of the PCI argument registers. An array of length 
 * 	TEXT_ARGUMENT_COUNT.</dd>
 * <dt>Argument_Count</dt> <dd>The number of arguments in the argument list, 
 * 	set as part of setting a destination.</dd>
 * <dt>Reply</dt> <dd>What we think the reply value should be.</dd>
 * <dt>Controller_Config</dt> <dd>The current value of the PCI controller status register.</dd>
 * <dt>Exposure_Length</dt> <dd>The length of the exposure, in milliseconds.</dd>
 * <dt>Exposure_Start_Time</dt> <dd>The time the exposure was started.</dd>
 * <dt>Pause_Start_Time</dt> <dd>The time the last pause was started.</dd>
 * <dt>Buffer</dt> <dd>Pointer to a memory buffer used for image storage.</dd>
 * <dt>Buffer_Length</dt> <dd>The allocated size of Buffer, in bytes.</dd>
 * <dt>Readout_Progress</dt> <dd>The number of bytes currently read out by the CCD.</dd>
 * </dl>
 * @see #TEXT_ARGUMENT_COUNT
 */
struct Text_Struct
{
	int Ioctl_Request;
	int HCVR_Command;
	int HCTR_Register;
	int HSTR_Register;
	int Manual_Command;
	enum CCD_DSP_BOARD_ID Destination;
	int Argument_List[TEXT_ARGUMENT_COUNT];
	int Argument_Count;
	int Reply;
	int Controller_Config;
	int Exposure_Length;
	struct timespec Exposure_Start_Time;
	struct timespec Pause_Start_Time;
	unsigned short *Buffer;
	int Buffer_Length;
	int Readout_Progress;
};

/**
 * Structure that holds information related to HCVR (Host Command Vector Register) and manual commands.
 * <dl>
 * <dt>Command</dt> <dd>The HCVR or Manual Command number.</dd>
 * <dt>Name</dt> <dd>A textual name for the command.</dd>
 * <dt>Reply</dt> <dd>The default reply value for the command.</dd>
 * <dt>Function</dt> <dd>A function pointer to call to do some special processing (usually relating to setting
 * 	up the reply value in some way).</dd>
 * </dl>
 */
struct Text_Command_Struct
{
	int Command;
	char Name[64];
	int Reply;
	void (*Function)(CCD_Interface_Handle_T *handle);
};

/**
 * Structure that holds information on DSP memory locations and their value.
 * <dl>
 * <dt>Board_Id</dt> <dd>The board the memory location is on.</dd>
 * <dt>Mem_Space</dt> <dd>The memory space the memory location is on.</dd>
 * <dt>Address</dt> <dd>The address of the memory location.</dd>
 * <dt>Value</dt> <dd>The value contained at the memory location.</dd>
 * </dl>
 */
struct Memory_Struct
{
	int Board_Id;
	int Mem_Space;
	int Address;
	int Value;
};

/* external variables */

/* internal routines */
static void Text_Print_Reply(CCD_Interface_Handle_T *handle);
static void Text_HCVR(CCD_Interface_Handle_T *handle,int hcvr_command);
static void Text_HSTR(void);
static void Text_Readout_Progress(void);
static void Text_Manual(CCD_Interface_Handle_T *handle,int manual_command);
static void Text_Destination(CCD_Interface_Handle_T *handle,int destination_number);
static void Text_Manual_Read_Controller_Config(CCD_Interface_Handle_T *handle);
static void Text_Manual_Test_Data_Link(CCD_Interface_Handle_T *handle);
static void Text_Manual_Read_Memory(CCD_Interface_Handle_T *handle);
static void Text_Manual_Read_Exposure_Time(CCD_Interface_Handle_T *handle);
static void Text_Manual_Set_Exposure_Time(CCD_Interface_Handle_T *handle);
static void Text_Manual_Start_Exposure(CCD_Interface_Handle_T *handle);
static void Text_Manual_Pause_Exposure(CCD_Interface_Handle_T *handle);
static void Text_Manual_Resume_Exposure(CCD_Interface_Handle_T *handle);

/* local variables */
/**
 * Variable holding error code of last operation performed by ccd_text.
 */
static int Text_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Text_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";
/**
 * Local variable for deciding how detailed the print information is.
 */
static enum CCD_TEXT_PRINT_LEVEL Text_Print_Level = CCD_TEXT_PRINT_LEVEL_COMMANDS;
/**
 * Local variable holding data normally held by the PCI registers.
 * @see #Text_Struct
 */
static struct Text_Struct Text_Data;
/**
 * A list of all the HCVR commands the text driver can process. A Text description is given, the
 * default reply value to set the reply buffer to, and a function pointer to call for cases where the
 * return value must be calculated in some way.
 * @see #Text_Command_Struct
 * @see #HCVR_COMMAND_COUNT
 */
static struct Text_Command_Struct Text_HCVR_Command_List[] = 
{
	{CCD_PCI_HCVR_RESET_CONTROLLER,"Reset Controller",CCD_DSP_SYR,NULL},
	{CCD_PCI_HCVR_PCI_PC_RESET,"Reset PCI board Program Counter",CCD_DSP_DON,NULL},
	{CCD_PCI_HCVR_SET_BIAS_VOLTAGES,"Set Bias Voltages",CCD_DSP_DON,NULL},
	{CCD_PCI_HCVR_ABORT_READOUT,"Abort Readout",CCD_DSP_DON,NULL},
};

/**
 * Hash Definition with the count of HCVR commands in the Text_HCVR_Command_List.
 * @see #Text_HCVR_Command_List
 */
#define HCVR_COMMAND_COUNT (sizeof(Text_HCVR_Command_List)/sizeof(Text_HCVR_Command_List[0]))
/**
 * A list of all the Manual commands the text driver can process. A Text description is given, the
 * default reply value to set the reply buffer to, and a function pointer to call for cases where the
 * return value must be calculated in some way.
 * @see #Text_Command_Struct
 * @see #MANUAL_COMMAND_COUNT
 */
static struct Text_Command_Struct Text_Manual_Command_List[] = 
{
	{CCD_DSP_AEX,"Abort Exposure",CCD_DSP_DON,NULL},
	{CCD_DSP_CLR,"Clear Array",CCD_DSP_DON,NULL},
	{CCD_DSP_CSH,"Close Shutter",CCD_DSP_DON,NULL},
	{CCD_DSP_IDL,"Resume Idling",CCD_DSP_DON,NULL},
	{CCD_DSP_LDA,"Load Application",CCD_DSP_DON,NULL},
	{CCD_DSP_OSH,"Open Shutter",CCD_DSP_DON,NULL},
	{CCD_DSP_POF,"Power Off",CCD_DSP_DON,NULL},
	{CCD_DSP_PEX,"Pause Exposure",CCD_DSP_DON,Text_Manual_Pause_Exposure},
	{CCD_DSP_PON,"Power On",CCD_DSP_DON,NULL},
	{CCD_DSP_RCC,"Read Controller Status",0,Text_Manual_Read_Controller_Config},
	{CCD_DSP_RDM,"Read Memory",0,Text_Manual_Read_Memory},
	{CCD_DSP_REX,"Resume Exposure",CCD_DSP_DON,Text_Manual_Resume_Exposure},
	{CCD_DSP_RET,"Read Exposure Time",0,Text_Manual_Read_Exposure_Time},
	{CCD_DSP_SET,"Set Exposure Time",CCD_DSP_DON,Text_Manual_Set_Exposure_Time},
	{CCD_DSP_SEX,"Start Exposure",CCD_DSP_DON,Text_Manual_Start_Exposure},
	{CCD_DSP_SGN,"Set Gain",CCD_DSP_DON,NULL},
	{CCD_DSP_SOS,"Set Output Source",CCD_DSP_DON,NULL},
	{CCD_DSP_SSP,"Set Subarray Position",CCD_DSP_DON,NULL},
	{CCD_DSP_SSS,"Set Subarray Size",CCD_DSP_DON,NULL},
	{CCD_DSP_STP,"Stop Idling",CCD_DSP_DON,NULL},
	{CCD_DSP_TDL,"Test Data Link",0,Text_Manual_Test_Data_Link},
	{CCD_DSP_WRM,"Write Memory",CCD_DSP_DON,NULL}
};

/**
 * Hash Definition with the count of Manual commands in the Text_Manual_Command_List.
 * @see #Text_Manual_Command_List
 */
#define MANUAL_COMMAND_COUNT (sizeof(Text_Manual_Command_List)/sizeof(Text_Manual_Command_List[0]))

/**
 * A list of DSP memory locations and the values in them. This is queried by commands such as Read Memory
 * to return sensible reply value for read memory requests.
 * @see #Memory_Struct
 */
static struct Memory_Struct Memory_List[] =
{
	{CCD_DSP_INTERFACE_BOARD_ID,CCD_DSP_MEM_SPACE_X,1,1},
	{CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,0x2,0xc60}, /* Heater ADUs from dewar */
	{CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,0x7,0xb1f}, /* Thermistor ADUs from utility board */
	{CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,0x8,0xcdf}, /* High voltage (+36v) power supply monitor (2051/0x803=failure) */
	{CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,0x9,0xdd0}, /* Low Voltage (+15v) power supply monitor (2051/0x803=failure) */
	{CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,0xa,0x245}, /* Low Voltage (-15v) power supply monitor (2051/0x803=failure) */
	{CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,0xc,0xaf0} /* Thermistor ADUs from dewar:2800 - -61 C */
};

/**
 * Hash Definition with the count of Memory locations in the Memory_List.
 * @see #Memory_List
 */
#define MEMORY_COUNT (sizeof(Memory_List)/sizeof(Memory_List[0]))

/* -------------------------------------------------------------------
** external functions 
** ------------------------------------------------------------------- */
/* device driver config functions */
/**
 * This routine is called to set the amount of information that gets printed out for all the data
 * that is received about a request.
 * @param level The level of printout to print - one of <a href="#CCD_TEXT_PRINT_LEVEL">CCD_TEXT_PRINT_LEVEL</a>:
 * 	CCD_TEXT_PRINT_LEVEL_COMMANDS,
 * 	CCD_TEXT_PRINT_LEVEL_REPLIES,
 * 	CCD_TEXT_PRINT_LEVEL_VALUES,
 * 	CCD_TEXT_PRINT_LEVEL_ALL.
 */
void CCD_Text_Set_Print_Level(enum CCD_TEXT_PRINT_LEVEL level)
{
	Text_Error_Number = 0;
	/* ensure level is a legal print level */
	if(!CCD_TEXT_IS_TEXT_PRINT_LEVEL(level))
	{
		Text_Error_Number = 5;
		sprintf(Text_Error_String,"CCD_Text_Set_Print_Level:Illegal value:level '%d'",level);
		CCD_Text_Error();
		return;
	}
	Text_Print_Level = level;
}

/* device driver implementation functions */
/**
 * This routine should be called at startup. 
 * In a real driver it will initialise the connection information ready for the device to be opened.
 * This routine just prints a message and initialises the Text_Data structure. It initialises the Text_File_Ptr
 * if it has not already been initialised by a call to CCD_Text_Set_File_Pointer.
 * @see ccd_interface.html#CCD_Interface_Initialise
 * @see #Text_Data
 * @see #Text_File_Ptr
 * @see #CCD_Text_Set_File_Pointer
 */
void CCD_Text_Initialise(void)
{
	int i;

	Text_Error_Number = 0;
	Text_Data.Ioctl_Request = 0;
	Text_Data.HCVR_Command = 0;
	Text_Data.HCTR_Register = 0;
	Text_Data.HSTR_Register = 0;
	Text_Data.Manual_Command = 0;
	Text_Data.Destination = 0;
	Text_Data.Argument_Count = 0;
	for(i=0;i<TEXT_ARGUMENT_COUNT;i++)
		Text_Data.Argument_List[i] = 0;
	Text_Data.Reply = -1;
	Text_Data.Controller_Config = TEXT_DEFAULT_CONTROLLER_CONFIG;
	Text_Data.Readout_Progress = 0;
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Text_Initialise:%s.\n",rcsid);
}

/**
 * This routine is called to open the device for communication. In this driver it just
 * prints out a message. 
 * @param device_pathname The pathname of the device we are trying to talk to.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return Returns TRUE if a device can be opened, otherwise it returns FALSE. Currently, the device can
 * 	always be opened.
 * @see #TEXT_MAX_FILENAME_LENGTH
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Open
 */
int CCD_Text_Open(char *device_pathname,CCD_Interface_Handle_T *handle)
{
	Text_Error_Number = 0;
	/* check parameters */
	if(device_pathname == NULL)
	{
		Text_Error_Number = 7;
		sprintf(Text_Error_String,"CCD_Text_Open failed:device_pathname was NULL.");
		return FALSE;
	}
	if(handle == NULL)
	{
		Text_Error_Number = 8;
		sprintf(Text_Error_String,"CCD_Text_Open failed:handle was NULL.");
		return FALSE;
	}
	if(strlen(device_pathname) > TEXT_MAX_FILENAME_LENGTH)
	{
		Text_Error_Number = 9;
		sprintf(Text_Error_String,"CCD_Text_Open failed:handle was NULL.");
		return FALSE;
	}
	/* Allocate Text block */
	handle->Handle.Text = (struct CCD_Text_Handle_Struct *)malloc(sizeof(struct CCD_Text_Handle_Struct));
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 10;
		sprintf(Text_Error_String,"CCD_Text_Open failed:Failed to allocate handle memory.");
		return FALSE;
	}
	/* try to open the device */
	strcpy(handle->Handle.Text->Text_Device_Filename,device_pathname);
	Text_Data.Buffer = NULL;
	Text_Data.Buffer_Length = 0;
	handle->Handle.Text->Text_File_Ptr = fopen(handle->Handle.Text->Text_Device_Filename,"a+");
	if(handle->Handle.Text->Text_File_Ptr == NULL)
	{
		Text_Error_Number = 11;
		sprintf(Text_Error_String,"CCD_Text_Open failed:Failed to open '%s' for appending.",
			handle->Handle.Text->Text_Device_Filename);
		return FALSE;
	}
	if(Text_Print_Level == CCD_TEXT_PRINT_LEVEL_ALL)
		fprintf(handle->Handle.Text->Text_File_Ptr,"CCD_Text_Open\n");
	return TRUE;
}

/**
 * Routine to create a memory map for image download. This is done using malloc, as we are only emulating
 * the interface.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param buffer_size The size of the buffer, in bytes.
 * @return Return TRUE if buffer initialisation is successful, FALSE if it wasn't.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Text_Memory_Map(CCD_Interface_Handle_T *handle,int buffer_size)
{
	if(handle == NULL)
	{
		Text_Error_Number = 12;
		sprintf(Text_Error_String,"CCD_Text_Memory_Map failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 13;
		sprintf(Text_Error_String,"CCD_Text_Memory_Map failed:handle Text pointer was NULL.");
		return FALSE;
	}
	if(buffer_size <= 0)
	{
		Text_Error_Number = 3;
		sprintf(Text_Error_String,"CCD_Text_Memory_Map failed:Illegal buffer size %d.",buffer_size);
		return FALSE;
	}
	/* diddly think about putting Buffer/Buffer_Length inside the Text handle, to allow multiple connections */
	Text_Data.Buffer_Length = buffer_size;
	Text_Data.Buffer = (unsigned short *)malloc(Text_Data.Buffer_Length);
	if(Text_Data.Buffer == NULL)
	{
		Text_Error_Number = 4;
		sprintf(Text_Error_String,"CCD_Text_Memory_Map:Memory allocation failed(%d).",Text_Data.Buffer_Length);
		return FALSE;
	}
	return TRUE;
}

/**
 * Routine to free a memory buffer for image download. This is done using free, as we are only emulating
 * the interface.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return Return TRUE if buffer initialisation is successful, FALSE if it wasn't.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Text_Memory_UnMap(CCD_Interface_Handle_T *handle)
{
	if(handle == NULL)
	{
		Text_Error_Number = 14;
		sprintf(Text_Error_String,"CCD_Text_Memory_UnMap failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 15;
		sprintf(Text_Error_String,"CCD_Text_Memory_UnMap failed:handle Text pointer was NULL.");
		return FALSE;
	}
	if(Text_Data.Buffer == NULL)
	{
		Text_Error_Number = 6;
		sprintf(Text_Error_String,"CCD_Text_Memory_UnMap:Buffer was NULL(%d).",Text_Data.Buffer_Length);
		return FALSE;
	}
	free(Text_Data.Buffer);
	Text_Data.Buffer = NULL;
	Text_Data.Buffer_Length = 0;
	return TRUE;
}

/**
 * This routine will send a command to the controller. 
 * This driver just prints out the command to be sent. The argument contains the data element associated with
 * this IOCTL command. Any reply value generated is passed back using the argument. 
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param request The type of request sent.
 * @param argument A pointer to the argument(s) to the request. On entry to the routine this should contain
 * 	the address of an integer containing any parameter data. On leaving this routine, any reply value
 * 	generated as a result of this command will be put into this integer.
 * @return Returns TRUE if the command was sent to the device driver successfully, FALSE if an error occured.
 * 	In this driver it always return TRUE.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Command
 * @see #Text_Print_Reply
 * @see #Text_HSTR
 * @see #Text_Readout_Progress
 * @see #Text_HCVR
 * @see #Text_Data
 * @see #Text_File_Ptr
 * @see #Text_Print_Level
 */
int CCD_Text_Command(CCD_Interface_Handle_T *handle,int request,int *argument)
{
	Text_Error_Number = 0;
	if(handle == NULL)
	{
		Text_Error_Number = 16;
		sprintf(Text_Error_String,"CCD_Text_Command failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 17;
		sprintf(Text_Error_String,"CCD_Text_Command failed:handle Text pointer was NULL.");
		return FALSE;
	}
	if(Text_Print_Level == CCD_TEXT_PRINT_LEVEL_ALL)
	{
		/* some command arguments have interdetminate arguments 
		** - the argument is not used or is filled in with a reply */
		if((request==CCD_PCI_IOCTL_GET_HCTR))
			fprintf(handle->Handle.Text->Text_File_Ptr,"ioctl(%#x,indeterminate)\n",request);
		else if(argument != NULL)
			fprintf(handle->Handle.Text->Text_File_Ptr,"ioctl(%#x,%#x)\n",request,*argument);
		else
			fprintf(handle->Handle.Text->Text_File_Ptr,"ioctl(%#x,NULL)\n",request);
	}
/* set Text_Data Ioctl_Request */
	Text_Data.Ioctl_Request = request;
	switch(request)
	{
		case CCD_PCI_IOCTL_GET_HCTR:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Get Host Control Register:");
			if(argument != NULL)
				Text_Data.Reply = Text_Data.HCTR_Register;
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"HCTR not filled in:argument was NULL:");
			break;
		case CCD_PCI_IOCTL_GET_PROGRESS:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Get Readout Progress:");
			Text_Readout_Progress();
			if(argument != NULL)
				Text_Data.Reply = Text_Data.Readout_Progress;
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,
					"Readout Progress not filled in:argument was NULL:");
			break;
		case CCD_PCI_IOCTL_GET_HSTR:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Get Host Status Transfer Register:");
			Text_HSTR();
			if(argument != NULL)
				Text_Data.Reply = Text_Data.HSTR_Register;
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"HSTR not filled in:argument was NULL:");
			break;
		case CCD_PCI_IOCTL_GET_DMA_ADDR:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Get DMA Address:");
			break;
		case CCD_PCI_IOCTL_SET_HCTR:
			fprintf(handle->Handle.Text->Text_File_Ptr,
				"Request:Set HCTR (Host Interface Control Register):");
			if(argument != NULL)
			{
				if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_VALUES)
					fprintf(handle->Handle.Text->Text_File_Ptr,"%#x:",(*argument));
				Text_Data.HCTR_Register = *argument;
			}
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"NULL Argument:");
			break;
		case CCD_PCI_IOCTL_SET_HCVR:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Set HCVR (Host Command Vector Register):");
			Text_Data.HCVR_Command = *argument;
			if(argument != NULL)
				Text_HCVR(handle,*argument);
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"NULL Argument:");
			break;
		case CCD_PCI_IOCTL_HCVR_DATA:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Set HCVR data:");
			if(argument != NULL)
			{
				Text_Data.Argument_List[0] = (*argument);
				if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_VALUES)
					fprintf(handle->Handle.Text->Text_File_Ptr,"%#x:",(*argument));
			/* HCVR_DATA does not return a reply. So we set the Text_Data.Reply to the input argument,
			** so that it is not changed. */
				Text_Data.Reply = (*argument);
			}
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"NULL Argument:");
			break;
		case CCD_PCI_IOCTL_COMMAND:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Command:");
			break;
		case CCD_PCI_IOCTL_PCI_DOWNLOAD:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:PCI Download:");
			if(argument != NULL)
			{
				Text_Data.Argument_List[0] = (*argument);
				if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_VALUES)
					fprintf(handle->Handle.Text->Text_File_Ptr,"%#x:",(*argument));
			}
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"NULL Argument:");
			break;
		case CCD_PCI_IOCTL_PCI_DOWNLOAD_WAIT:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:PCI Download Wait:");
			if(argument != NULL)
			{
				Text_Data.Argument_List[0] = (*argument);
				if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_VALUES)
					fprintf(handle->Handle.Text->Text_File_Ptr,"%#x:",(*argument));
				Text_Data.Reply = CCD_DSP_DON;
			}
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"NULL Argument:");
			break;
		case CCD_PCI_IOCTL_SET_IMAGE_BUFFERS:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Set address of the image data buffers:");
			break;
		case CCD_PCI_IOCTL_SET_UTIL_OPTIONS:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Set Utility options:");
			if(argument != NULL)
			{
				if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_VALUES)
					fprintf(handle->Handle.Text->Text_File_Ptr,"%d:",(*argument));
				Text_Data.Reply = CCD_DSP_DON;
			}
			else
				fprintf(handle->Handle.Text->Text_File_Ptr,"NULL Argument:");
			break;
		default:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Unknown Request");
			break;
	}
/* reply is passed back in argument - copy any set from Text_Data.Reply */
	if(argument != NULL)
	{
		(*argument) = Text_Data.Reply;
		Text_Print_Reply(handle);
	}
	fprintf(handle->Handle.Text->Text_File_Ptr,"\n");
	fflush(handle->Handle.Text->Text_File_Ptr);
	return TRUE;
}

/**
 * This routine will send a command to the SDSU controller boards. 
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param request The type of request sent.
 * @param argument_list The list of arguments to be sent to the controller.
 * @param argument_count The number of arguments in the argument_list.
 * @return Returns TRUE if the command was sent to the device driver successfully, FALSE if an error occured.
 * 	In this driver it always return TRUE.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Command_List
 * @see #Text_Destination
 * @see #Text_Manual
 * @see #Text_Print_Reply
 */
int CCD_Text_Command_List(CCD_Interface_Handle_T *handle,int request,int *argument_list,int argument_count)
{
	int i;

	Text_Error_Number = 0;
	if(handle == NULL)
	{
		Text_Error_Number = 18;
		sprintf(Text_Error_String,"CCD_Text_Command_List failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 19;
		sprintf(Text_Error_String,"CCD_Text_Command_List failed:handle Text pointer was NULL.");
		return FALSE;
	}
	if(Text_Print_Level == CCD_TEXT_PRINT_LEVEL_ALL)
	{
		/* some command arguments have interdetminate arguments 
		** - the argument is not used or is filled in with a reply */
		if((request==CCD_PCI_IOCTL_GET_HCTR))
			fprintf(handle->Handle.Text->Text_File_Ptr,"ioctl(%#x,indeterminate)\n",request);
		else if(argument_count > 0)
		{
			fprintf(handle->Handle.Text->Text_File_Ptr,"ioctl(%#x",request);
			for(i=0;i< argument_count;i++)
			{
				fprintf(handle->Handle.Text->Text_File_Ptr,",%#x",argument_list[i]);
			}
			fprintf(handle->Handle.Text->Text_File_Ptr,")\n");
		}
		else
			fprintf(handle->Handle.Text->Text_File_Ptr,"ioctl(%#x,NULL)\n",request);
	}
/* set Text_Data Ioctl_Request */
	Text_Data.Ioctl_Request = request;
	switch(request)
	{
		case CCD_PCI_IOCTL_COMMAND:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Request:Command:");
			if(argument_count > 0)
				Text_Destination(handle,argument_list[0]);
		/* Copy arguments.
		** Loop starts from 2, first 2 CCD_PCI_IOCTL_COMMAND arguments are header word and 
		** Manual Command itself. */
			Text_Data.Argument_Count = argument_count-2;
			for(i=2;i<argument_count;i++)
			{
				Text_Data.Argument_List[i-2] = argument_list[i];
			}
		/* Call manual command routine */
			if(argument_count > 1)
				Text_Manual(handle,argument_list[1]);
		/* put reply value in argument_list[0] */
			argument_list[0] = Text_Data.Reply;
			Text_Print_Reply(handle);
			break;
		default:
			fprintf(handle->Handle.Text->Text_File_Ptr,"Unknown Request");
			break;
	}
	fprintf(handle->Handle.Text->Text_File_Ptr,"\n");
	fflush(handle->Handle.Text->Text_File_Ptr);
	return TRUE;
}

/**
 * This routine emulates getting reply data from the SDSU CCD Controller. The reply data is stored in
 * the data parameter, up to byte_count bytes of it. This allows the routine to read an arbitary amount of data 
 * (an image for instance).
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param data The address of an unsigned short pointer, which on return from this routine will point to
 *        an area of memory containing the read out CCD image. 
 *        In this routine the image is filled with bytes that are ASCII characters so the resultant image 
 *        can be printed out.
 * @return The routine returns TRUE on success and FALSE on failure.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Get_Reply_Data
 */
int CCD_Text_Get_Reply_Data(CCD_Interface_Handle_T *handle,unsigned short **data)
{
	int i;

	Text_Error_Number = 0;
	if(handle == NULL)
	{
		Text_Error_Number = 24;
		sprintf(Text_Error_String,"CCD_Text_Get_Reply_Data failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 20;
		sprintf(Text_Error_String,"CCD_Text_Get_Reply_Data failed:handle Text pointer was NULL.");
		return FALSE;
	}
	if(Text_Print_Level == CCD_TEXT_PRINT_LEVEL_ALL)
		fprintf(handle->Handle.Text->Text_File_Ptr,"CCD_Text_Reply_Data\n");
	/* if data is null we can't put information in it */
	if(data == NULL)
	{
		Text_Error_Number = 1;
		sprintf(Text_Error_String,"CCD_Text_Get_Reply_Data:data is NULL");
		return FALSE;
	}
	if(Text_Data.Buffer == NULL)
	{
		Text_Error_Number = 2;
		sprintf(Text_Error_String,"CCD_Text_Get_Reply_Data:Reply Buffer is NULL");
		return FALSE;
	}
	/* fill data with return values */
	(*data) = (unsigned short *)(Text_Data.Buffer);
	i=0;
	while((i<(Text_Data.Buffer_Length/sizeof(unsigned short)))&&(!CCD_DSP_Get_Abort()))
	{
		(*data)[i] = (i%((1<<16)-1));
		i++;
	}
	fprintf(handle->Handle.Text->Text_File_Ptr,"CCD_Text_Get_Reply_Data:%d.\n",Text_Data.Buffer_Length);
	return TRUE;
}

/**
 * This routine will close the interface.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return The routine returns the return value from the close routine it called. This will normally be TRUE
 * 	if the device was successfully closed, or FALSE if it failed in some way. In this device, it always
 * 	returns TRUE.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Close
 */
int CCD_Text_Close(CCD_Interface_Handle_T *handle)
{
	int error_number;
	int retval;

	Text_Error_Number = 0;
	if(handle == NULL)
	{
		Text_Error_Number = 21;
		sprintf(Text_Error_String,"CCD_Text_Close failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.Text == NULL)
	{
		Text_Error_Number = 22;
		sprintf(Text_Error_String,"CCD_Text_Close failed:handle Text pointer was NULL.");
		return FALSE;
	}
	if(Text_Print_Level == CCD_TEXT_PRINT_LEVEL_ALL)
		fprintf(handle->Handle.Text->Text_File_Ptr,"CCD_Text_Close\n");
	retval = fclose(handle->Handle.Text->Text_File_Ptr);
	if(retval < 0)
	{
		error_number = errno;
		Text_Error_Number = 23;
		sprintf(Text_Error_String,"CCD_Text_Close failed:fclose returned %d(%d).",retval,error_number);
		return FALSE;
	}
	free(handle->Handle.Text);
	handle->Handle.Text = NULL;
	return TRUE;
}

/**
 * Get the current value of ccd_text's error number.
 * @return The current value of ccd_text's error number.
 */
int CCD_Text_Get_Error_Number(void)
{
	return Text_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_text in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Text_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Text_Error_Number == 0)
		sprintf(Text_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Text:Error(%d) : %s\n",time_string,Text_Error_Number,Text_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_text in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Text_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Text_Error_Number == 0)
		sprintf(Text_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Text:Error(%d) : %s\n",time_string,
		Text_Error_Number,Text_Error_String);
}

/**
 * The warning routine that reports any warnings occuring in ccd_text in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Text_Warning(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an warning message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an warning to display */
	if(Text_Error_Number == 0)
		sprintf(Text_Error_String,"Logic Error:No Warning defined");
	fprintf(stderr,"%s CCD_Text:Warning(%d) : %s\n",time_string,Text_Error_Number,Text_Error_String);
}

/* -------------------------------------------------------------------
** 	Internal routines 
** ------------------------------------------------------------------- */
/**
 * Routine that prints out a textual representation of Text_Data.Reply,
 * if Text_Print_Level is greater than or equal to CCD_TEXT_PRINT_LEVEL_REPLIES.
 * The spacial case return values CCD_DSP_DON, CCD_DSP_ERR and
 * CCD_DSP_SYR are checked for special printouts.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Print_Level
 * @see #Text_Data
 * @see #CCD_TEXT_PRINT_LEVEL_REPLIES
 * @see ccd_dsp.html#CCD_DSP_DON
 * @see ccd_dsp.html#CCD_DSP_ERR
 * @see ccd_dsp.html#CCD_DSP_SYR
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Print_Reply(CCD_Interface_Handle_T *handle)
{
/* if it's a standard reply print out a text representation. */
	if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_REPLIES)
	{
		switch(Text_Data.Reply)
		{
			case CCD_DSP_DON:
				fprintf(handle->Handle.Text->Text_File_Ptr,"DON:");
				break;
			case CCD_DSP_ERR:
				fprintf(handle->Handle.Text->Text_File_Ptr,"ERR:");
				break;
			case CCD_DSP_SYR:
				fprintf(handle->Handle.Text->Text_File_Ptr,"SYR:");
				break;
			default:
				fprintf(handle->Handle.Text->Text_File_Ptr,"%#x:",Text_Data.Reply);
				break;
		}/* end switch on reply value */
	}/* end if printing replies */
}

/**
 * Internal routine which prints information about the HCVR command passed in.
 * This uses the Text_HCVR_Command_List to determines what the command is.
 * It also performs any operations as a result of this command using a function pointer in the list.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param hcvr_command The value to put into the HCVR register.
 * @see #Text_HCVR_Command_List
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_HCVR(CCD_Interface_Handle_T *handle,int hcvr_command)
{
	int i,found;

	i=0;
	found = FALSE;
	while((i<HCVR_COMMAND_COUNT)&&(found == FALSE))
	{
		found = (Text_HCVR_Command_List[i].Command == hcvr_command);
		if(!found)
			i++;
	}
	if(found)
	{
		if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_COMMANDS)
			fprintf(handle->Handle.Text->Text_File_Ptr,":%s:",Text_HCVR_Command_List[i].Name);
		Text_Data.Reply = Text_HCVR_Command_List[i].Reply;
		if(Text_HCVR_Command_List[i].Function != NULL)
			Text_HCVR_Command_List[i].Function(handle);
	}
	else
		fprintf(handle->Handle.Text->Text_File_Ptr,":Unknown HCVR command:");
}

/**
 * This routine is called whenever the ioctl command GET_HSTR is called.
 * It allows us to calculate the value of Text_Data.HSTR_Register, i.e. when to switch to readout mode.
 * @see #Text_Manual_Read_Exposure_Time
 * @see ccd_exposure.html#CCD_EXPOSURE_HSTR_READOUT
 * @see ccd_exposure.html#CCD_EXPOSURE_HSTR_BIT_SHIFT
 */
static void Text_HSTR(void)
{
	int elapsed_exposure_time;

	/* call routine to get current elapsed exposure time - result put in Text_Data.Reply */
	Text_Manual_Read_Exposure_Time(NULL);
	elapsed_exposure_time = Text_Data.Reply;
	/* if elapsed exposure time > exposure length, go into readout mode. */
	if(elapsed_exposure_time> Text_Data.Exposure_Length)
		Text_Data.HSTR_Register |= (CCD_EXPOSURE_HSTR_READOUT<<CCD_EXPOSURE_HSTR_BIT_SHIFT);
}

/**
 * This routine is called whenever the ioctl command GET_PROGRESS is called.
 * This allows us to modify the Text_Data.Readout_Progress field to simulate readout.
 */
static void Text_Readout_Progress(void)
{
	/* check we are in readout, i.e. the exposure has finished... 
       ** as GET_PROGRESS now called even when exposure underway, for readouts less than 1 second. */
	if(((Text_Data.HSTR_Register>>CCD_EXPOSURE_HSTR_BIT_SHIFT)&CCD_EXPOSURE_HSTR_READOUT) == 
	   CCD_EXPOSURE_HSTR_READOUT)
	{
		/* read out 500000 bytes between calls, if we call GET_PROGRESS every second,
		** about a 10 second readout. */
		Text_Data.Readout_Progress = Text_Data.Readout_Progress + 500000;
	}
	else
		Text_Data.Readout_Progress = 0;
}

/**
 * Internal routine which prints information about the Manual command passed in.
 * This uses the Text_Manual_Command_List to determines what the command is.
 * It also performs any operations as a result of this command using a function pointer in the list.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param manual_command The value to put into the Manual register.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #Text_Manual_Command_List
 */
static void Text_Manual(CCD_Interface_Handle_T *handle,int manual_command)
{
	int i,found;

	i=0;
	found = FALSE;
	while((i<MANUAL_COMMAND_COUNT)&&(found == FALSE))
	{
		found = (Text_Manual_Command_List[i].Command == manual_command);
		if(!found)
			i++;
	}
	if(found)
	{
		if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_COMMANDS)
			fprintf(handle->Handle.Text->Text_File_Ptr,":%s:",Text_Manual_Command_List[i].Name);
		Text_Data.Reply = Text_Manual_Command_List[i].Reply;
		if(Text_Manual_Command_List[i].Function != NULL)
			Text_Manual_Command_List[i].Function(handle);
	}
	else
		fprintf(handle->Handle.Text->Text_File_Ptr,":Unknown Manual command:");
}

/**
 * Routine to convert a manual command header word into a textual description string.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param destination_number The first argument to the CCD_PCI_IOCTL_COMMAND ioctl request.
 * @see ccd_pci.html#CCD_PCI_IOCTL_COMMAND
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Destination(CCD_Interface_Handle_T *handle,int destination_number)
{
	char *Board_Name_List[] = {"Host","Interface","Timing board","Utility board"};
	int Board_Name_Count = 4;

	Text_Data.Destination = (destination_number >> 8)&0xFF;
	Text_Data.Argument_Count = destination_number&0xFF;
	Text_Data.Reply = CCD_DSP_DON;
	if(Text_Print_Level >= CCD_TEXT_PRINT_LEVEL_VALUES)
	{
		if((Text_Data.Destination > 0)&&(Text_Data.Destination<Board_Name_Count))
		{
			fprintf(handle->Handle.Text->Text_File_Ptr,":%s:Number of Arguments:%d:",
				Board_Name_List[Text_Data.Destination],Text_Data.Argument_Count);
		}
		else
		{
			fprintf(handle->Handle.Text->Text_File_Ptr,":UNKNOWN BOARD %d:Number of Arguments:%d:",
				Text_Data.Destination,Text_Data.Argument_Count);
		}
	}
}

/**
 * Function invoked from Text_Manual when a RCC command is sent to the driver.
 * This retrieves the controller configuration word.
 * This function sets Text_Data.Reply to Text_Data.Controller_Config.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Manual
 * @see ccd_dsp.html#CCD_DSP_RCC
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Read_Controller_Config(CCD_Interface_Handle_T *handle)
{
	Text_Data.Reply = Text_Data.Controller_Config;
}

/**
 * Function invoked from Text_Manual when a TDL command is sent to the driver.
 * This tests the driver by sending argument 1 to the required board, which should return the value.
 * Hence this function sets Text_Data.Reply to Text_Data.Argument_List[0].
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Manual
 * @see ccd_dsp.html#CCD_DSP_TDL
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Test_Data_Link(CCD_Interface_Handle_T *handle)
{
	Text_Data.Reply = Text_Data.Argument_List[0];
}

/**
 * Routine invoked from Text_Manual when a Read Memory command is sent to the driver.
 * This routine needs to get the relevant memory address we are reading (board/memory space/address)
 * and return a suitable value for some cases. This uses the Memory_List defined above.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Manual
 * @see #Memory_List
 * @see ccd_dsp.html#CCD_DSP_RDM
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Read_Memory(CCD_Interface_Handle_T *handle)
{
	int i,memory_space,address;

	memory_space = Text_Data.Argument_List[0] & 0xf00000;
	address = Text_Data.Argument_List[0] & 0xfffff;
	fprintf(handle->Handle.Text->Text_File_Ptr,
		"Text_Manual_Read_Memory:Destination = %#x:Memory Space = %#x:Address = %#x\n",
		Text_Data.Destination,memory_space,address);
	for(i=0;i<MEMORY_COUNT;i++)
	{
		if((Text_Data.Destination == Memory_List[i].Board_Id)&&
			(memory_space == Memory_List[i].Mem_Space)&&
			(address == Memory_List[i].Address))
		{
			fprintf(handle->Handle.Text->Text_File_Ptr,"Text_Manual_Read_Memory:Match Found:Value = %#x\n",
				Memory_List[i].Value);
			Text_Data.Reply = Memory_List[i].Value;
		}
	}
}

/**
 * Function invoked from Text_Manual when a RET command is sent to the driver.
 * This should set the return argument to the elapsed time of exposure, in milliseconds.
 * Text_Data.Reply is set to the elapsed time, measured as the current time minus Text_Data.Exposure_Start_Time. 
 * However, if the exposure is currently paused Text_Data.Pause_Start_Time is non zero. In this case the 
 * current elapsed exposure time is the pause start time minus the exposure start time.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Manual
 * @see ccd_dsp.html#CCD_DSP_RET
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Read_Exposure_Time(CCD_Interface_Handle_T *handle)
{
	struct timespec current_time;
#ifndef _POSIX_TIMERS
	struct timeval gtod_current_time;
#endif
	long int elapsed_time;

#ifdef _POSIX_TIMERS
	clock_gettime(CLOCK_REALTIME,&current_time);
#else
	gettimeofday(&gtod_current_time,NULL);
	current_time.tv_sec = gtod_current_time.tv_sec;
	current_time.tv_nsec = gtod_current_time.tv_usec*TEXT_ONE_MICROSECOND_NS;
#endif
/* if we are currently paused */
	if(Text_Data.Pause_Start_Time.tv_sec > 0)
	{
		elapsed_time = (Text_Data.Pause_Start_Time.tv_sec-Text_Data.Exposure_Start_Time.tv_sec)*1000;
		elapsed_time += (Text_Data.Pause_Start_Time.tv_nsec-Text_Data.Exposure_Start_Time.tv_nsec)/1000000;
	}
	else
	{
		elapsed_time = (current_time.tv_sec-Text_Data.Exposure_Start_Time.tv_sec)*1000;
		/* voodoo waits until elapsed time returns zero before assuming timing has started.
		** This hack makes the first exposure time we return zero 
		** (assuming we request exposure time within one second of starting an exposure). */
		if(elapsed_time > 0)
			elapsed_time += (current_time.tv_nsec-Text_Data.Exposure_Start_Time.tv_nsec)/1000000;
	}
	Text_Data.Reply = elapsed_time;
}

/**
 * Invoked from Text_Manual when a SET (Set Exposure Time) command is sent to the driver.
 * Sets exposure time in Text_Data from argument list.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Data
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Set_Exposure_Time(CCD_Interface_Handle_T *handle)
{
	Text_Data.Exposure_Length = Text_Data.Argument_List[0];
}

/**
 * Function invoked from Text_Manual when a SEX command is sent to the driver.
 * Text_Data sets the reply value to CCD_DSP_DON.
 * We set the Text_Data.Exposure_Start_Time to the current time when the exposure started.
 * We also reset Text_Data.Pause_Start_Time to zero - we are starting an exposure - we can't be paused.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_Manual
 * @see ccd_dsp.html#CCD_DSP_SEX
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Start_Exposure(CCD_Interface_Handle_T *handle)
{
#ifndef _POSIX_TIMERS
	struct timeval gtod_current_time;
#endif

#ifdef _POSIX_TIMERS
	clock_gettime(CLOCK_REALTIME,&(Text_Data.Exposure_Start_Time));
#else
	gettimeofday(&gtod_current_time,NULL);
	Text_Data.Exposure_Start_Time.tv_sec = gtod_current_time.tv_sec;
	Text_Data.Exposure_Start_Time.tv_nsec = gtod_current_time.tv_usec*TEXT_ONE_MICROSECOND_NS;
#endif
/* reset pause time */
	Text_Data.Pause_Start_Time.tv_sec = 0;
	Text_Data.Pause_Start_Time.tv_nsec = 0;
/* sort out HSTR - switch off readout flags */
	Text_Data.HSTR_Register = 0;
/* re-initialise Readout_Progress, not started reading out yet. */
	Text_Data.Readout_Progress = 0;
}

/**
 * Function invoked from Text_HCVR when a PAUSE_EXPOSURE command is sent to the driver.
 * Text_Data sets the reply value to CCD_DSP_DON.
 * We set the Text_Data.Pause_Start_Time to the current time when the exposure started.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_HCVR
 * @see ccd_pci.html#CCD_PCI_HCVR_PAUSE_EXPOSURE
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Pause_Exposure(CCD_Interface_Handle_T *handle)
{
#ifndef _POSIX_TIMERS
	struct timeval gtod_current_time;
#endif

#ifdef _POSIX_TIMERS
	clock_gettime(CLOCK_REALTIME,&(Text_Data.Pause_Start_Time));
#else
	gettimeofday(&gtod_current_time,NULL);
	Text_Data.Pause_Start_Time.tv_sec = gtod_current_time.tv_sec;
	Text_Data.Pause_Start_Time.tv_nsec = gtod_current_time.tv_usec*TEXT_ONE_MICROSECOND_NS;
#endif
}

/**
 * Function invoked from Text_HCVR when a RESUME_EXPOSURE command is sent to the driver.
 * Text_Data sets the reply value to CCD_DSP_DON.
 * We get the current time and calculate the time elapsed from the Text_Data.Pause_Start_Time. We add the elapsed time
 * to the Text_Data.Exposure_Start_Time so that subsequent calls to get the elapsed exposure time do not
 * include the paused time. The Text_Data.Pause_Start_Time is reset.
 * Note the paused time is calculated only to the nearest second for simplicity.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @see #Text_HCVR
 * @see ccd_pci.html#CCD_PCI_HCVR_RESUME_EXPOSURE
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static void Text_Manual_Resume_Exposure(CCD_Interface_Handle_T *handle)
{
	struct timespec resume_time;
#ifndef _POSIX_TIMERS
	struct timeval gtod_current_time;
#endif
	time_t paused_time;

#ifdef _POSIX_TIMERS
	clock_gettime(CLOCK_REALTIME,&resume_time);
#else
	gettimeofday(&gtod_current_time,NULL);
	resume_time.tv_sec = gtod_current_time.tv_sec;
	resume_time.tv_nsec = gtod_current_time.tv_usec*TEXT_ONE_MICROSECOND_NS;
#endif
/* add amount of paused time to Exposure_Start_Time, so returned elapsed time is sensible */
	paused_time = resume_time.tv_sec - Text_Data.Pause_Start_Time.tv_sec;
	Text_Data.Exposure_Start_Time.tv_sec += paused_time;
/* reset pause time */
	Text_Data.Pause_Start_Time.tv_sec = 0;
	Text_Data.Pause_Start_Time.tv_nsec = 0;

}

/*
** $Log: not supported by cvs2svn $
** Revision 0.26  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 0.25  2006/05/16 14:14:09  cjm
** gnuify: Added GNU General Public License.
**
** Revision 0.24  2003/06/06 12:36:01  cjm
** Added VON/VOF emulation.
**
** Revision 0.23  2003/03/26 15:44:48  cjm
** Added Windowing emulation.
**
** Revision 0.22  2002/12/16 16:49:36  cjm
** Removed Error routines resetting error number to zero.
**
** Revision 0.21  2002/12/03 17:13:19  cjm
** Added fake pressure gauge ADUs.
**
** Revision 0.20  2002/11/07 19:13:39  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 0.19  2001/03/20 11:54:22  cjm
** Added conditional compilation on CCD_FILTER_WHEEL_INPUT_HOME,
** so that the emulated Y:DIG_IN digital input word has the correct bits set as though
** the filter wheels are in the home detent position, whether the library
** is using a proximity or magnetic home input sensor.
**
** Revision 0.18  2001/02/09 19:35:27  cjm
** Added defauly memory location values, so that filter wheel
** RDM's succeed.
**
** Revision 0.17  2001/01/30 12:37:30  cjm
** Added filter wheel manual command set.
**
** Revision 0.16  2000/09/25 09:51:28  cjm
** Changes to use with v1.4 SDSU DSP code.
**
** Revision 0.15  2000/07/06 09:33:43  cjm
** Changed GET_REPLY time delay.
**
** Revision 0.14  2000/06/19 08:48:34  cjm
** Backup.
**
** Revision 0.13  2000/06/09 16:06:20  cjm
** Added HCTR implementation.
** Added Controller status implementation.
** Removed Clear Start Time function/variable, now Text_Get_Reply sleeps for 5 seconds instead.
** Added SOS implementation.
**
** Revision 0.12  2000/05/12 15:29:19  cjm
** Fixed synthetic image output.
**
** Revision 0.11  2000/05/09 15:17:44  cjm
** Changed data values returned from CCD_Text_Get_Reply_Data.
**
** Revision 0.10  2000/04/13 13:11:35  cjm
** Added current time to error routines.
**
** Revision 0.10  2000/04/13 13:01:27  cjm
** Modified error routine to print current time.
**
** Revision 0.9  2000/03/20 11:45:20  cjm
** Added _POSIX_TIMERS feature test macros around calls to clock_gettime, to allow the
** library to compile under Linux. Note nanosleep should also be tested, but this exists under
** Linux so it is not a problem.
**
** Revision 0.8  2000/03/08 14:32:45  cjm
** Pause and resume exposure emulation added.
**
** Revision 0.7  2000/03/01 10:50:06  cjm
** Fixed Text_Get_Reply print for CLR case.
**
** Revision 0.6  2000/02/24 11:42:38  cjm
** Made exposure time depend on real time clock.
** Made Clear array take some time.
**
** Revision 0.5  2000/02/10 16:26:08  cjm
** Changed Text_Manual_Read_Exposure_Time so that it works with voodoo.
**
** Revision 0.4  2000/02/09 18:27:39  cjm
** Fixed ioctl indeterminate prints for CLEAR_REPLY and GET_REPLY.
**
** Revision 0.3  2000/01/28 16:19:39  cjm
** Added CCD_Text_Set_File_Pointer function.
** Changed implementation of Clear Reply Memory and others so that linking with voodoo
** produce required results.
**
** Revision 0.2  2000/01/26 14:02:39  cjm
** Added low level printout for regression testing against voodoo.
**
** Revision 0.1  2000/01/25 14:57:27  cjm
** initial revision (PCI version).
**
*/
