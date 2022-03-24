/* ccd_pci.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_pci.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/

#ifndef CCD_PCI_H
#define CCD_PCI_H

/**
 * Default string for the name of the first device.
 */
#define CCD_PCI_DEFAULT_DEVICE_ZERO		"/dev/astropci0"
/**
 * Default string for the name of the second device.
 */
#define CCD_PCI_DEFAULT_DEVICE_ONE		"/dev/astropci1"

/**
 * The number of integers in the array to pass as an argument into
 * an ioctl multiple argument command using the SDSU device.
 */
#define CCD_PCI_IOCTL_ARGUMENT_LIST_COUNT	(0x6)

/* The next block of hash definitions are PCI interface meta commands for passing to the HCVR
** Host Command Vector Register */
/* Should this be an enum ? */
/**
 * HCVR (Host Command Vector Register) command. Used as an ioctl request argument for the CCD_PCI_IOCTL_SET_HCVR
 * ioctl request.
 * This command resets the SDSU controller.
 * @see #CCD_PCI_IOCTL_SET_HCVR
 */
#define CCD_PCI_HCVR_RESET_CONTROLLER 		(0x87)
/**
 * HCVR (Host Command Vector Register) command. Used as an ioctl request argument for the CCD_PCI_IOCTL_SET_HCVR
 * ioctl request.
 * This command resets the PCI boards program counter, to stop it locking up.
 * @see #CCD_PCI_IOCTL_SET_HCVR
 */
#define CCD_PCI_HCVR_PCI_PC_RESET 		(0x8077)
/**
 * HCVR (Host Command Vector Register) command. Used as an ioctl request argument for the CCD_PCI_IOCTL_SET_HCVR
 * ioctl request.
 * This command sets the bias voltages by reading the voltages codes on the timing board Y: memory area and
 * writing them to the video processor. It is equivalent to sending the SBV DSP command to the timing board.
 * @see #CCD_PCI_IOCTL_SET_HCVR
 */
#define CCD_PCI_HCVR_SET_BIAS_VOLTAGES 		(0x8091)
/**
 * HCVR (Host Command Vector Register) command. Used as an ioctl request argument for the CCD_PCI_IOCTL_SET_HCVR
 * ioctl request.
 * This command causes the timing board and PCI interface to abort the readout of an image on the CCD.
 * @see #CCD_PCI_IOCTL_SET_HCVR
 */
#define CCD_PCI_HCVR_ABORT_READOUT 		(0x8079)

/* The next block of hash definitions are PCI ioctl request numbers */
/* Should this be an enum ? */

/**
 * ioctl request code for the SDSU controller PCI interface.
 * Get the current value of the PCI DSP Host Control Register.
 */
#define CCD_PCI_IOCTL_GET_HCTR 			(0x1)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * This command gets the progress of the current read image command.
 */
#define CCD_PCI_IOCTL_GET_PROGRESS 		(0x2)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * This command gets the Direct Memory Address pointer.
 */
#define CCD_PCI_IOCTL_GET_DMA_ADDR		(0x3)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Get the current value of the PCI DSP Host Status Register.
 */
#define CCD_PCI_IOCTL_GET_HSTR 			(0x4)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * This command sets the the data parameter associated with the HCVR command.
 * It's also used to download DSP code to the PCI board.
 */
#define CCD_PCI_IOCTL_HCVR_DATA			(0x10)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Set the value of the HCTR (Host Interface Control Register).
 */
#define CCD_PCI_IOCTL_SET_HCTR			(0x11)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Set the value of the HCVR (Host Command Vector Register).
 */
#define CCD_PCI_IOCTL_SET_HCVR			(0x12)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * This command tells the SDSU controller to go into a mode to receive
 * a downlaod of the PCI DSP program.
 */
#define CCD_PCI_IOCTL_PCI_DOWNLOAD		(0x13)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * This command waits for the SDSU controller PCI board to complete initialisation after
 * a DSP code download.
 */
#define CCD_PCI_IOCTL_PCI_DOWNLOAD_WAIT		(0x14)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * This command gives the SDSU controller a command to execute.
 */
#define CCD_PCI_IOCTL_COMMAND			(0x15)

/**
 * ioctl request code for the SDSU controller PCI interface.
 * Set the value of the manual command register.
 */
#define CCD_PCI_IOCTL_SET_CMDR 			(0x100)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Set the current value of the manual command register.
 */
#define CCD_PCI_IOCTL_SET_DESTINATION 		(0x111)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Set the address of the image data buffers.
 */
#define CCD_PCI_IOCTL_SET_IMAGE_BUFFERS 	(0x122)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Set utility board option word.
 */
#define CCD_PCI_IOCTL_SET_UTIL_OPTIONS 		(0x123)
/**
 * ioctl request code for the SDSU controller PCI interface.
 * Used to Abort readout in the device driver.
 */
#define CCD_PCI_IOCTL_ABORT_READ 		(0x302)

/**
 * Typedef for the PCI handle pointer, which is an instance of CCD_PCI_Handle_Struct.
 * @see #CCD_PCI_Handle_Struct
 */
typedef struct CCD_PCI_Handle_Struct CCD_PCI_Handle_T;

/* external routines */
extern void CCD_PCI_Initialise(void);
extern int CCD_PCI_Open(char *device_pathname,CCD_Interface_Handle_T *handle);
extern int CCD_PCI_Memory_Map(CCD_Interface_Handle_T *handle,int buffer_size);
extern int CCD_PCI_Memory_UnMap(CCD_Interface_Handle_T *handle);
extern int CCD_PCI_Command(CCD_Interface_Handle_T *handle,int request,int *argument);
extern int CCD_PCI_Command_List(CCD_Interface_Handle_T *handle,int request,int *argument_list,int argument_count);
extern int CCD_PCI_Get_Reply_Data(CCD_Interface_Handle_T *handle,unsigned short **data);
extern int CCD_PCI_Close(CCD_Interface_Handle_T *handle);
extern int CCD_PCI_Get_Error_Number(void);
extern void CCD_PCI_Error(void);
extern void CCD_PCI_Error_String(char *error_string);
extern void CCD_PCI_Warning(void);

#endif
