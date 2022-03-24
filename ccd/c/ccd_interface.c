/* ccd_interface.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_interface.c,v 1.1 2011-11-23 10:59:52 cjm Exp $
*/
/**
 * ccd_interface.c is a generic interface for communicating with the underlying hardware interface to the
 * SDSU CCD Controller hardware. A device is selected, then the generic routines in this module call the
 * interface specific routines to perform the task.
 * @author SDSU, Chris Mottram
 * @version $Revision: 1.1 $
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "log_udp.h"
#include "ccd_exposure.h"
#include "ccd_global.h"
#include "ccd_interface.h"
#include "ccd_text.h"
#include "ccd_pci.h"
#include "ccd_setup.h"
#include "ccd_interface_private.h"

/* internal structures */
/* internal variables */
/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_interface.c,v 1.1 2011-11-23 10:59:52 cjm Exp $";

/* external variables */

/* local variables */
/**
 * Variable holding error code of last operation performed by ccd_interface.
 */
static int Interface_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Interface_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";

/* external functions */
/**
 * This routine calls the setup routine for the device type implementors.
 * @see ccd_text.html#CCD_Text_Initialise
 * @see ccd_pci.html#CCD_PCI_Initialise
 */
void CCD_Interface_Initialise(void)
{
	Interface_Error_Number = 0;
	CCD_Text_Initialise();
	CCD_PCI_Initialise();
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Interface_Initialise:%s.\n",rcsid);
}

/**
 * This routine opens the interface for the device the library is currently using.
 * @param device_number The device the library will talk to. One of
 * <a href="#CCD_INTERFACE_DEVICE_ID">CCD_INTERFACE_DEVICE_ID</a>:
 * CCD_INTERFACE_DEVICE_NONE,
 * CCD_INTERFACE_DEVICE_TEXT or
 * CCD_INTERFACE_DEVICE_PCI.
 * @param device_pathname The pathname of the device we are trying to talk to.
 * @param handle The address of a pointer to a CCD_Interface_Handle_T to 
 *  store the device connection specific information into.
 * @return The routine returns the return value from the open routine it called. This will normally be TRUE
 * 	if the device was successfully opened, or FALSE if it failed in some way.
 * @see #CCD_INTERFACE_DEVICE_ID
 * @see #CCD_Interface_Handle_T
 * @see ccd_exposure.html#CCD_Exposure_Data_Initialise
 * @see ccd_text.html#CCD_Text_Open
 * @see ccd_pci.html#CCD_PCI_Open
 * @see ccd_setup.html#CCD_Setup_Data_Initialise
 */
int CCD_Interface_Open(enum CCD_INTERFACE_DEVICE_ID device_number,char *device_pathname,
			      CCD_Interface_Handle_T **handle)
{
	Interface_Error_Number = 0;
	/* check arguments */
	if(!CCD_INTERFACE_IS_INTERFACE_DEVICE(device_number))
	{
		Interface_Error_Number = 1;
		sprintf(Interface_Error_String,"CCD_Interface_Open:Illegal device '%d'.",
			device_number);
		return FALSE;
	}
	if(device_pathname == NULL)
	{
		Interface_Error_Number = 2;
		sprintf(Interface_Error_String,"CCD_Interface_Open:device_pathname was NULL.");
		return FALSE;
	}
	if(handle == NULL)
	{
		Interface_Error_Number = 10;
		sprintf(Interface_Error_String,"CCD_Interface_Open:handle was NULL.");
		return FALSE;
	}
	/* allocate handle */
	(*handle) = (CCD_Interface_Handle_T *)malloc(sizeof(CCD_Interface_Handle_T));
	if((*handle) == NULL)
	{
		Interface_Error_Number = 17;
		sprintf(Interface_Error_String,"CCD_Interface_Open:Failed to allocate handle.");
		return FALSE;
	}
	/* set the device type */
	(*handle)->Interface_Device = device_number;
	/* initialise setup and exposure data */
	CCD_Exposure_Data_Initialise((*handle));
        CCD_Setup_Data_Initialise((*handle));
#if LOGGING > 1
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Interface_Open() %s of type %d using handle %p.",
			      device_pathname,device_number,(*handle));
#endif
	/* call the device specific open routine */
	switch((*handle)->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			return CCD_Text_Open(device_pathname,(*handle));
		case CCD_INTERFACE_DEVICE_PCI:
			return CCD_PCI_Open(device_pathname,(*handle));
		default:
			Interface_Error_Number = 3;
			sprintf(Interface_Error_String,"CCD_Interface_Open failed:No device selected.");
			return FALSE;
	}
}

/**
 * This routine sorts out the memory mapping, or emulation, for the specified interface.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param buffer_size The size of the buffer, in bytes.
 * @return The routine returns TRUE if the operation was successfully completed, 
 *         or FALSE if it failed in some way.
 * @see #CCD_Interface_Handle_T
 * @see ccd_text.html#CCD_Text_Memory_Map
 * @see ccd_pci.html#CCD_PCI_Memory_Map
 */
