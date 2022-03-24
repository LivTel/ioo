/* ccd_pci.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_pci.c,v 1.1 2011-11-23 10:59:52 cjm Exp $
*/
/**
 * ccd_pci.c will implement a specific interface that connects the SDSU CCD Controller system with a host
 * computer using a PCI interface.
 * @author SDSU, Chris Mottram
 * @version $Revision: 1.1 $
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux
#include <sys/ioctl.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include "ccd_global.h"
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_interface_private.h"

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_pci.c,v 1.1 2011-11-23 10:59:52 cjm Exp $";

/* #defines */
/**
 * The maximum size for the device name strings.
 */
#define	PCI_MAX_DEV_SIZE	255

/* structures */
/**
 * Internal Data type needed by the device. PCI_Dev holds the name of the device
 * the device driver can connect to, and PCI_Fd is the opened file descriptor for the connected device.
 * <dl>
 * <dt>PCI_Dev</dt> <dd>The PCI device name, a string of length PCI_MAX_DEV_SIZE.</dd>
 * <dt>PCI_Fd</dt> <dd>The file descriptor, used for communication with the SDSU device driver.</dd>
 * <dt>Buffer</dt> <dd>Pointer to a memory buffer used for image storage.</dd>
 * <dt>Buffer_Length</dt> <dd>The allocated size of Buffer, in bytes.</dd>
 * </dl>
 * @see #PCI_MAX_DEV_SIZE
 */
struct CCD_PCI_Handle_Struct 
{
	char PCI_Dev[PCI_MAX_DEV_SIZE+1];
	int PCI_Fd;
	unsigned short *Buffer;
	int Buffer_Length;
};

/* external variables */

/* local variables */
/**
 * Variable holding error code of last operation performed by ccd_pci.
 */
static int PCI_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char PCI_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";

/* external functions */
/**
 * This routine should be called at startup if this device is to be used to communicate with the SDSU CCD Controler. 
 * It will initialise the connection information ready for the device to be opened.
 * @see ccd_interface.html#CCD_Interface_Initialise
 */
void CCD_PCI_Initialise(void)
{
	PCI_Error_Number = 0;
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_PCI_Initialise:%s.\n",rcsid);
}

/**
 * This routine will open the device so that commands and replies can be sent and received across the
 * connection. 
 * @param device_pathname The pathname of the device we are trying to talk to.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return Returns TRUE if a device can be opened, otherwise it returns FALSE.
 * @see ccd_interface.html#CCD_Interface_Open
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_PCI_Open(char *device_pathname,CCD_Interface_Handle_T *handle)
{
	int error_number;

	PCI_Error_Number = 0;
	/* test parameters */
	if(device_pathname == NULL)
	{
		PCI_Error_Number = 7;
		sprintf(PCI_Error_String,"CCD_PCI_Open:device_pathname was NULL.");
		return FALSE;
	}
	if(handle == NULL)
	{
		PCI_Error_Number = 16;
		sprintf(PCI_Error_String,"CCD_PCI_Open:handle was NULL.");
		return FALSE;
	}
	if(strlen(device_pathname) > PCI_MAX_DEV_SIZE)
	{
		PCI_Error_Number = 17;
		sprintf(PCI_Error_String,"CCD_PCI_Open:device_pathname was too long:%d.",strlen(device_pathname));
		return FALSE;
	}
	/* Allocate PCI block */
	handle->Handle.PCI = (struct CCD_PCI_Handle_Struct *)malloc(sizeof(struct CCD_PCI_Handle_Struct));
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 18;
		sprintf(PCI_Error_String,"CCD_PCI_Open:Failed to allocate memory.");
		return FALSE;
	}
	/* try to open the device */
	strcpy(handle->Handle.PCI->PCI_Dev,device_pathname);
	handle->Handle.PCI->PCI_Fd = 0;
	handle->Handle.PCI->Buffer = NULL;
	handle->Handle.PCI->Buffer_Length = 0;
	if((handle->Handle.PCI->PCI_Fd = open(handle->Handle.PCI->PCI_Dev,O_RDWR))==-1)
	{
		error_number = errno;
		PCI_Error_Number = 1;
		sprintf(PCI_Error_String,"CCD_PCI_Open:failed(%d,%s).",
			error_number,handle->Handle.PCI->PCI_Dev);
		free(handle->Handle.PCI);
		return FALSE;
	}
	return TRUE;
}

