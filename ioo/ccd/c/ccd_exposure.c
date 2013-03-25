/* ccd_exposure.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_exposure.c,v 1.7 2013-03-25 15:15:03 cjm Exp $
*/
/**
 * ccd_exposure.c contains routines for performing an exposure with the SDSU CCD Controller. There is a
 * routine that does the whole job in one go, or several routines can be called to do parts of an exposure.
 * An exposure can be paused and resumed, or it can be stopped or aborted.
 * @author SDSU, Chris Mottram
 * @version $Revision: 1.7 $
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for time.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for time.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#ifndef _POSIX_TIMERS
#include <sys/time.h>
#endif
#include <time.h>
#include "log_udp.h"
#include "ccd_dsp.h"
#include "ccd_exposure.h"
#include "ccd_exposure_private.h"
#include "ccd_interface.h"
#include "ccd_interface_private.h"
#include "ccd_pixel_stream.h"
#include "ccd_setup.h"

/* hash definitions */
/**
 * Memory address on the SDSU Timing Board, X memory space, which holds the controller status.
 * The CCD_DSP_CONTROLLER_STATUS_OPEN_SHUTTER_BIT holds whether to open the shutter when
 * a SEX command is issued to the controller.
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_STATUS_OPEN_SHUTTER_BIT
 */
#define EXPOSURE_ADDRESS_CONTROLLER_STATUS		(0x0)
/**
 * Memory address on the SDSU Timing Board, Y memory space, which holds the shutter delay value in milliseconds.
 * At the end of an exposure sequence, after a close shutter command is set, the code waits for Y:SHDEL
 * milliseconds before starting to read out the CCD. In the C layer, we should set this value to the
 * sum of the Shutter_Close_Delay and Readout_Delay variables. This address should match the SHDEL value 
 * in the tim.lod file.
 */
#define EXPOSURE_ADDRESS_SHDEL                          (0x17)
/**
 * Bits used when getting the HSTR status.
 */
#define EXPOSURE_HSTR_HTF_BITS				(0x38)
/**
 * Number used to determine how long we keep getting the same number of readout pixels
 * returned before we timeout. This number depeends on the sleep in the loop.
 * This is by default 1 second, which makes this number the number of seconds
 * we don't read out more pixels before we time out.
 */
#define EXPOSURE_READ_TIMEOUT                           (0x5)
/**
 * The number of milliseconds before the controller stops exposing and starts reading out,
 * that we switch the exposure status from EXPOSING to READOUT. This is done early as we 
 * only check the HSTR status every second, and RDM/TDL/RET/WRM check the exposure status
 * to determine whether it is safe. It is not safe to call RDM/TDL/RET/WRM when
 * the HSTR is in readout mode, so we change exposure state early. Note the value
 * of this define should be greater than the sleep in the exposure loop.
 */
#define EXPOSURE_DEFAULT_READOUT_REMAINING_TIME       	(1500)
/**
 * The default amount of time before we are due to start an exposure, that a CLEAR_ARRAY command should be sent to
 * the controller. This time is in seconds, and must be greater than the time the CLEAR_ARRAY command takes to
 * clock all accumulated charge off the CCD (approx 5 seconds for a 2kx2k EEV42-40).
 */
#define EXPOSURE_DEFAULT_START_EXPOSURE_CLEAR_TIME	(10)
/**
 * The default amount of time, in milliseconds, before the desired start of exposure that we should send the
 * START_EXPOSURE command, to allow for transmission delay.
 */
#define EXPOSURE_DEFAULT_START_EXPOSURE_OFFSET_TIME	(2)
/**
 * The default amount of time, in milliseconds, between the Uniblitz CS90 shutter controller receiving the
 * open shutter signal, and the shutter starting to open.
 * The Uniblitz spec sheet says 20ms, IAS calibration 2011/09/28 15ms.
 */
#define EXPOSURE_DEFAULT_SHUTTER_TRIGGER_DELAY          (15)
/**
 * The default amount of time, in milliseconds, for the Uniblitz CS90 shutter to open completely.
 * The Uniblitz spec sheet says 70ms, IAS calibration 2011/09/28 45ms.
 */
#define EXPOSURE_DEFAULT_SHUTTER_OPEN_DELAY             (45)
/**
 * The default amount of time, in milliseconds, for the Uniblitz CS90 shutter to close completely.
 * The Uniblitz spec sheet says 90ms, IAS calibration 2011/09/28 110ms.
 */
#define EXPOSURE_DEFAULT_SHUTTER_CLOSE_DELAY            (110)
/**
 * The default amount of time, in milliseconds, to wait in addition to the shutter close delay, after the shutter
 * has been told to shut, before we start to readout.
 */
#define EXPOSURE_DEFAULT_READOUT_DELAY                  (100)

/* structure */
/**
 * Structure used to hold local data to ccd_exposure. Note there is also a CCD_Exposure_Struct, defined in
 * ccd_exposure_private.h, that is used to hold exposure related data within a CCD_Interface_Handle_T.
 * This structure defines library wide data.
 * <dl>
 * <dt>Start_Exposure_Clear_Time</dt> <dd>The amount of time before we are due to start an exposure, 
 * 	that a CLEAR_ARRAY command should be sent to the controller. This time is in seconds, 
 * 	and must be greater than the time the CLEAR_ARRAY command takes to clock all accumulated charge off the CCD 
 * 	(approx 5 seconds for a 2kx2k EEV42-40).</dd>
 * <dt>Start_Exposure_Offset_Time</dt> <dd>The amount of time, in milliseconds, before the desired start of 
 * 	exposure that we should send the START_EXPOSURE command, to allow for transmission delay.</dd>
 * <dt>Shutter_Trigger_Delay</dt> <dd>The amount of time, in milliseconds, between the DSP code setting the shutter
 *     line high and the shutter starting to open. This delay is caused by the Uniblitz CS90 shutter controller.</dd>
 * <dt>Shutter_Open_Delay</dt> <dd>The length of time in milliseconds, that it takes the Uniblitz CS90 shutter 
 *     to open.</dd>
 * <dt>Shutter_Close_Delay</dt> <dd>The length of time in milliseconds, that it takes the Uniblitz CS90 shutter 
 *     to close.</dd>
 * <dt>Readout_Delay</dt> <dd>The length of time in milliseconds, to wait after the shutter has closed before
 *     we start the readout.</dd>
 * <dt>Readout_Remaining_Time</dt> <dd>Amount of time, in milleseconds,
 * 	remaining for an exposure when we change status to READOUT, to stop RDM/TDL/WRMs affecting the readout.</dd>
 * </dl>
 */
struct Exposure_Struct
{
	int Start_Exposure_Clear_Time;
	int Start_Exposure_Offset_Time;
	int Shutter_Trigger_Delay;
	int Shutter_Open_Delay;
	int Shutter_Close_Delay;
	int Readout_Delay;
	int Readout_Remaining_Time;
};

/* external variables */

/* internal variables */
/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_exposure.c,v 1.7 2013-03-25 15:15:03 cjm Exp $";
/**
 * Internal exposure data (library wide).
 * @see #Exposure_Struc
 */
static struct Exposure_Struct Exposure_Data;
/**
 * Variable holding error code of last operation performed by ccd_exposure.
 */
static int Exposure_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Exposure_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";

