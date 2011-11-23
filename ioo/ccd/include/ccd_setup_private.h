/* ccd_setup_private.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_setup_private.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/

#ifndef CCD_SETUP_PRIVATE_H
#define CCD_SETUP_PRIVATE_H

#include "ccd_dsp.h" /* enum CCD_DSP_* declaration */
#include "ccd_setup.h" /* enum CCD_Setup_Window_Struct declaration */

/**
 * Data type used to hold local data to ccd_setup. Fields are:
 * <dl>
 * <dt>NCols</dt> <dd>The number of columns that will be used on the CCD.</dd>
 * <dt>NRows</dt> <dd>The number of rows that will be used on the CCD.</dd>
 * <dt>NSBin</dt> <dd>The amount of binning of columns on the CCD.</dd>
 * <dt>NPBin</dt> <dd>The amount of binning of rows on the CCD.</dd>
 * <dt>DeInterlace_Type</dt> <dd>The type of deinterlacing the image will require. This depends on the way the
 * 	SDSU CCD Controller reads out the CCD. Acceptable values in 
 * 	<a href="ccd_dsp.html#CCD_DSP_DEINTERLACE_TYPE">CCD_DSP_DEINTERLACE_TYPE</a> are:
 *	CCD_DSP_DEINTERLACE_SINGLE,
 *	CCD_DSP_DEINTERLACE_FLIP,
 *	CCD_DSP_DEINTERLACE_SPLIT_PARALLEL,
 * 	CCD_DSP_DEINTERLACE_SPLIT_SERIAL or
 * 	CCD_DSP_DEINTERLACE_SPLIT_QUAD.</dd>
 * <dt>Gain</dt> <dd>The gain setting used to configure the CCD electronics.</dd>
 * <dt>Amplifier</dt> <dd>The amplifier setting used to configure the CCD electronics.</dd>
 * <dt>Idle</dt> <dd>A boolean, set as to whether we set the CCD electronics to Idle clock or not.</dd>
 * <dt>Window_Flags</dt> <dd>The window flags for this setup. Determines which of the four possible windows
 * 	are in use for this setup.</dd>
 * <dt>Window_List</dt> <dd>A list of window positions on the CCD. Theere are a maximum of CCD_SETUP_WINDOW_COUNT
 * 	windows. The windows should not overlap in either dimension.</dd>
 * <dt>Power_Complete</dt> <dd>A boolean value indicating whether the power cycling operation was completed
 * 	successfully.</dd>
 * <dt>PCI_Complete</dt> <dd>A boolean value indicating whether the PCI interface program was completed
 * 	successfully.</dd>
 * <dt>Timing_Complete</dt> <dd>A boolean value indicating whether the timing program was completed
 * 	successfully.</dd>
 * <dt>Utility_Complete</dt> <dd>A boolean value indicating whether the utility program was completed
 * 	successfully.</dd>
 * <dt>Dimension_Complete</dt> <dd>A boolean value indicating whether the dimension setup was completed
 * 	successfully.</dd>
 * <dt>Setup_In_Progress</dt> <dd>A boolean value indicating whether the setup operation is in progress.</dd>
 * </dl>
 * @see #CCD_Setup_Window_Struct
 */
struct CCD_Setup_Struct
{
	int NCols;
	int NRows;
	int NSBin;
	int NPBin;
	enum CCD_DSP_DEINTERLACE_TYPE DeInterlace_Type;
	enum CCD_DSP_GAIN Gain;
	enum CCD_DSP_AMPLIFIER Amplifier;
	int Idle;
	int Window_Flags;
	struct CCD_Setup_Window_Struct Window_List[CCD_SETUP_WINDOW_COUNT];
	int Power_Complete;
	int PCI_Complete;
	int Timing_Complete;
	int Utility_Complete;
	int Dimension_Complete;
	int Setup_In_Progress;
};

/*
** $Log: not supported by cvs2svn $
** Revision 1.1  2009/02/05 11:40:43  cjm
** Initial revision
**
*/
#endif