/**
 * Routine to create a memory map for image download.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param buffer_size The size of the buffer, in bytes.
 * @return Return TRUE if buffer initialisation is successful, FALSE if it wasn't.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_PCI_Memory_Map(CCD_Interface_Handle_T *handle,int buffer_size)
{
	int mmap_errno;

	if(handle == NULL)
	{
		PCI_Error_Number = 19;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_Map failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 20;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_UnMap failed:handle PCI pointer was NULL.");
		return FALSE;
	}
	if(buffer_size <= 0)
	{
		PCI_Error_Number = 12;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_Map failed:Illegal buffer size %d.",buffer_size);
		return FALSE;
	}
	if(handle->Handle.PCI->PCI_Fd == 0)
	{
		PCI_Error_Number = 13;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_Map failed:PCI file descriptor was NULL.");
		return FALSE;
	}
	handle->Handle.PCI->Buffer_Length = buffer_size;
	handle->Handle.PCI->Buffer = (unsigned short *)mmap(0,handle->Handle.PCI->Buffer_Length,(PROT_READ|PROT_WRITE),
							    MAP_SHARED,handle->Handle.PCI->PCI_Fd,0);
	if(handle->Handle.PCI->Buffer == MAP_FAILED)
	{
		mmap_errno = errno;
		PCI_Error_Number = 6;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_Map:Memory map failed(%d,%d:%s).",
			handle->Handle.PCI->Buffer_Length,mmap_errno,strerror(mmap_errno));
		return FALSE;
	}
	return TRUE;
}

/**
 * Routine to unmap the memory map used for image download.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return Return TRUE if the operation was successful, FALSE if it wasn't.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_PCI_Memory_UnMap(CCD_Interface_Handle_T *handle)
{
	int retval,munmap_errno;

	if(handle == NULL)
	{
		PCI_Error_Number = 21;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_UnMap failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 22;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_UnMap failed:handle PCI pointer was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI->Buffer == NULL)
	{
		PCI_Error_Number = 14;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_UnMap failed:Buffer was NULL (%d).",
			handle->Handle.PCI->Buffer_Length);
		return FALSE;
	}
	retval = munmap((void *)handle->Handle.PCI->Buffer,handle->Handle.PCI->Buffer_Length);
	if(retval < 0)
	{
		munmap_errno = errno;
		PCI_Error_Number = 15;
		sprintf(PCI_Error_String,"CCD_PCI_Memory_UnMap:Memory unmap failed(%p,%d,%d).",
			(void *)(handle->Handle.PCI->Buffer),
			handle->Handle.PCI->Buffer_Length,munmap_errno);
		return FALSE;
	}
	handle->Handle.PCI->Buffer = NULL;
	handle->Handle.PCI->Buffer_Length = 0;
	return TRUE;
}

/**
 * This routine will send a command to the device driver to be sent to the controller. 
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param request The type of request sent.
 * @param argument The address of an integer that contains the data to be sent as a parameter to this request,
 * 	or to receive the results of this request.
 * @return Returns TRUE if the command was sent to the device driver successfully, FALSE if an error occured.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Command
 */