/* internal functions */
static int Exposure_Shutter_Control(CCD_Interface_Handle_T* handle,int value);

/* external functions */
/**
 * This routine sets up ccd_exposure internal variables.
 * It should be called at startup. The following is setup:
 * <dl>
 * <dt>Start_Exposure_Clear_Time</dt>  <dd>EXPOSURE_DEFAULT_START_EXPOSURE_CLEAR_TIME</dd>
 * <dt>Start_Exposure_Offset_Time</dt> <dd>EXPOSURE_DEFAULT_START_EXPOSURE_OFFSET_TIME</dd>
 * <dt>Shutter_Trigger_Delay</dt>      <dd>EXPOSURE_DEFAULT_SHUTTER_TRIGGER_DELAY</dd>
 * <dt>Shutter_Open_Delay</dt>         <dd>EXPOSURE_DEFAULT_SHUTTER_OPEN_DELAY</dd>
 * <dt>Shutter_Close_Delay</dt>        <dd>EXPOSURE_DEFAULT_SHUTTER_CLOSE_DELAY</dd>
 * <dt>Readout_Delay</dt>              <dd>EXPOSURE_DEFAULT_READOUT_DELAY</dd>
 * <dt>Readout_Remaining_Time</dt>     <dd>EXPOSURE_DEFAULT_READOUT_REMAINING_TIME</dd>
 * </dl>
 * @see #Exposure_Data
 * @see #Exposure_Struct
 * @see #EXPOSURE_DEFAULT_START_EXPOSURE_CLEAR_TIME
 * @see #EXPOSURE_DEFAULT_START_EXPOSURE_OFFSET_TIME
 * @see #EXPOSURE_DEFAULT_SHUTTER_TRIGGER_DELAY
 * @see #EXPOSURE_DEFAULT_SHUTTER_OPEN_DELAY
 * @see #EXPOSURE_DEFAULT_SHUTTER_CLOSE_DELAY
 * @see #EXPOSURE_DEFAULT_READOUT_DELAY
 * @see #EXPOSURE_DEFAULT_READOUT_REMAINING_TIME
 */
void CCD_Exposure_Initialise(void)
{
	Exposure_Error_Number = 0;
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Exposure_Initialise:%s.\n",rcsid);
#ifdef CCD_EXPOSURE_BYTE_SWAP
	fprintf(stdout,"CCD_Exposure_Initialise:Image data is byte swapped by the application.\n");
#else
	fprintf(stdout,"CCD_Exposure_Initialise:Image data is byte swapped by the device driver.\n");
#endif
#ifdef CFITSIO
	fprintf(stdout,"CCD_Exposure_Initialise:Using CFITSIO.\n");
#else
	fprintf(stdout,"CCD_Exposure_Initialise:NOT Using CFITSIO.\n");
#endif
	Exposure_Data.Start_Exposure_Clear_Time = EXPOSURE_DEFAULT_START_EXPOSURE_CLEAR_TIME;
	Exposure_Data.Start_Exposure_Offset_Time = EXPOSURE_DEFAULT_START_EXPOSURE_OFFSET_TIME;
	Exposure_Data.Shutter_Trigger_Delay = EXPOSURE_DEFAULT_SHUTTER_TRIGGER_DELAY;
	Exposure_Data.Shutter_Open_Delay = EXPOSURE_DEFAULT_SHUTTER_OPEN_DELAY;
	Exposure_Data.Shutter_Close_Delay = EXPOSURE_DEFAULT_SHUTTER_CLOSE_DELAY;
	Exposure_Data.Readout_Delay = EXPOSURE_DEFAULT_READOUT_DELAY;
	Exposure_Data.Readout_Remaining_Time = EXPOSURE_DEFAULT_READOUT_REMAINING_TIME;
}

/**
 * Routine to initialise the exposure data in the interface handle. The data is initialsied as follows:
 * <dl>
 * <dt>Exposure_Status</dt> <dd>CCD_EXPOSURE_STATUS_NONE</dd>
 * <dt>Exposure_Length</dt> <dd>0</dd>
 * <dt>Modified_Exposure_Length</dt> <dd>0</dd>
 * <dt>Exposure_Start_Time</dt> <dd>{0L,0L}</dd>
 * </dl>
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
void CCD_Exposure_Data_Initialise(CCD_Interface_Handle_T* handle)
{
	handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
	handle->Exposure_Data.Exposure_Length = 0;
	handle->Exposure_Data.Modified_Exposure_Length = 0;
	handle->Exposure_Data.Exposure_Start_Time.tv_sec = 0;
	handle->Exposure_Data.Exposure_Start_Time.tv_nsec = 0;
}

/**
 * Routine to perform an exposure.
 * <ul>
 * <li>It checks to ensure CCD Setup has been successfully completed using CCD_Setup_Get_Setup_Complete.
 * <li>The controller is told whether to open the shutter or not during the exposure, depending on the value
 * 	of the open_shutter parameter.
 * <li>The exposure length is modifed by adding the shutter transfer delay and subtracting the shutter close delay,
 *     if open_shutter is TRUE.
 * <li>The modified length of exposure is sent to the controller using CCD_DSP_Command_SET.
 * <li>The timing board SHDEL variable (delay between sending shutter close and starting readout) is configured
 *     using CCD_DSP_Command_WRM to be the sum of the shutter close delay and readout delay.
 * <li>A sleep is executed until it is nearly (Exposure_Data.Start_Exposure_Clear_Time) time to start the exposure.
 * <li>The array is cleared calling  CCD_DSP_Command_CLR TWICE.
 * <li>The exposure is started by calling CCD_DSP_Command_SEX.
 * <li>Enter a loop, until the readout is completed:
 * 	<ul>
 * 	<li>Get the Host Status Transfer Register value, using CCD_DSP_Command_Get_HSTR.
 * 	<li>If we are not reading out, and have more than Exposure_Data.Readout_Remaining_Time milliseconds 
 * 		left of exposure, use CCD_DSP_Command_RET to get the current elapsed exposure time.
 * 	<li>If the exposure length minus the current elapsed exposure time is less than 
 * 		Exposure_Data.Readout_Remaining_Time milliseconds, switch exposure status to READOUT.
 * 	<li>If we are in readout mode, use CCD_DSP_Command_Get_Readout_Progress to get how many pixels
 * 		we have read out.
 * 	<li>Check to see if we have finished reading out.
 * 	<li>Check to see whether we have been aborted.
 *	</ul>
 * <li>Get a pointer to the read out reply data, using CCD_Interface_Get_Reply_Data.
 * <li>If we are reading out a full frame, call CCD_Pixel_Stream_Post_Readout_Full_Frame. Otherwise call
 *     CCD_Pixel_Stream_Post_Readout_Window.
 * </ul>
 * The Exposure_Data.Exposure_Status is changed to reflect the operation being performed on the CCD.
 * If the exposure is aborted at any stage the routine returns. CCD_Pixel_Stream_Delete_Fits_Images is
 * called to attempt to delete the blank FITS files, if the routine fails or is aborted.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param clear_array An integer representing a boolean. This should be set to TRUE if we wish to
 * 	manually clear the array before the exposure starts, FALSE if we do not. This is usually TRUE.
 * @param open_shutter TRUE if the shutter is to be opened over the duration of the exposure, FALSE if the
 * 	shutter should remain closed. The shutter may not want to be opened if a calibration image is
 * 	being taken.
 * @param start_time The time to start the exposure. If both the fields in the <i>struct timespec</i> are zero,
 * 	the exposure can be started at any convenient time.
 * @param exposure_time The length of time to open the shutter for in milliseconds. This must be greater than zero,
 * 	and less than the maximum exposure length CCD_DSP_EXPOSURE_MAX_LENGTH.
 * @param filename_list A list of filenames to save the exposure into. This is normally of length 1,unless 
 *        we are windowing, in which case there will be one filename for each window.
 * @param filename_count The number of filenames in the filename_list.
 * @return Returns TRUE if the exposure succeeds and the file is saved, returns FALSE if an error
 *	occurs or the exposure is aborted.
 * @see #EXPOSURE_HSTR_HTF_BITS
 * @see #CCD_EXPOSURE_HSTR_READOUT
 * @see #CCD_EXPOSURE_HSTR_BIT_SHIFT
 * @see #EXPOSURE_READ_TIMEOUT
 * @see #EXPOSURE_ADDRESS_SHDEL
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see #Exposure_Shutter_Control
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Delete_Fits_Images
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Post_Readout_Full_Frame
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Post_Readout_Window
 * @see ccd_setup.html#CCD_Setup_Get_Setup_Complete
 * @see ccd_setup.html#CCD_Setup_Get_Window_Flags
 * @see ccd_setup.html#CCD_Setup_Get_Readout_Pixel_Count
 * @see ccd_setup.html#CCD_Setup_Get_DeInterlace_Type
 * @see ccd_dsp.html#CCD_DSP_Command_CLR
 * @see ccd_dsp.html#CCD_DSP_Command_SET
 * @see ccd_dsp.html#CCD_DSP_Command_SEX
 * @see ccd_dsp.html#CCD_DSP_Command_Get_HSTR
 * @see ccd_dsp.html#CCD_DSP_Command_RET
 * @see ccd_dsp.html#CCD_DSP_Command_Get_Readout_Progress
 * @see ccd_dsp.html#CCD_DSP_EXPOSURE_MAX_LENGTH
 * @see ccd_interface.html#CCD_Interface_Get_Reply_Data
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Expose(CCD_Interface_Handle_T* handle,int clear_array,int open_shutter,
			struct timespec start_time,int exposure_time,
			char **filename_list,int filename_count)
{
	struct timespec sleep_time,current_time;
#ifndef _POSIX_TIMERS
	struct timeval gtod_current_time;
#endif
	unsigned short *exposure_data = NULL;
	int elapsed_exposure_time,done;
	int status,window_flags,i;
	int expected_pixel_count,current_pixel_count,last_pixel_count,readout_timeout_count,shdel;

	Exposure_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Expose(handle=%p,clear_array=%d,"
			      "open_shutter=%d,start_time_sec=%ld,exposure_time=%d,filename_count=%d) started.",
			      handle,clear_array,open_shutter,start_time.tv_sec,exposure_time,filename_count);
#endif
/* reset abort flag */
	CCD_DSP_Set_Abort(FALSE);
