/* ccd_dsp_download.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_dsp_download.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/
#ifndef CCD_DSP_DOWNLOAD_H
#define CCD_DSP_DOWNLOAD_H
#include "ccd_global.h"
#include "ccd_dsp.h"
#include "ccd_interface.h"

extern int CCD_DSP_Download_Initialise(void);
extern int CCD_DSP_Download(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,char *filename);
extern int CCD_DSP_Download_Get_Error_Number(void);
extern void CCD_DSP_Download_Error(void);
extern void CCD_DSP_Download_Error_String(char *error_string);

#endif
