/* ccd_filter_wheel.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_filter_wheel.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/
#ifndef CCD_FILTER_WHEEL_H
#define CCD_FILTER_WHEEL_H

/* definition of CCD_Interface_Handle_T */
#include "ccd_interface.h"

/* These #define/enum definitions should match with those in CCDLibrary.java */
/**
 * Return value from CCD_Filter_Wheel_Get_Status.
 * <ul>
 * <li>CCD_FILTER_WHEEL_STATUS_NONE means the library is not currently doing anything with the filter wheels.
 * <li>CCD_FILTER_WHEEL_STATUS_LOCATORS_OUT means the filter wheel locators are being moved out.
 * <li>CCD_FILTER_WHEEL_STATUS_MOVING means the library is moving the filter wheels.
 * <li>CCD_FILTER_WHEEL_STATUS_LOCATORS_IN means the filter wheel locators are being moved in.
 * <li>CCD_FILTER_WHEEL_STATUS_ABORTED means the library is aborting the filter wheels.
 * </ul>
 * @see #CCD_Filter_Wheel_Get_Status
 */
enum CCD_FILTER_WHEEL_STATUS
{
	CCD_FILTER_WHEEL_STATUS_NONE,CCD_FILTER_WHEEL_STATUS_LOCATORS_OUT,
	CCD_FILTER_WHEEL_STATUS_MOVING,CCD_FILTER_WHEEL_STATUS_LOCATORS_IN,CCD_FILTER_WHEEL_STATUS_ABORTED
};


extern int CCD_Filter_Wheel_Initialise(void);

extern int CCD_Filter_Wheel_Position_Count_Set(int position_count);
extern int CCD_Filter_Wheel_Position_Count_Get(void);
extern int CCD_Filter_Wheel_Reset(CCD_Interface_Handle_T* handle);
extern int CCD_Filter_Wheel_Move(CCD_Interface_Handle_T* handle,int position);
/* CCD_Filter_Wheel_Move_Relative is external for engineering/test software only. See API. */
extern int CCD_Filter_Wheel_Move_Relative(CCD_Interface_Handle_T* handle,int direction,int relative_position,
					  int check_home);
extern int CCD_Filter_Wheel_Abort(CCD_Interface_Handle_T* handle);
extern int CCD_Filter_Wheel_Get_Position(int *position);
extern enum CCD_FILTER_WHEEL_STATUS CCD_Filter_Wheel_Get_Status(void);

extern int CCD_Filter_Wheel_Get_Error_Number(void);
extern void CCD_Filter_Wheel_Error(void);
extern void CCD_Filter_Wheel_Error_String(char *error_string);

/*
** $Log: not supported by cvs2svn $
*/
#endif