/* we shouldn't be able to expose until setup has been successfully completed - check this */
/* However we can do this whilst using command line programs, as calling test_dsp_download and 
** test_analogue_power is roughly equivalent to CCD_Setup_Startup */
/*
	if(!CCD_Setup_Get_Setup_Complete(handle))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 1;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Exposure failed:Setup was not complete");
		return FALSE;
	}
*/
/* check parameter ranges */
	if(!CCD_GLOBAL_IS_BOOLEAN(clear_array))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 6;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Illegal value:clear_array = %d.",
			clear_array);
		return FALSE;
	}
	if(!CCD_GLOBAL_IS_BOOLEAN(open_shutter))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 2;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Illegal value:open_shutter = %d.",
			open_shutter);
		return FALSE;
	}
	if((exposure_time < 0)||(exposure_time > CCD_DSP_EXPOSURE_MAX_LENGTH))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 3;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Illegal value:exposure_time = %d",exposure_time);
		return FALSE;
	}
	if(filename_count < 0)
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 7;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Illegal value:filename_count = %d",filename_count);
		return FALSE;
	}
	window_flags = CCD_Setup_Get_Window_Flags(handle);
	if((window_flags == 0)&&(filename_count > 1))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 8;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Too many filenames for window_flags %d:"
			"filename_count = %d",window_flags,filename_count);
		return FALSE;
	}
/* get information from setup that we need to do an exposure */
	expected_pixel_count = CCD_Setup_Get_Readout_Pixel_Count(handle);
	if(expected_pixel_count <= 0)
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 9;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Illegal expected pixel count '%d'.",
			expected_pixel_count);
		return FALSE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 4;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted");
		return FALSE;
	}
/* setup the shutter control bit - which determines whether the SEX command has
** control to open and close the shutter at the appropriate times */
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			      "CCD_Exposure_Expose(handle=%p):Setting shutter control(%d).",
			      handle,open_shutter);
#endif
	if(!Exposure_Shutter_Control(handle,open_shutter))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		return FALSE;
	}
	if(CCD_DSP_Get_Abort())
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 5;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted");
		return FALSE;
	}
/* Save the exposure length for FITS headers etc */
	handle->Exposure_Data.Exposure_Length = exposure_time;
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			      "CCD_Exposure_Expose(handle=%p):Original exposure length(%d).",
			      handle,exposure_time);
#endif
	/* if we are using the shutter modify the exposure length by various shutter delays */
	if(open_shutter)
	{
		handle->Exposure_Data.Modified_Exposure_Length = exposure_time+Exposure_Data.Shutter_Trigger_Delay-
			Exposure_Data.Shutter_Close_Delay;
#if LOGGING > 4
		CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				      "CCD_Exposure_Expose(handle=%p):Modified exposure length:%d = %d + %d - %d.",
				      handle,handle->Exposure_Data.Modified_Exposure_Length,
				      handle->Exposure_Data.Exposure_Length,Exposure_Data.Shutter_Trigger_Delay,
				      Exposure_Data.Shutter_Close_Delay);
#endif
	}
	else
	{
		/* for biases and darks the modified exposure length is the exposure length */
		handle->Exposure_Data.Modified_Exposure_Length = exposure_time;
	}
	/* actually write the modified exposure length to the timing board, to be used by the SDSU controller */
	if(!CCD_DSP_Command_SET(handle,handle->Exposure_Data.Modified_Exposure_Length))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 23;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Setting exposure time failed.");
		return FALSE;
	}
	/* configure Y:SHDEL (the delay between telling the shutter to close and starting to readout the CCD,
	** to be SCD+RD */
	shdel = Exposure_Data.Shutter_Close_Delay + Exposure_Data.Readout_Delay;
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			      "CCD_Exposure_Expose(handle=%p):Setting Y:SHDEL to %d = %d + %d.",
			      handle,shdel,Exposure_Data.Shutter_Close_Delay,Exposure_Data.Readout_Delay);
#endif
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,EXPOSURE_ADDRESS_SHDEL,shdel) 
	   != CCD_DSP_DON)
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 32;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Setting timing board SHDEL failed.");
		return FALSE;
	}
	if(CCD_DSP_Get_Abort())
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		Exposure_Error_Number = 25;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted");
		return FALSE;
	}
