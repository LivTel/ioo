/* ccd_interface.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_interface.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/

#ifndef CCD_INTERFACE_H
#define CCD_INTERFACE_H

/* These enum definitions should match with those in CCDLibrary.java */
/**
 * Enum for the various interfaces commands generated by libccd can be sent to. The CCD_Interface_Set_Device
 * command is passed one of these to set the device.
 * <ul>
 * <li>CCD_INTERFACE_DEVICE_NONE Interface device number, showing that commands will currently be sent nowhere.
 * <li>CCD_INTERFACE_DEVICE_TEXT Interface device number, showing that commands will currently be sent to the 
 * text interface to be printed out.
 * <li>CCD_INTERFACE_DEVICE_PCI Interface device number, showing that commands will currently be sent to the PCI 
 * interface.
 * </ul>
 * @see #CCD_Interface_Set_Device
 */
enum CCD_INTERFACE_DEVICE_ID
{
	CCD_INTERFACE_DEVICE_NONE,CCD_INTERFACE_DEVICE_TEXT,CCD_INTERFACE_DEVICE_PCI
};

/**
 * Macro to check whether the interface device number is in range.
 */
#define CCD_INTERFACE_IS_INTERFACE_DEVICE(interface_device)	(((interface_device) == CCD_INTERFACE_DEVICE_NONE)|| \
	((interface_device) == CCD_INTERFACE_DEVICE_TEXT)||((interface_device) == CCD_INTERFACE_DEVICE_PCI))

/**
 * Typedef for the interface handle pointer, which is an instance of CCD_Interface_Handle_Struct.
 * @see #CCD_Interface_Handle_Struct
 */
typedef struct CCD_Interface_Handle_Struct CCD_Interface_Handle_T;

/* top level implementation of device interface */
extern void CCD_Interface_Initialise(void);
extern int CCD_Interface_Open(enum CCD_INTERFACE_DEVICE_ID device_number,char *device_pathname,
			      CCD_Interface_Handle_T **handle);
extern int CCD_Interface_Memory_Map(CCD_Interface_Handle_T *handle,int buffer_size);
extern int CCD_Interface_Memory_UnMap(CCD_Interface_Handle_T *handle);
extern int CCD_Interface_Command(CCD_Interface_Handle_T *handle,int request,int *argument);
extern int CCD_Interface_Command_List(CCD_Interface_Handle_T *handle,int request,int *argument_list,
				      int argument_count);
extern int CCD_Interface_Get_Reply_Data(CCD_Interface_Handle_T *handle,unsigned short **data);
extern int CCD_Interface_Close(CCD_Interface_Handle_T **handle);
extern int CCD_Interface_Get_Error_Number(void);
extern void CCD_Interface_Error(void);
extern void CCD_Interface_Error_String(char *error_string);

#endif