int CCD_PCI_Command(CCD_Interface_Handle_T *handle,int request,int *argument)
{
	int retval,error_number;

	PCI_Error_Number = 0;
	if(handle == NULL)
	{
		PCI_Error_Number = 23;
		sprintf(PCI_Error_String,"CCD_PCI_Command failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 24;
		sprintf(PCI_Error_String,"CCD_PCI_Command failed:handle PCI pointer was NULL.");
		return FALSE;
	}
	if(argument == NULL)
	{
		PCI_Error_Number = 2;
		sprintf(PCI_Error_String,"CCD_PCI_Command:data is NULL");
		return FALSE;
	}
	/* send the command 'request' to the PCI interface, using the passed in memory */
	retval = ioctl(handle->Handle.PCI->PCI_Fd,request,argument);
	if (retval < 0)
	{
		error_number = errno;
		PCI_Error_Number = 3;
		sprintf(PCI_Error_String,"CCD_PCI_Command:failed(%d,%d,%d,%d).",
			handle->Handle.PCI->PCI_Fd,request,(*argument),error_number);
	}
	return (retval == 0);
}

/**
 * This routine will send a command to the device driver to be sent to the controller. 
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param request The type of request sent.
 * @param argument_list A list of arguments to send as a parameter to the request.
 * @param argument_count The number of arguments in argument_list.
 * @return Returns TRUE if the command was sent to the device driver successfully, FALSE if an error occured.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Command_List
 * @see #CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT
 */
int CCD_PCI_Command_List(CCD_Interface_Handle_T *handle,int request,int *argument_list,int argument_count)
{
	int ioctl_argument_list[CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT];
	int retval,error_number,i;

	PCI_Error_Number = 0;
/* check arguments */
	if(handle == NULL)
	{
		PCI_Error_Number = 25;
		sprintf(PCI_Error_String,"CCD_PCI_Command_List failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 26;
		sprintf(PCI_Error_String,"CCD_PCI_Command_List failed:handle PCI pointer was NULL.");
		return FALSE;
	}
	if(argument_list == NULL)
	{
		PCI_Error_Number = 9;
		sprintf(PCI_Error_String,"CCD_PCI_Command_List:argument_list is NULL");
		return FALSE;
	}
	if(argument_count < 0)
	{
		PCI_Error_Number = 10;
		sprintf(PCI_Error_String,"CCD_PCI_Command_List:illegal argument_count: %d.",argument_count);
		return FALSE;
	}
/* copy arguments into the ioctl argument list, which is always CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT long.
** Fill unused arguments with -1 */
	for(i=0;i<CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT;i++)
	{
		if(i < argument_count)
			ioctl_argument_list[i] = argument_list[i];
		else
			ioctl_argument_list[i] = -1;
	}
/* send the command 'request' to the PCI interface */
	retval = ioctl(handle->Handle.PCI->PCI_Fd,request,ioctl_argument_list);
	if (retval < 0)
	{
		error_number = errno;
		PCI_Error_Number = 11;
		sprintf(PCI_Error_String,"CCD_PCI_Command_List:failed(%d,%d,%d,%d).",
			handle->Handle.PCI->PCI_Fd,request,argument_count,error_number);
	}
/* copy return ioctl_arguments  back to argument list, as they contain results from ioctl. */
	for(i=0;i<argument_count;i++)
	{
		argument_list[i] = ioctl_argument_list[i];
	}
	return (retval == 0);
}

/**
 * This routine will get reply data from the SDSU CCD Controller via the PCI interface. The data parameter
 * is set to the memory mapped area, mapped to the PCI file descriptor, which will contain the read out data.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @param data The address of an unsigned short pointer, which on return from this routine will point to
 *        an area of memory containing the read out CCD image.
 * @return The routine returns TRUE on success, and FALSE if a failure occured.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Get_Reply_Data
 */
int CCD_PCI_Get_Reply_Data(CCD_Interface_Handle_T *handle,unsigned short **data)
{
	PCI_Error_Number = 0;
	/* if the data parameter is null we can't save anything in it ! */
	if(handle == NULL)
	{
		PCI_Error_Number = 27;
		sprintf(PCI_Error_String,"CCD_PCI_Get_Reply_Data failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 28;
		sprintf(PCI_Error_String,"CCD_PCI_Get_Reply_Data failed:handle PCI pointer was NULL.");
		return FALSE;
	}
	if(data == NULL)
	{
		PCI_Error_Number = 4;
		sprintf(PCI_Error_String,"CCD_PCI_Get_Reply_Data:data is NULL");
		return FALSE;
	}
	if(handle->Handle.PCI->Buffer == NULL)
	{
		PCI_Error_Number = 5;
		sprintf(PCI_Error_String,"CCD_PCI_Get_Reply_Data:Reply Buffer is NULL");
		return FALSE;
	}
	(*data) = (unsigned short *)(handle->Handle.PCI->Buffer);
	return TRUE;
}

/**
 * This routine will close the PCI interface.
 * @param handle The address of a CCD_Interface_Handle_T to store the device connection specific information into.
 * @return The routine returns the return value from the close routine it called. This will normally be TRUE
 * 	if the device was successfully closed, or FALSE if it failed in some way.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Close
 */
int CCD_PCI_Close(CCD_Interface_Handle_T *handle)
{
	int error_number;

	PCI_Error_Number = 0;
	/* check parameters */
	if(handle == NULL)
	{
		PCI_Error_Number = 29;
		sprintf(PCI_Error_String,"CCD_PCI_Close failed:handle was NULL.");
		return FALSE;
	}
	if(handle->Handle.PCI == NULL)
	{
		PCI_Error_Number = 30;
		sprintf(PCI_Error_String,"CCD_PCI_Close failed:handle PCI pointer was NULL.");
		return FALSE;
	}
	/* close the interface - close returns -1 on failure */
	if((close(handle->Handle.PCI->PCI_Fd))==-1)
	{
		error_number = errno;
		PCI_Error_Number = 8;
		sprintf(PCI_Error_String,"CCD_PCI_Close:failed %d",error_number);
		return FALSE;
	}
	/* free allocated handle space */
	free(handle->Handle.PCI);
	handle->Handle.PCI = NULL;
	return TRUE;
}
/**
 * Returns the current value of ccd_pci's error number.
 * @return The current value of ccd_pci's error number.
 */
int CCD_PCI_Get_Error_Number(void)
{
	return PCI_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_pci in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_PCI_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(PCI_Error_Number == 0)
		sprintf(PCI_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_PCI:Error(%d) : %s\n",time_string,PCI_Error_Number,PCI_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_pci in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_PCI_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(PCI_Error_Number == 0)
		sprintf(PCI_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_PCI:Error(%d) : %s\n",time_string,
		PCI_Error_Number,PCI_Error_String);
}

/**
 * The warning routine that reports any warnings occuring in ccd_pci in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_PCI_Warning(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an warning message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an warning to display */
	if(PCI_Error_Number == 0)
		sprintf(PCI_Error_String,"Logic Error:No Warning defined");
	fprintf(stderr,"%s CCD_PCI:Warning(%d) : %s\n",time_string,PCI_Error_Number,PCI_Error_String);
}

/*
** $Log: not supported by cvs2svn $
** Revision 0.9  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 0.8  2006/05/17 17:25:19  cjm
** Added ioctl include for Linux.
** Fixed casting problem.
**
** Revision 0.7  2006/05/16 14:14:06  cjm
** gnuify: Added GNU General Public License.
**
** Revision 0.6  2002/12/16 16:49:36  cjm
** Removed Error routines resetting error number to zero.
**
** Revision 0.5  2002/11/07 19:13:39  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 0.4  2000/06/19 08:48:34  cjm
** Backup.
**
** Revision 0.3  2000/06/13 17:14:13  cjm
** Changes to make Ccs agree with voodoo.
**
** Revision 0.2  2000/04/13 13:13:34  cjm
** Added current time to error routines.
**
** Revision 0.1  2000/01/25 14:57:27  cjm
** initial revision (PCI version).
**
*/