/* initialise variables */
/* We will use the start_time parameter to determine when to start the exposure IF 
** it's seconds are greater then zero */ 
/* do the clear array a few seconds before the exposure is due to start */
	if(start_time.tv_sec > 0)
	{
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_WAIT_START;
		done = FALSE;
		while(done == FALSE)
		{
#ifdef _POSIX_TIMERS
			clock_gettime(CLOCK_REALTIME,&current_time);
#else
			gettimeofday(&gtod_current_time,NULL);
			current_time.tv_sec = gtod_current_time.tv_sec;
			current_time.tv_nsec = gtod_current_time.tv_usec*CCD_GLOBAL_ONE_MICROSECOND_NS;
#endif
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				       "CCD_Exposure_Expose(handle=%p):Waiting for exposure start time (%ld,%ld).",
				       handle,current_time.tv_sec,start_time.tv_sec);
#endif
		/* if we've time, sleep for a second */
			if((start_time.tv_sec - current_time.tv_sec) > Exposure_Data.Start_Exposure_Clear_Time)
			{
				sleep_time.tv_sec = 1;
				sleep_time.tv_nsec = 0;
				nanosleep(&sleep_time,NULL);
			}
			else
				done = TRUE;
		/* check - have we been aborted? */
			if(CCD_DSP_Get_Abort())
			{
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
				Exposure_Error_Number = 37;
				sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted.");
				return FALSE;
			}
		}/* end while */
	}
/* clear the array */
	if(clear_array)
	{
#if LOGGING > 4
       		CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Expose():Clearing CCD array.");
#endif
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_CLEAR;
		/* we call CLR twice here. This is because CLR only parallel clocks 1024 times,
		** and we need 2049 times to clear the array. Setting NPCLR to 2049 causes CLR to timeout TOUT though.
		*/
		for(i=0; i< 2; i++)
		{
			if(!CCD_DSP_Command_CLR(handle))
			{
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
				Exposure_Error_Number = 38;
				sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Clear Array failed(%d).",i);
				return FALSE;
			}
		}/* end for */
	}/* end if clear array */
/* check - have we been aborted? */
	if(CCD_DSP_Get_Abort())
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
		Exposure_Error_Number = 20;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted.");
		return FALSE;
	}
/* Send the command to start the exposure, and monitor for completion. */
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Expose(handle=%p):Starting Exposure.",handle);
#endif
	/* Exposure status is set in CCD_DSP_Command_SEX, as this routine sleeps before starting
	** the exposure. */
	if(!CCD_DSP_Command_SEX(handle,start_time,handle->Exposure_Data.Modified_Exposure_Length))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
		Exposure_Error_Number = 39;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:SEX command failed(%ld,%ld,%d).",
			start_time.tv_sec,start_time.tv_nsec,handle->Exposure_Data.Modified_Exposure_Length);
		return FALSE;
	}
/* wait while the exposure is taken and read out */
	done = FALSE;
        elapsed_exposure_time = 0;
	current_pixel_count = 0;
	last_pixel_count = 0;
	while(done == FALSE)
	{
#if LOGGING > 4
		CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			       "CCD_Exposure_Expose(handle=%p):Getting Host Status Transfer Register.",handle);
#endif
		if(!CCD_DSP_Command_Get_HSTR(handle,&status))
		{
			CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
			handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
			Exposure_Error_Number = 40;
			sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Getting HSTR failed.");
			return FALSE;
		}
#if LOGGING > 9
		CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Expose(handle=%p):HSTR is %#x.",
				      handle,status);
#endif
		status = (status & EXPOSURE_HSTR_HTF_BITS) >> CCD_EXPOSURE_HSTR_BIT_SHIFT;
#if LOGGING > 9
		CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				      "CCD_Exposure_Expose(handle=%p):HSTR reply bits %#x.",handle,status);
#endif
		if(status != CCD_EXPOSURE_HSTR_READOUT)
		{
			/* are we about to start reading out? */
			if((handle->Exposure_Data.Modified_Exposure_Length - elapsed_exposure_time) >= 
			   Exposure_Data.Readout_Remaining_Time)
			{
#if LOGGING > 4
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				       "CCD_Exposure_Expose(handle=%p):Getting Elapsed exposure time.",handle);
#endif
				/* get elapsed time from controller */
				elapsed_exposure_time = CCD_DSP_Command_RET(handle);
#if LOGGING > 9
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				       "CCD_Exposure_Expose(handle=%p):Elapsed exposure time is %#x.",
						      handle,elapsed_exposure_time);
#endif
				if (elapsed_exposure_time < 0)
					elapsed_exposure_time = 0;
				if(elapsed_exposure_time == 0)
				{
					if(CCD_DSP_Get_Error_Number() != 0)
						CCD_DSP_Error();
				}
			}/* end if there is over Exposure_Data.Readout_Remaining_Time milliseconds of exposure left */
			if((handle->Exposure_Data.Exposure_Status == CCD_EXPOSURE_STATUS_EXPOSE)&&
			   ((handle->Exposure_Data.Modified_Exposure_Length - elapsed_exposure_time) < 
			    Exposure_Data.Readout_Remaining_Time))
			{
				/* Here we change the Exposure status to PRE_READOUT, when there
				** is less than Exposure_Data.Readout_Remaining_Time milliseconds of 
				** exposure time left. 
				** The exposure status is checked in WRM,RDM,TDL and RET commands, 
				** so we can't send these commands when in readout mode. 
				** We switch to exposure readout Exposure_Data.Readout_Remaining_Time milliseconds 
				** early as we sleep for a second at the bottom of the loop, and the HSTR status
				** may change before we check it again. */
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_PRE_READOUT;
#if LOGGING > 4
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						      "CCD_Exposure_Expose(handle=%p):Exposure Status "
						      "changed to PRE_READOUT %d milliseconds before readout starts.",
				    handle,(handle->Exposure_Data.Modified_Exposure_Length - elapsed_exposure_time));
#endif
			}/* end if there  is less than Exposure_Data.Readout_Remaining_Time milliseconds 
			 ** of exposure time left */
		}/* end if HSTR status is not readout */
		/* Testing whether the status is CCD_EXPOSURE_HSTR_READOUT can fail to be detected, 
		** if it is in this state for less than one second (i.e. dual amplifier readout with binning 4)
		** We could try the following test to get round this:
		**    if(handle->Exposure_Data.Exposure_Status == CCD_EXPOSURE_STATUS_PRE_READOUT and
		**       exposure_time - elapsed_exposure_time < 0)
		**         handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_READOUT
		** However this won't work as we go into PRE_READOUT (which stops updating elapsed_exposure_time)
		** 1500 ms before the exposure fails.
		** See below for solution.
		*/
		if(status == CCD_EXPOSURE_HSTR_READOUT)
		{
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				       "CCD_Exposure_Expose(handle=%p):HSTR Status is READOUT.",handle);
#endif
			/* is this the first time through the loop we have detected readout mode? */
			if(handle->Exposure_Data.Exposure_Status != CCD_EXPOSURE_STATUS_READOUT)
			{
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_READOUT;
#if LOGGING > 4
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				 "CCD_Exposure_Expose(handle=%p):Exposure Status changed to READOUT(HSTR).",handle);
#endif
			}
		}
		/* We want to get the readout progress after we have moved into exposure status readout.
		** We want to continue getting readout progress after the HSTR status has come out of readout mode,
		** to get the progress of the last few bytes read out whilst we were sleeping. 
		** We used to only get readout progress when:
		** (handle->Exposure_Data.Exposure_Status == CCD_EXPOSURE_STATUS_READOUT), (i.e. during and after
		** we had detected HSTR register status to be CCD_EXPOSURE_HSTR_READOUT)
		** However we can miss detecting readout mode, if the whole readout takes less than 1 second.
		*/
