/* ccd_interface_private.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_interface_private.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/

#ifndef CCD_INTERFACE_PRIVATE_H
#define CCD_INTERFACE_PRIVATE_H
#include "ccd_pci.h"
#include "ccd_text.h"
#include "ccd_exposure_private.h"
#include "ccd_setup_private.h"

/**
 * Structure containing handle data.
 * <dl>
 * <dt>Interface_Device</dt> <dd>The type of device accessed by this handle.</dd>
 * <dt>Handle</dt> <dd>Union of handles for each interface device type:
 *     <dl>
 *     <dt>PCI</dt> <dd>Pointer to PCI Handle of type CCD_PCI_Handle_T.</dd>
 *     <dt>Text</dt> <dd>Pointer to Text Handle of type CCD_Text_Handle_T.</dd>
 *     </dl>
 * <dt>Setup_Date</dt> <dd>Data type used to hold local data to ccd_setup.</dd>
 * <dt>Exposure_Data</dt> <dd>Structure used to hold local data to ccd_exposure.</dd>
 * </dl>
 * @see #CCD_INTERFACE_DEVICE_ID
 * @see ccd_pci.html#CCD_PCI_Handle_T
 * @see ccd_text.html#CCD_Text_Handle_T
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 */
struct CCD_Interface_Handle_Struct
{
	enum CCD_INTERFACE_DEVICE_ID Interface_Device;
	union
	{
		CCD_PCI_Handle_T *PCI;
		CCD_Text_Handle_T *Text;
	} Handle;
	struct CCD_Setup_Struct Setup_Data;
	struct CCD_Exposure_Struct Exposure_Data;
};

/*
** $Log: not supported by cvs2svn $
** Revision 1.1  2008/12/11 14:20:22  cjm
** Initial revision
**
*/
#endif
