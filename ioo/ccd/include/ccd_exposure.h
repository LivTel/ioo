/* ccd_exposure.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_exposure.h,v 1.3 2013-03-25 15:26:42 cjm Exp $
*/
#ifndef CCD_EXPOSURE_H
#define CCD_EXPOSURE_H
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for time.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for time. Only defined if not already defined.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h>
#include "ccd_global.h"
#include "ccd_interface.h"

/* These #define/enum definitions should match with those in CCDLibrary.java */
/**
 * When bits 3 and 5 in the HSTR are set, the SDSU controllers are reading out.
 */
#define CCD_EXPOSURE_HSTR_READOUT				(0x5)
/**
 * How many bits to shift the HSTR register to get the READOUT status bits.
 */
#define CCD_EXPOSURE_HSTR_BIT_SHIFT				(0x3)

/**
 * Return value from CCD_DSP_Get_Exposure_Status. 
 * <ul>
 * <li>CCD_EXPOSURE_STATUS_NONE means the library is not currently performing an exposure.
 * <li>CCD_EXPOSURE_STATUS_WAIT_START means the library is waiting for the correct moment to open the shutter.
 * <li>CCD_EXPOSURE_STATUS_CLEAR means the library is currently clearing the ccd.
 * <li>CCD_EXPOSURE_STATUS_EXPOSE means the library is currently performing an exposure.
 * <li>CCD_EXPOSURE_STATUS_PRE_READOUT means the library is currently exposing, but is about
 * 	to start reading out data from the ccd (so don't start any commands that won't work in readout). 
 * <li>CCD_EXPOSURE_STATUS_READOUT means the library is currently reading out data from the ccd.
 * <li>CCD_EXPOSURE_STATUS_POST_READOUT means the library has finished reading out, but is post processing
 * 	the data (byte swap/de-interlacing/saving to disk).
 * </ul>
 * If these values are changed, the relevant values in ngat.ccd.CCDLibrary should be updated to match.
 * @see #CCD_DSP_Get_Exposure_Status
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_NONE
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_WAIT_START
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_CLEAR
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_EXPOSE
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_PRE_READOUT
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_READOUT
 * @see ../../javadocs/ngat/o/ccd/CCDLibrary.html#EXPOSURE_STATUS_POST_READOUT
 */
enum CCD_EXPOSURE_STATUS
{
	CCD_EXPOSURE_STATUS_NONE,CCD_EXPOSURE_STATUS_WAIT_START,CCD_EXPOSURE_STATUS_CLEAR,
	CCD_EXPOSURE_STATUS_EXPOSE,CCD_EXPOSURE_STATUS_PRE_READOUT,CCD_EXPOSURE_STATUS_READOUT,
	CCD_EXPOSURE_STATUS_POST_READOUT
};

/**
 * Macro to check whether the exposure status is a legal value.
 * @see #CCD_EXPOSURE_STATUS
 */
#define CCD_EXPOSURE_IS_STATUS(status)	(((status) == CCD_EXPOSURE_STATUS_NONE)|| \
        ((status) == CCD_EXPOSURE_STATUS_WAIT_START)|| \
	((status) == CCD_EXPOSURE_STATUS_CLEAR)||((status) == CCD_EXPOSURE_STATUS_EXPOSE)|| \
        ((status) == CCD_EXPOSURE_STATUS_READOUT)||((status) == CCD_EXPOSURE_STATUS_POST_READOUT))

extern void CCD_Exposure_Initialise(void);
extern void CCD_Exposure_Data_Initialise(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Expose(CCD_Interface_Handle_T* handle,int clear_array,int open_shutter,
			       struct timespec start_time,int exposure_time,
			       char **filename_list,int filename_count);
extern int CCD_Exposure_Bias(CCD_Interface_Handle_T* handle,char *filename);
extern int CCD_Exposure_Open_Shutter(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Close_Shutter(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Pause(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Resume(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Abort(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Read_Out_CCD(CCD_Interface_Handle_T* handle,char *filename);

extern int CCD_Exposure_Set_Exposure_Status(CCD_Interface_Handle_T* handle,enum CCD_EXPOSURE_STATUS status);
extern enum CCD_EXPOSURE_STATUS CCD_Exposure_Get_Exposure_Status(CCD_Interface_Handle_T* handle);
extern struct timespec CCD_Exposure_Get_Exposure_Start_Time(CCD_Interface_Handle_T* handle);
extern int CCD_Exposure_Get_Exposure_Length(CCD_Interface_Handle_T* handle);
extern void CCD_Exposure_Set_Exposure_Start_Time(CCD_Interface_Handle_T* handle);
extern void CCD_Exposure_Set_Start_Exposure_Clear_Time(int time);
extern int CCD_Exposure_Get_Start_Exposure_Clear_Time(void);
extern void CCD_Exposure_Set_Start_Exposure_Offset_Time(int time);
extern int CCD_Exposure_Get_Start_Exposure_Offset_Time(void);
extern void CCD_Exposure_Set_Readout_Remaining_Time(int time);
extern int CCD_Exposure_Get_Readout_Remaining_Time(void);

extern void CCD_Exposure_Shutter_Trigger_Delay_Set(int delay_ms);
extern int CCD_Exposure_Shutter_Trigger_Delay_Get(void);
extern void CCD_Exposure_Shutter_Open_Delay_Set(int delay_ms);
extern int CCD_Exposure_Shutter_Open_Delay_Get(void);
extern void CCD_Exposure_Shutter_Close_Delay_Set(int delay_ms);
extern int CCD_Exposure_Shutter_Close_Delay_Get(void);
extern void CCD_Exposure_Readout_Delay_Set(int delay_ms);
extern int CCD_Exposure_Readout_Delay_Get(void);

extern int CCD_Exposure_Get_Error_Number(void);
extern void CCD_Exposure_Error(void);
extern void CCD_Exposure_Error_String(char *error_string);
extern void CCD_Exposure_Warning(void);

#endif