#if LOGGING > 4
		CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
			       "CCD_Exposure_Expose(handle=%p):Getting Readout Progress.",handle);
#endif
		last_pixel_count = current_pixel_count;
		if(!CCD_DSP_Command_Get_Readout_Progress(handle,&current_pixel_count))
		{
			CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
			handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
			Exposure_Error_Number = 41;
			sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Get Readout Progress failed.");
			return FALSE;
		}
#if LOGGING > 9
		CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				      "CCD_Exposure_Expose(handle=%p):Readout progress is %#x of %#x pixels.",
				      handle,current_pixel_count,expected_pixel_count);
#endif
		/* If the current pixel count is greater than zero, we must be reading out, right? 
	        ** Correct, see IIA in START_EXPOSURE (timCCDmisc.asm), INITIALIZE_NUMBER_OF_PIXELS in pciboot.asm,
		** READ_NUMBER_OF_PIXELS_READ (0x8075) in pciboot.asm, and READ_PIXEL_COUNT/ASTROPCI_GET_PROGRESS in
		** astropci.c. */
		if(current_pixel_count > 0)
		{
			/* is this the first time through the loop we have detected readout mode? */
			if(handle->Exposure_Data.Exposure_Status != CCD_EXPOSURE_STATUS_READOUT)
			{
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_READOUT;
#if LOGGING > 4
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						      "CCD_Exposure_Expose(handle=%p):"
						   "Exposure Status changed to READOUT(current_pixel_count).",handle);
#endif
			}
		}
		/* We can only have a readout timeout, if we are in readout mode. */
		if(handle->Exposure_Data.Exposure_Status == CCD_EXPOSURE_STATUS_READOUT)
		{
			/* has the pixel count changed. If not, increment timeout */
			if(current_pixel_count == last_pixel_count)
				readout_timeout_count++;
			else
				readout_timeout_count = 0;
			/* have we timed out? If so, exit loop. */
			if(readout_timeout_count == EXPOSURE_READ_TIMEOUT)
			{
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
#if LOGGING > 9
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
				       "CCD_Exposure_Expose(handle=%p):Readout timeout has occured.",handle);
#endif
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
				Exposure_Error_Number = 43;
				sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Readout timed out.");
				return FALSE;
			}
		} 
		/* check - have we been aborted? */
		if(CCD_DSP_Get_Abort())
		{
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
					      "CCD_Exposure_Expose(handle=%p):Abort detected.",handle);
#endif
			if(handle->Exposure_Data.Exposure_Status == CCD_EXPOSURE_STATUS_EXPOSE)
			{
#if LOGGING > 4
				CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
						      "CCD_Exposure_Expose(handle=%p):Trying AEX.",handle);
#endif
				if(CCD_DSP_Command_AEX(handle) != CCD_DSP_DON)
				{
					CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
					handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
					Exposure_Error_Number = 15;
					sprintf(Exposure_Error_String,"CCD_Exposure_Expose:AEX Abort command failed.");
					return FALSE;
				}
				CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
				/* we now only abort when exposure status is STATUS_EXPOSE. */
				handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
				Exposure_Error_Number = 42;
				sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted.");
				return FALSE;
			}
			/* If the exposure status is PRE_READOUT, we should wait until it is READOUT,
			** and the call ABR. */
			/* If the exposure status is READOUT, we should call ABR.
			** However ABR seems to be causing lockups even when called correctly.
			** So for now we let the READOUT continue until it is finished,
			** then an ABORT check after the exposure/readout loop will catch the abort. */
		}
		/* check - all pixels read out? */
		if(current_pixel_count >= expected_pixel_count)
		{
#if LOGGING > 9
			CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,
					      "CCD_Exposure_Expose(handle=%p):Readout completed.",handle);
#endif
			done = TRUE;
		}
		/* sleep for a bit */
		sleep_time.tv_sec = 1;
		sleep_time.tv_nsec = 0;
		nanosleep(&sleep_time,NULL);
	}/* end while not done */
	/* did the readout time out?*/
	if(readout_timeout_count == EXPOSURE_READ_TIMEOUT)
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
		Exposure_Error_Number = 30;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Readout timed out.");
		return FALSE;
	}
/* check - have we been aborted? */
	if(CCD_DSP_Get_Abort())
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
		Exposure_Error_Number = 24;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted.");
		return FALSE;
	}
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Expose(handle=%p):Getting reply data.",handle);
#endif
	/* get data */
	if(!CCD_Interface_Get_Reply_Data(handle,&exposure_data))
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
		Exposure_Error_Number = 44;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Failed to get reply data.");
		return FALSE;
	}
	handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_POST_READOUT;
/* did we abort? */
	if(CCD_DSP_Get_Abort())
	{
		CCD_Pixel_Stream_Delete_Fits_Images(filename_list,filename_count);
		handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
		Exposure_Error_Number = 26;
		sprintf(Exposure_Error_String,"CCD_Exposure_Expose:Aborted.");
		return FALSE;
	}
/* post-readout processing depends on whether we are windowing or not. */
	if(window_flags == 0)
	{
		if(CCD_Pixel_Stream_Post_Readout_Full_Frame(handle,exposure_data,filename_list[0]) == FALSE)
		{
			/* Do not call CCD_Pixel_Stream_Delete_Fits_Images here - we may have saved to disk */
			handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
			return FALSE;
		}
	}
	else
	{
		if(CCD_Pixel_Stream_Post_Readout_Window(handle,exposure_data,filename_list,filename_count) == FALSE)
		{
			/* Do not call CCD_Pixel_Stream_Delete_Fits_Images here - we may have saved to disk */
			handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
			return FALSE;
		}
	}
/* reset exposure status */
	handle->Exposure_Data.Exposure_Status = CCD_EXPOSURE_STATUS_NONE;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Expose(handle=%p) returned TRUE.",handle);
#endif
	return TRUE;
}