int CCD_Interface_Memory_Map(CCD_Interface_Handle_T *handle,int buffer_size)
{
	Interface_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		Interface_Error_Number = 11;
		sprintf(Interface_Error_String,"CCD_Interface_Memory_Map:handle was NULL.");
		return FALSE;
	}
	/* call the device specific open routine */
	switch(handle->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			return CCD_Text_Memory_Map(handle,buffer_size);
		case CCD_INTERFACE_DEVICE_PCI:
			return CCD_PCI_Memory_Map(handle,buffer_size);
		default:
			Interface_Error_Number = 8;
			sprintf(Interface_Error_String,"CCD_Interface_Memory_Map failed:No device selected(%p,%d).",
				(void*)handle,handle->Interface_Device);
			return FALSE;
	}
}

/**
 * This routine frees the memory mapping, or emulation, for the specified interface.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return The routine returns TRUE if the operation was successfully completed, 
 *         or FALSE if it failed in some way.
 * @see #CCD_Interface_Handle_T
 * @see ccd_text.html#CCD_Text_Memory_UnMap
 * @see ccd_pci.html#CCD_PCI_Memory_UnMap
 */
int CCD_Interface_Memory_UnMap(CCD_Interface_Handle_T *handle)
{
	Interface_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		Interface_Error_Number = 12;
		sprintf(Interface_Error_String,"CCD_Interface_Memory_UnMap:handle was NULL.");
		return FALSE;
	}
	/* call the device specific open routine */
	switch(handle->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			return CCD_Text_Memory_UnMap(handle);
		case CCD_INTERFACE_DEVICE_PCI:
			return CCD_PCI_Memory_UnMap(handle);
		default:
			Interface_Error_Number = 9;
			sprintf(Interface_Error_String,"CCD_Interface_Memory_UnMap failed:No device selected(%p,%d).",
				(void*)handle,handle->Interface_Device);
			return FALSE;
	}
}

/**
 * This routine sends a request to the device the library is currently using. It is usually called from
 * <a href="ccd_dsp.html#DSP_Send_Command">DSP_Send_Command</a>.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param request The request number sent to the device.
 * @param argument The address of the data to send as a parameter to the request. Upon a successfull return from
 * 	the routine, the return value from the DSP code may be in the argument.
 * @return The routine returns the return value from the command routine it called. This will normally be TRUE
 * 	if the request was sent correctly, or FALSE if it failed in some way.
 * @see #CCD_Interface_Handle_T
 * @see ccd_text.html#CCD_Text_Command
 * @see ccd_pci.html#CCD_PCI_Command
 * @see ccd_dsp.html#DSP_Send_Command
 */
int CCD_Interface_Command(CCD_Interface_Handle_T *handle,int request,int *argument)
{
	Interface_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		Interface_Error_Number = 13;
		sprintf(Interface_Error_String,"CCD_Interface_Command:handle was NULL.");
		return FALSE;
	}
	/* call the device specific command routine */
	switch(handle->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			return CCD_Text_Command(handle,request,argument);
		case CCD_INTERFACE_DEVICE_PCI:
			return CCD_PCI_Command(handle,request,argument);
		default:
			Interface_Error_Number = 4;
			sprintf(Interface_Error_String,"CCD_Interface_Command failed:No device selected(%p,%d).",
				(void*)handle,handle->Interface_Device);
			return FALSE;
	}
}

/**
 * This routine sends a request to the device the library is currently using. It is usually called from
 * <a href="ccd_dsp.html#DSP_Send_Command">DSP_Send_Command</a>.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param request The ioctl request number sent to the device.
 * @param argument_list A list of arguments to send as a parameter to the request. Upon a successfull return from
 * 	the routine, the return value from the DSP code may be in the argument list.
 * @param argument_count The number of arguments in argument_list.
 * @return The routine returns the return value from the command routine it called. This will normally be TRUE
 * 	if the request was sent correctly, or FALSE if it failed in some way.
 * @see #CCD_Interface_Handle_T
 * @see ccd_text.html#CCD_Text_Command_List
 * @see ccd_pci.html#CCD_PCI_Command_List
 * @see ccd_dsp.html#DSP_Send_Command
 */
int CCD_Interface_Command_List(CCD_Interface_Handle_T *handle,int request,int *argument_list,int argument_count)
{
	Interface_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		Interface_Error_Number = 14;
		sprintf(Interface_Error_String,"CCD_Interface_Command_List:handle was NULL.");
		return FALSE;
	}
	/* call the device specific command routine */
	switch(handle->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			return CCD_Text_Command_List(handle,request,argument_list,argument_count);
		case CCD_INTERFACE_DEVICE_PCI:
			return CCD_PCI_Command_List(handle,request,argument_list,argument_count);
		default:
			Interface_Error_Number = 5;
			sprintf(Interface_Error_String,"CCD_Interface_Command_List failed:No device selected(%p,%d).",
				(void*)handle,handle->Interface_Device);
			return FALSE;
	}
}

