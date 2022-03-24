/* ccd_exposure_private.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_exposure_private.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/

#ifndef CCD_EXPOSURE_PRIVATE_H
#define CCD_EXPOSURE_PRIVATE_H

#include "ccd_exposure.h" /* enum CCD_EXPOSURE_STATUS declaration */

/**
 * Structure used to hold local data to ccd_exposure.
 * <dl>
 * <dt>Exposure_Status</dt> <dd>Whether an operation is being performed to CLEAR, EXPOSE or READOUT the CCD.</dd>
 * <dt>Exposure_Length</dt> <dd>The last exposure length to be set, as requested by the user.</dd>
 * <dt>Modified_Exposure_Length</dt> <dd>The last exposure length to be set, as sent to the timing board using
 *     the SET command. This has the shutter trigger delay (STD) added, and the Shutter Close Delay SCD subtracted.
 *     See the shutter timing documentation for details.</dd>
 * <dt>Exposure_Start_Time</dt> <dd>The time stamp when the START_EXPOSURE command was sent to the controller.</dd>
 * </dl>
 * @see ccd_exposure.html#CCD_EXPOSURE_STATUS
 */
struct CCD_Exposure_Struct
{
	enum CCD_EXPOSURE_STATUS Exposure_Status;
	int Exposure_Length;
	int Modified_Exposure_Length;
	struct timespec Exposure_Start_Time;
};


/*
** $Log: not supported by cvs2svn $
** Revision 1.1  2008/12/11 14:19:59  cjm
** Initial revision
**
*/
#endif