/**
 * Routine to take a bias frame. Calls CCD_Exposure_Expose with clear_array TRUE, open_shutter FALSE and 
 * zero exposure length. Note assumes single readout filename, will not work if setup is windowed.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param filename The filename to save the resultant data (in FITS format) to.
 * @return The routine returns TRUE if the operation was completed successfully, FALSE if it failed.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Bias(CCD_Interface_Handle_T* handle,char *filename)
{
	struct timespec start_time;
	char *filename_list[1];

	start_time.tv_sec = 0;
	start_time.tv_nsec = 0;
	filename_list[0] = filename;
	return CCD_Exposure_Expose(handle,TRUE,FALSE,start_time,0,filename_list,1);
}

/**
 * This routine would not normally be called as part of an exposure sequence. It simply opens the shutter by 
 * executing an Open Shutter command.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE if the operation succeeded, FALSE if it fails.
 * @see ccd_dsp.html#CCD_DSP_Command_OSH
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Open_Shutter(CCD_Interface_Handle_T* handle)
/* a seperarate command to the main exposure sequence */
{
	Exposure_Error_Number = 0;
	if(!CCD_DSP_Command_OSH(handle))
	{
		CCD_DSP_Set_Abort(FALSE);
		Exposure_Error_Number = 11;
		sprintf(Exposure_Error_String,"CCD_Exposure_Open_Shutter:Open shutter failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * This routine would not normally be called as part of an exposure sequence. It simply closes the shutter by 
 * executing a close shutter command.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE if the operation succeeded, FALSE if it fails.
 * @see ccd_dsp.html#CCD_DSP_Command_CSH
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Close_Shutter(CCD_Interface_Handle_T* handle)
/* a seperarate command to the main exposure sequence */
{
	Exposure_Error_Number = 0;
	if(!CCD_DSP_Command_CSH(handle))
	{
		CCD_DSP_Set_Abort(FALSE);
		Exposure_Error_Number = 12;
		sprintf(Exposure_Error_String,"CCD_Exposure_Close_Shutter:Close shutter failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * This routine pauses an exposure currently underway by 
 * executing a pause exposure command.
 * The timer is paused until the exposure is resumed.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE if the operation succeeded, FALSE if it fails.
 * @see #CCD_Exposure_Resume
 * @see ccd_dsp.html#CCD_DSP_Command_PEX
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Pause(CCD_Interface_Handle_T* handle)
/* a seperarate command to the main exposure sequence */
{
	Exposure_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Pause() started.");
#endif
	if(!CCD_DSP_Command_PEX(handle))
	{
		CCD_DSP_Set_Abort(FALSE);
		Exposure_Error_Number = 13;
		sprintf(Exposure_Error_String,"CCD_Exposure_Pause:Pause command failed.");
		return FALSE;
	}
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Pause() returned TRUE.");
#endif
	return TRUE;
}

/**
 * This routine resumes a paused exposure by 
 * executing a resume exposure command.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE if the operation succeeded, FALSE if it fails.
 * @see #CCD_Exposure_Pause
 * @see ccd_dsp.html#CCD_DSP_Command_REX
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Resume(CCD_Interface_Handle_T* handle)
/* a seperarate command to the main exposure sequence */
{
	Exposure_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Resume() started.");
#endif
	if(!CCD_DSP_Command_REX(handle))
	{
		CCD_DSP_Set_Abort(FALSE);
		Exposure_Error_Number = 14;
		sprintf(Exposure_Error_String,"CCD_Exposure_Resume:Resume command failed.");
		return FALSE;
	}
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Resume() returned TRUE.");
#endif
	return TRUE;
}

/**
 * This routine aborts an exposure currenly underway, whether it is reading out or not.
 * This routine sets the Abort flag to true by calling CCD_DSP_Set_Abort(TRUE).
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return Returns TRUE if the abort succeeds  returns FALSE if an error occurs.
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see #CCD_Exposure_Expose
 * @see #CCD_Exposure_Get_Exposure_Status
 * @see #CCD_DSP_Set_Abort
 * @see ccd_dsp.html#CCD_DSP_Set_Abort
 * @see ccd_dsp.html#CCD_EXPOSURE_STATUS
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Abort(CCD_Interface_Handle_T* handle)
{
	Exposure_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Abort() started with exposure status %d.",
			      handle->Exposure_Data.Exposure_Status);
#endif
	CCD_DSP_Set_Abort(TRUE);
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"CCD_Exposure_Abort() finished.");
#endif
	return TRUE;
}


/**
 * This routine is not called as part of the normal exposure sequence. It is used to read out a ccd exposure
 * under manual control or to read out an aborted exposure. 
 * If the readout is aborted at any stage the routine returns.
 * Note assumes single readout filename, will not work if setup is windowed.
 * This routine just calls CCD_Exposure_Expose, with clear_array FALSE (to prevent destruction of the image),
 * open_shutter FALSE, and an exposure length of zero.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param filename The filename to save the exposure into.
 * @return Returns TRUE if the readout succeeds and the file is saved, returns FALSE if an error
 *	occurs or the readout is aborted.
 * @see #CCD_Exposure_Expose
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Read_Out_CCD(CCD_Interface_Handle_T* handle,char *filename)
{
	struct timespec start_time;
	char *filename_list[1];

	start_time.tv_sec = 0;
	start_time.tv_nsec = 0;
	filename_list[0] = filename;
	return CCD_Exposure_Expose(handle,FALSE,FALSE,start_time,0,filename_list,1);
}

/**
 * Routine to set the current value of the exposure status.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param status The exposure status.
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see #CCD_EXPOSURE_STATUS
 * @see #CCD_EXPOSURE_IS_STATUS
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Set_Exposure_Status(CCD_Interface_Handle_T* handle,enum CCD_EXPOSURE_STATUS status)
{
	if(!CCD_EXPOSURE_IS_STATUS(status))
	{
		Exposure_Error_Number = 72;
		sprintf(Exposure_Error_String,"CCD_Exposure_Set_Exposure_Status:Status illegal value (%d).",status);
		return FALSE;
	}
	handle->Exposure_Data.Exposure_Status = status;
	return TRUE;
}

/**
 * This routine gets the current value of Exposure Status.
 * Exposure_Status is defined in Exposure_Data.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The current status of exposure.
 * @see #CCD_EXPOSURE_STATUS
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
enum CCD_EXPOSURE_STATUS CCD_Exposure_Get_Exposure_Status(CCD_Interface_Handle_T* handle)
{
	return handle->Exposure_Data.Exposure_Status;
}

/**
 * This routine gets the current value of Exposure Length.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The last exposure length.
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Exposure_Get_Exposure_Length(CCD_Interface_Handle_T* handle)
{
	return handle->Exposure_Data.Exposure_Length;
}

/**
 * This routine gets the time stamp for the start of the exposure.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The time stamp for the start of the exposure.
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
struct timespec CCD_Exposure_Get_Exposure_Start_Time(CCD_Interface_Handle_T* handle)
{
	return handle->Exposure_Data.Exposure_Start_Time;
}

/**
 * Routine to set how many seconds before an exposure is due to start we wish to send the CLEAR_ARRAY
 * command to the controller.
 * @param time The time in seconds. This should be greater than the time the CLEAR_ARRAY command takes to
 * 	clock all accumulated charge off the CCD (approx 5 seconds for a 2kx2k EEV42-40).
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
void CCD_Exposure_Set_Start_Exposure_Clear_Time(int time)
{
	Exposure_Data.Start_Exposure_Clear_Time = time;
}

/**
 * Routine to get the current setting for how many seconds before an exposure is due to start we wish 
 * to send the CLEAR_ARRAY command to the controller.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
int CCD_Exposure_Get_Start_Exposure_Clear_Time(void)
{
	return Exposure_Data.Start_Exposure_Clear_Time;
}

/**
 * Routine to set the amount of time, in milliseconds, before the desired start of exposure that we should send the
 * START_EXPOSURE command, to allow for transmission delay.
 * @param time The time, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
void CCD_Exposure_Set_Start_Exposure_Offset_Time(int time)
{
	Exposure_Data.Start_Exposure_Offset_Time = time;
}

/**
 * Routine to get the amount of time, in milliseconds, before the desired start of exposure that we should send the
 * START_EXPOSURE command, to allow for transmission delay.
 * @return The time, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
int CCD_Exposure_Get_Start_Exposure_Offset_Time(void)
{
	return Exposure_Data.Start_Exposure_Offset_Time;
}

/**
 * Routine to set the amount of time, in milleseconds, remaining for an exposure when we change status to READOUT, 
 * to stop RDM/TDL/WRMs affecting the readout.
 * @param time The time, in milliseconds. Note, because the exposure time is read every second, it is best
 * 	not have have this constant an exact multiple of 1000.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
void CCD_Exposure_Set_Readout_Remaining_Time(int time)
{
	Exposure_Data.Readout_Remaining_Time = time;
}

/**
 * Routine to get the amount of time, in milliseconds, remaining for an exposure when we change status to READOUT, 
 * to stop RDM/TDL/WRMs affecting the readout.
 * @return The time, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
int CCD_Exposure_Get_Readout_Remaining_Time(void)
{
	return Exposure_Data.Readout_Remaining_Time;
}

/**
 * Routine to set the delay between the Uniblitz CS90 shutter controller getting a signal to open the
 * shutter (from the SDSU utility board), and it starting to do so.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @param delay_ms The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern void CCD_Exposure_Shutter_Trigger_Delay_Set(int delay_ms)
{
	Exposure_Data.Shutter_Trigger_Delay = delay_ms;
}

/**
 * Routine to get the delay between the Uniblitz CS90 shutter controller getting a signal to open the
 * shutter (from the SDSU utility board), and it starting to do so.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @return The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern int CCD_Exposure_Shutter_Trigger_Delay_Get(void)
{
	return Exposure_Data.Shutter_Trigger_Delay;
}

/**
 * Routine to set the length of time the Uniblitz CS90 shutter controller takes to open the shutter.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @param delay_ms The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern void CCD_Exposure_Shutter_Open_Delay_Set(int delay_ms)
{
	Exposure_Data.Shutter_Open_Delay = delay_ms;
}

/**
 * Routine to get the length of time the Uniblitz CS90 shutter controller takes to open the shutter.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @return The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern int CCD_Exposure_Shutter_Open_Delay_Get(void)
{
	return Exposure_Data.Shutter_Open_Delay;
}

/**
 * Routine to set the length of time the Uniblitz CS90 shutter controller takes to close the shutter.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @param delay_ms The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern void CCD_Exposure_Shutter_Close_Delay_Set(int delay_ms)
{
	Exposure_Data.Shutter_Close_Delay = delay_ms;
}

/**
 * Routine to get the length of time the Uniblitz CS90 shutter controller takes to close the shutter.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @return The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern int CCD_Exposure_Shutter_Close_Delay_Get(void)
{
	return Exposure_Data.Shutter_Close_Delay;
}

/**
 * Routine to set the length of time to wait after the shutter close delay, before starting to readout.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @param delay_ms The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern void CCD_Exposure_Readout_Delay_Set(int delay_ms)
{
	Exposure_Data.Readout_Delay = delay_ms;
}

/**
 * Routine to get the length of time to wait after the shutter close delay, before starting to readout.
 * See the IO:O "O Shutter Timing" documentation for more details.
 * @param delay_ms The delay length, in milliseconds.
 * @see #Exposure_Struct
 * @see #Exposure_Data
 */
extern int CCD_Exposure_Readout_Delay_Get(void)
{
	return Exposure_Data.Readout_Delay;
}

/**
 * Routine to set the Exposure_Start_Time of Exposure_Data, to the current time of the real time clock.
 * clock_gettime or gettimeofday is used, depending on whether _POSIX_TIMERS is defined.
 * We then add the Shutter Trigger Time to the captured time, as this delay occurs after the SEX command is sent,
 * before the shutter bagins to open.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @see ccd_exposure_private.html#CCD_Exposure_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_global.html#CCD_Global_Add_Time_Ms
 */
void CCD_Exposure_Set_Exposure_Start_Time(CCD_Interface_Handle_T* handle)
{
#ifndef _POSIX_TIMERS
	struct timeval gtod_current_time;
#endif

#ifdef _POSIX_TIMERS
	clock_gettime(CLOCK_REALTIME,&(handle->Exposure_Data.Exposure_Start_Time));
#else
	gettimeofday(&gtod_current_time,NULL);
	handle->Exposure_Data.Exposure_Start_Time.tv_sec = gtod_current_time.tv_sec;
	handle->Exposure_Data.Exposure_Start_Time.tv_nsec = gtod_current_time.tv_usec*CCD_GLOBAL_ONE_MICROSECOND_NS;
#endif
	/* add shutter trigger delay to get correct time */
	CCD_Global_Add_Time_Ms(&(handle->Exposure_Data.Exposure_Start_Time),Exposure_Data.Shutter_Trigger_Delay);
}

/**
 * Get the current value of the ccd_exposure error number.
 * @return The current value of the ccd_exposure error number.
 */
int CCD_Exposure_Get_Error_Number(void)
{
	return Exposure_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_exposure in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Exposure_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Exposure_Error_Number == 0)
		sprintf(Exposure_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Exposure:Error(%d) : %s\n",time_string,Exposure_Error_Number,Exposure_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_exposure in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Exposure_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Exposure_Error_Number == 0)
		sprintf(Exposure_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Exposure:Error(%d) : %s\n",time_string,
		Exposure_Error_Number,Exposure_Error_String);
}

/**
 * The warning routine that reports any warnings occuring in ccd_exposure in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Exposure_Warning(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an warning message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an warning to display */
	if(Exposure_Error_Number == 0)
		sprintf(Exposure_Error_String,"Logic Error:No Warning defined");
	fprintf(stderr,"%s CCD_Exposure:Warning(%d) : %s\n",time_string,Exposure_Error_Number,Exposure_Error_String);
}

/* ----------------------------------------------------------------
**	Internal routines
** ---------------------------------------------------------------- */
/**
 * Internal exposure routine that sets the shutter bit. The SDSU SEX command looks
 * at this bit to determine whether to open and close the shutter whilst performing an exposure.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param open_shutter If it is TRUE, the SDSU SEX command will open/close the 
 * 	shutter at the relevant places during an exposure. If it is FALSE, the shutter will remain in it's 
 * 	current state throughout the exposure.
 * @return Returns TRUE if the operation succeeded, FALSE if it fails.
 * @see #EXPOSURE_ADDRESS_CONTROLLER_STATUS
 * @see #CCD_Exposure_Expose
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_dsp.html#CCD_DSP_Command_WRM
 * @see ccd_dsp.html#CCD_DSP_CONTROLLER_STATUS_OPEN_SHUTTER_BIT
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Exposure_Shutter_Control(CCD_Interface_Handle_T* handle,int open_shutter)
{
	int current_status;

#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"Exposure_Shutter_Control:Started.");
#endif
/* check parameter */
	if(!CCD_GLOBAL_IS_BOOLEAN(open_shutter))
	{
		Exposure_Error_Number = 21;
		sprintf(Exposure_Error_String,"Exposure_Shutter_Control:Illegal open_shutter value:%d.",
			open_shutter);
		return FALSE;
	}
/* get current controller status */
	current_status = CCD_DSP_Command_RDM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_X,
					     EXPOSURE_ADDRESS_CONTROLLER_STATUS);
	if((current_status == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		Exposure_Error_Number = 28;
		sprintf(Exposure_Error_String,"Exposure_Shutter_Control: Reading timing status failed.");
		return FALSE;
	}
/* set or clear open shutter bit */
	if(open_shutter)
		current_status |= CCD_DSP_CONTROLLER_STATUS_OPEN_SHUTTER_BIT;
	else
		current_status &= ~(CCD_DSP_CONTROLLER_STATUS_OPEN_SHUTTER_BIT);
/* write new controller status value */
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_X,
			       EXPOSURE_ADDRESS_CONTROLLER_STATUS,current_status) != CCD_DSP_DON)
	{
		Exposure_Error_Number = 22;
		sprintf(Exposure_Error_String,"Exposure_Shutter_Control: Writing shutter bit failed.");
		return FALSE;
	}
#if LOGGING > 4
	CCD_Global_Log(LOG_VERBOSITY_INTERMEDIATE,"Exposure_Shutter_Control:Completed.");
#endif
	return TRUE;
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.6  2012/07/19 14:07:46  cjm
** Changed PARALLEL_SPLIT De-Interlace so image is correcly orientated.
** Commeneted out CCD_Exposure_Flip_X as this is not needed for BOTHRIGHT readout,
** is was needed for QUAD readout.
**
** Revision 1.5  2012/07/17 16:55:02  cjm
** Added flipping code.
** Added flip in X to readouts.
** Changed CCD_Setup NCols/NRows calls to match API change.
**
** Revision 1.4  2012/06/13 14:40:53  cjm
** Fixed Exposure_DeInterlace split parallel de interlace code.
**
** Revision 1.3  2012/01/11 15:04:55  cjm
** Changed SPLIT_PARALLEL deinterlace to allow for both amplifiers being on the right.
**
** Revision 1.2  2011/11/23 10:59:52  cjm
** Added Modified Exposure time which is set based on shuttering effects.
** Fixed de-interlacing.
**
** Revision 1.1  2011/09/28 13:07:53  cjm
** Initial revision
**
** Revision 0.36  2009/02/05 11:40:27  cjm
** Swapped Bitwise for Absolute logging levels.
**
** Revision 0.35  2008/12/11 16:50:25  cjm
** Added handle logging for multiple CCD system.
**
** Revision 0.34  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 0.33  2006/05/17 18:06:20  cjm
** Fixed unused variables and mismatches sprintfs.
**
** Revision 0.32  2006/05/16 14:14:02  cjm
** gnuify: Added GNU General Public License.
**
** Revision 0.31  2005/02/04 17:46:49  cjm
** Added Exposure_Expose_Delete_Fits_Images.
** Most of the time, FITS filename passed into CCD_Exposure_Expose that
** do not subsequently get filled with read out data get deleted.
** There are some places (e.g. Exposure_Save) where they do not.
**
** Revision 0.30  2004/08/02 16:34:58  cjm
** Added CCD_DSP_DEINTERLACE_FLIP.
**
** Revision 0.29  2004/06/03 16:23:23  cjm
** Calling ABR when exposure status is READOUT seems to cause occasional lockups.
** So we now only abort when exposing.
** If we abort when in readout mode, the software will wait until it exits the
** exposure/readout loop to catch the abort.
**
** Revision 0.28  2004/05/16 15:34:11  cjm
** Added CCD_EXPOSURE_STATUS_NONE set if ABR failed.
**
** Revision 0.27  2004/05/16 14:28:18  cjm
** Re-wrote abort code.
**
** Revision 0.26  2003/12/08 15:04:00  cjm
** CCD_EXPOSURE_STATUS_WAIT_START added.
**
** Revision 0.25  2003/11/04 14:42:00  cjm
** Minor MJD fixes based on errors in Linux versions of the code.
**
** Revision 0.24  2003/03/26 15:44:48  cjm
** Added windowing code.
**
** Revision 0.23  2003/03/04 17:09:53  cjm
** Added NGAT_Astro call.
**
** Revision 0.22  2002/12/16 16:49:36  cjm
** Fixed problems with status during an exposure, so that it only goes into
** PRE_READOUT at the correct time.
** Removed Error routines resetting error number to zero.
**
** Revision 0.21  2002/11/08 12:13:19  cjm
** CCD_DSP_Command_SEX now changes the exposure status to READOUT immediately,
** if the exposure length is small enough.
**
** Revision 0.20  2002/11/07 19:13:39  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 0.19  2001/06/04 14:38:00  cjm
** Changed DEBUG to LOGGING.
** Added readout process priority changes and memory locking code.
**
** Revision 0.18  2001/02/16 09:55:18  cjm
** Added more detail to error messages.
**
** Revision 0.17  2001/02/09 18:30:40  cjm
** comment spelling.
**
** Revision 0.16  2001/02/05 14:30:09  cjm
** Added checks to CCD_Exposure_Bias to STP/IDL called
** only when Idling was configured at startup.
**
** Revision 0.15  2001/01/23 18:21:27  cjm
** Added check for maximum exposure length CCD_DSP_EXPOSURE_MAX_LENGTH.
**
** Revision 0.14  2000/09/25 09:51:28  cjm
** Changes to use with v1.4 SDSU DSP code.
**
** Revision 0.13  2000/07/14 16:25:44  cjm
** Backup.
**
** Revision 0.12  2000/07/11 10:42:24  cjm
** Removed CCD_Exposure_Flush_CCD.
**
** Revision 0.11  2000/06/20 12:53:07  cjm
** CCD_DSP_Command_Sex now automatically calls CCD_DSP_Command_RDI.
**
** Revision 0.10  2000/06/19 08:48:34  cjm
** Backup.
**
** Revision 0.9  2000/06/13 17:14:13  cjm
** Changes to make Ccs agree with voodoo.
**
** Revision 0.8  2000/05/23 10:34:46  cjm
** Added call to CCD_DSP_Set_Exposure_Start_Time in CCD_Exposure_Bias,
** so that bias frames now have an exposure start time set,
** which gives a sensible value for DATE, DATE-OBS and UTSTART in FITS files.
**
** Revision 0.7  2000/05/10 14:37:52  cjm
** Removed number of bytes parameter from CCD_DSP_Command_RDI.
**
** Revision 0.6  2000/04/13 13:17:36  cjm
** Added current time to error routines.
**
** Revision 0.5  2000/03/13 12:30:17  cjm
** Removed duplicate CCD_DSP_Set_Abort(FALSE) in CCD_Exposure_Bias.
**
** Revision 0.4  2000/02/28 19:13:01  cjm
** Backup.
**
** Revision 0.3  2000/02/22 16:05:21  cjm
** Changed call structure to CCD_DSP_Set_Abort.
**
** Revision 0.2  2000/02/01 17:50:01  cjm
** Changed references to CCD_Setup_Setup_CCD to CCD_Setup_Get_Setup_Complete.
**
** Revision 0.1  2000/01/25 14:57:27  cjm
** initial revision (PCI version).
**
*/