/**
 * This routine gets reply data from the device the library is currently using. 
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param data The address of an unsigned short pointer, which on return from this routine will point to
 *        an area of memory containing the read out CCD image.
 * @return The routine returns TRUE on success, and FALSE if a failure occured.
 * @see #CCD_Interface_Handle_T
 * @see ccd_text.html#CCD_Text_Get_Reply_Data
 * @see ccd_pci.html#CCD_PCI_Get_Reply_Data
 */
int CCD_Interface_Get_Reply_Data(CCD_Interface_Handle_T *handle,unsigned short **data)
{
	Interface_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		Interface_Error_Number = 15;
		sprintf(Interface_Error_String,"CCD_Interface_Get_Reply_Data:handle was NULL.");
		return FALSE;
	}
	/* call the device specific get reply data routine */
	switch(handle->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			return CCD_Text_Get_Reply_Data(handle,data);
		case CCD_INTERFACE_DEVICE_PCI:
			return CCD_PCI_Get_Reply_Data(handle,data);
		default:
			Interface_Error_Number = 6;
			sprintf(Interface_Error_String,
				"CCD_Interface_Get_Reply_Data failed:No device selected(%p,%d).",
				(void*)handle,handle->Interface_Device);
			return FALSE;
	}
}

/**
 * This routine closes the interface for the device the library is currently using.
 * @param handle The address of a pointer to a CCD_Interface_Handle_T to 
 *      store the device connection specific information into.
 * @return The routine returns the return value from the close routine it called. This will normally be TRUE
 * 	if the device was successfully closed, or FALSE if it failed in some way.
 * @see #CCD_Interface_Handle_T
 * @see ccd_text.html#CCD_Text_Close
 * @see ccd_pci.html#CCD_PCI_Close
 */
int CCD_Interface_Close(CCD_Interface_Handle_T **handle)
{
	Interface_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		Interface_Error_Number = 16;
		sprintf(Interface_Error_String,"CCD_Interface_Close:handle was NULL.");
		return FALSE;
	}
	if((*handle) == NULL)
	{
		Interface_Error_Number = 18;
		sprintf(Interface_Error_String,"CCD_Interface_Close:handle points to NULL.");
		return FALSE;
	}
	/* call the device specific close routine */
	switch((*handle)->Interface_Device)
	{
		case CCD_INTERFACE_DEVICE_TEXT:
			if(!CCD_Text_Close((*handle)))
				return FALSE;
			break;
		case CCD_INTERFACE_DEVICE_PCI:
			if(!CCD_PCI_Close((*handle)))
				return FALSE;
			break;
		default:
			Interface_Error_Number = 7;
			sprintf(Interface_Error_String,"CCD_Interface_Close failed:No device selected(%p,%d).",
				(void*)(*handle),(*handle)->Interface_Device);
			return FALSE;
	}
#if LOGGING > 1
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Interface_Close(): Closing handle %p of type %d.",
			      (*handle),(*handle)->Interface_Device);
#endif
	/* free alocated handle */
	free((*handle));
	(*handle) = NULL;
	return TRUE;
}

/**
 * Routine that returns the current value of ccd_interfaces's error number.
 * @return The current value of ccd_interfaces's error number.
 */
int CCD_Interface_Get_Error_Number(void)
{
	return Interface_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_interface in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Interface_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Interface_Error_Number == 0)
		sprintf(Interface_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Interface:Error(%d) : %s\n",time_string,Interface_Error_Number,Interface_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_interface in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Interface_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Interface_Error_Number == 0)
		sprintf(Interface_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Interface:Error(%d) : %s\n",time_string,
		Interface_Error_Number,Interface_Error_String);
}

/*
** $Log: not supported by cvs2svn $
** Revision 0.9  2009/02/05 11:40:27  cjm
** Swapped Bitwise for Absolute logging levels.
**
** Revision 0.8  2008/12/04 15:05:50  cjm
** Fixed CCD_Interface_Close so switch on interface type does not fall through to error generating code.
**
** Revision 0.7  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 0.6  2006/05/16 14:14:05  cjm
** gnuify: Added GNU General Public License.
**
** Revision 0.5  2002/12/16 16:49:36  cjm
** Removed Error routines resetting error number to zero.
**
** Revision 0.4  2002/11/07 19:13:39  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 0.3  2000/06/13 17:14:13  cjm
** Changes to make Ccs agree with voodoo.
**
** Revision 0.2  2000/04/13 13:15:33  cjm
** Added current time to error routines.
**
** Revision 0.1  2000/01/25 14:57:27  cjm
** initial revision (PCI version).
**
*/
