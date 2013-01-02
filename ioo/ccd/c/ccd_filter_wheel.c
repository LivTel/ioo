/* ccd_filter_wheel.c
** filter wheel control module.
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_filter_wheel.c,v 1.2 2013-01-02 11:19:16 cjm Exp $
*/
/**
 * ccd_filter_wheel holds the routines for moving and controlling the filter wheel.
 * It hold state on the current position of the wheel.
 * @author Chris Mottram
 * @version $Revision: 1.2 $
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#ifndef _POSIX_TIMERS
#include <sys/time.h>
#endif
#include "log_udp.h"
#include "ccd_global.h"
#include "ccd_dsp.h"
#include "ccd_filter_wheel.h"

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_filter_wheel.c,v 1.2 2013-01-02 11:19:16 cjm Exp $";
/**
 * The default number of positions in each filter wheel.
 * @see #Filter_Wheel_Struct
 */
#define FW_DEFAULT_POSITION_COUNT		(12)
/**
 * How often, in milliseconds the SERVICE routine is executed.
 * This is 0.8.
 */
#define FW_ISR_SPEED				(0.8)
/**
 * The memory location on the utility board, X data area, of the status word.
 * This location should be kept up to date with the memory address of X:STATUS, 
 * defined in utilboot.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_STATUS		(0x0)
/**
 * The memory location on the utility board, Y data area, of the digital input word.
 * The value held at this address is the last series of inputs read from the hardware.
 * This location should be kept up to date with the memory address of Y:DIG_IN,
 * defined in utilappl.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT	(0x0)
/**
 * The memory location on the utility board, Y data area, of the digital output word.
 * The value held at this address is the output bits to be written to the hardware.
 * This location should be kept up to date with the memory address of Y:DIG_OUT,
 * defined in utilappl.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_DIGITAL_OUTPUT	(0x1)
/**
 * The memory location on the utility board, Y data area, of the position we last successfully moved to.
 * This location should be kept up to date with the memory address of Y:FW_LAST_POS,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_LAST_POS          (0x31)
/**
 * The memory location on the utility board, Y data area, of the filter wheel position we are trying to move to.
 * This location should be kept up to date with the memory address of Y:FW_TARGET_POS,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_TARGET_POS        (0x32)
/**
 * The memory location on the utility board, Y data area, holding the number of positions between the last and
 * target memory locations. This is decremented during a move as we move through positions.
 * This location should be kept up to date with the memory address of Y:FW_OFFSET_POS,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_OFFSET_POS        (0x33)
/**
 * The memory location on the utility board, Y data area, of the pattern the proximity sensors have to make
 * in Y:DIG_IN when we are in the target filter wheel position.
 * This is updated at the start of the FWM command.
 * This location should be kept up to date with the memory address of Y:FW_TARGET_PROXIMITY_PATTERN,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_TARGET_PROXIMITY_PATTERN (0x34)
/**
 * The memory location on the utility board, Y data area, of the filter wheel error code.
 * This is updated when an error occurs during moving the wheel (in the ISR or FWM/FWR/FWA commands).
 * This location should be kept up to date with the memory address of Y:FW_ERROR_CODE,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_ERROR_CODE        (0x35)
/**
 * The memory location on the utility board, Y data area, of the filter wheel move timeout index.
 * This is incremented in the ISR when moving the filter wheel or locators, and compared against
 * the timeout count to see when an operation has not completed in a certain time period.
 * This location should be kept up to date with the memory address of Y:FW_MOVE_TIMEOUT,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_MOVE_TIMEOUT_INDEX      (0x36)
/**
 * The memory location on the utility board, Y data area, of the filter wheel move timeout count.
 * This is the number used against the timeout index to determine when a timeout has occured.
 * This location should be kept up to date with the memory address of Y:FW_MOVE_TIMEOUT,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_MOVE_TIMEOUT_COUNT      (0x37)
/**
 * The memory location on the utility board, Y data area, of the start of list of proximity patterns.
 * This is a list of Y:DIG_IN inpout bits for the proximity sensors when they are in each filter wheel position.
 * The list length is the number of positions in the wheel (i.e. normally 12).
 * This location should be kept up to date with the memory address of Y:FW_PROXIMITY_PATTERN,
 * defined in filter_wheel_variables.asm.
 */
#define FW_MEM_LOC_UTIL_BOARD_PROXIMITY_PATTERN       (0x38)
/**
 * The memory location on the utility board, Y data area, for the temporary variable Y:FW_TMP1.
 * This is used as a debugging variable.
 */
#define FW_MEM_LOC_UTIL_BOARD_TMP1                    (0x44)

/**
 * The bit in the utility board, X:STATUS, that is set when a move operation is underway.
 * This bit definition should be kept up to date with ST_FW_MOVE, defined in filter_wheel_equ.asm.
 */
#define FW_STATUS_BIT_MOVE			(1<<5)
/**
 * The bit in the utility board, X:STATUS, that is set when we are reseting the filter wheel.
 * This bit definition should be kept up to date with ST_FW_RESET, defined in filter_wheel_equ.asm.
 */
#define FW_STATUS_BIT_RESET		        (1<<6)
/**
 * The bit in the utility board, X:STATUS, that is set when the locators are being moved out during a move operation.
 * This bit definition should be kept up to date with ST_FW_LOCATORS_OUT, defined in filter_wheel_equ.asm.
 */
#define FW_STATUS_BIT_LOCATORS_OUT		(1<<7)
/**
 * The bit in the utility board, X:STATUS, that is set when the wheel is being moved into a position
 * where some proximity sensor are triggered.
 * This bit definition should be kept up to date with ST_FW_MOVE_IN_POSITION, defined in filter_wheel_equ.asm.
 */
#define FW_STATUS_BIT_MOVE_IN_POSITION		(1<<8)
/**
 * The bit in the utility board, X:STATUS, that is set when the wheel is being moved out of a position
 * where some proximity sensor are triggered.
 * This bit definition should be kept up to date with ST_FW_MOVE_OUT_POSITION, defined in filter_wheel_equ.asm.
 */
#define FW_STATUS_BIT_MOVE_OUT_POSITION		(1<<9)
/**
 * The bit in the utility board, X:STATUS, that is set when the locators are being moved in during a move operation.
 * This bit definition should be kept up to date with ST_FW_LOCATORS_IN, defined in filter_wheel_equ.asm.
 */
#define FW_STATUS_BIT_LOCATORS_IN		(1<<10)

/**
 * The bit on the utility board, Y:DIG_IN, <b>CLEAR</b> when locator 1 is in the in position.
 * This bit definition should be kept up to date with DIG_IN_BIT_LOCATOR_1_IN, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_LOCATOR_1_IN			(1<<0)
/**
 * The bit on the utility board, Y:DIG_IN, <b>CLEAR</b> when locator 1 is in the out position.
 * This bit definition should be kept up to date with DIG_IN_BIT_LOCATOR_1_OUT, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_LOCATOR_1_OUT			(1<<1)
/**
 * The bit on the utility board, Y:DIG_IN, <b>CLEAR</b> when locator 2 is in the in position.
 * This bit definition should be kept up to date with DIG_IN_BIT_LOCATOR_2_IN, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_LOCATOR_2_IN			(1<<2)
/**
 * The bit on the utility board, Y:DIG_IN, <b>CLEAR</b> when locator 2 is in the out position.
 * This bit definition should be kept up to date with DIG_IN_BIT_LOCATOR_2_OUT, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_LOCATOR_2_OUT			(1<<3)
/**
 * The bit on the utility board, Y:DIG_IN, <b>CLEAR</b> when locator 3 is in the in position.
 * This bit definition should be kept up to date with DIG_IN_BIT_LOCATOR_3_IN, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_LOCATOR_3_IN			(1<<4)
/**
 * The bit on the utility board, Y:DIG_IN, <b>CLEAR</b> when locator 3 is in the out position.
 * This bit definition should be kept up to date with DIG_IN_BIT_LOCATOR_3_OUT, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_LOCATOR_3_OUT			(1<<5)
/**
 * The bit on the utility board, Y:DIG_IN, set when the motor clutch in engaged.
 * This bit definition should be kept up to date with DIG_IN_BIT_CLUTCH_ENGAGED, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_CLUTCH_ENGAGED			(1<<6)
/**
 * The bit on the utility board, Y:DIG_IN, set when the motor clutch in disengaged.
 * This bit definition should be kept up to date with DIG_IN_BIT_CLUTCH_DISENGAGED, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_CLUTCH_DISENGAGED		(1<<7)
/**
 * The bit on the utility board, Y:DIG_IN, CLEAR when proximity sensor 1 is triggered.
 * This bit definition should be kept up to date with DIG_IN_BIT_PROXIMITY_1_ON, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_1_ON			(1<<8)
/**
 * The bit on the utility board, Y:DIG_IN, CLEAR when proximity sensor 2 is triggered.
 * This bit definition should be kept up to date with DIG_IN_BIT_PROXIMITY_2_ON, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_2_ON			(1<<9)
/**
 * The bit on the utility board, Y:DIG_IN, CLEAR when proximity sensor 3 is triggered.
 * This bit definition should be kept up to date with DIG_IN_BIT_PROXIMITY_3_ON, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_3_ON			(1<<10)
/**
 * The bit on the utility board, Y:DIG_IN, CLEAR when proximity sensor 4 is triggered.
 * This bit definition should be kept up to date with DIG_IN_BIT_PROXIMITY_4_ON, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_4_ON			(1<<11)
/**
 * The bit on the utility board, Y:DIG_IN, CLEAR when proximity sensor 5 is triggered.
 * This bit definition should be kept up to date with DIG_IN_BIT_PROXIMITY_5_ON, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_5_ON			(1<<12)
/**
 * The bit on the utility board, Y:DIG_IN, CLEAR when proximity sensor 6 is triggered.
 * This bit definition should be kept up to date with DIG_IN_BIT_PROXIMITY_6_ON, defined in filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_6_ON			(1<<13)
/**
 * Mask to apply to Y:DIG_IN to extract proximity bits: bits 8-13. Should match DIG_IN_PROXIMITY_MASK in 
 * filter_wheel_equ.asm.
 */
#define FW_INPUT_PROXIMITY_MASK                 (0x3f00)
/**
 * The bit on the utility board, Y:DIG_OUT, is set to move the three locators in, 
 * and cleared to move the three locators  out.
 * This bit definition should be kept up to date with DIG_OUT_BIT_LOCATORS_IN, defined in filter_wheel_equ.asm.
 */
#define FW_OUTPUT_LOCATORS_IN                  (1<<0)
/**
 * The bit on the utility board, Y:DIG_OUT, is set to turn the motor on, and cleared to turn the motor off.
 * This bit definition should be kept up to date with DIG_OUT_BIT_MOTOR_ON, defined in filter_wheel_equ.asm.
 */
#define FW_OUTPUT_MOTOR_ON                     (1<<1)
/**
 * The bit on the utility board, Y:DIG_OUT, is set to disengage the motor clutch, 
 * and cleared to engage the motor clutch.
 * This bit definition should be kept up to date with DIG_OUT_BIT_CLUTCH_DISENGAGE, defined in filter_wheel_equ.asm.
 */
#define FW_OUTPUT_CLUTCH_DISENGAGE             (1<<2)

/* data types */
/**
 * Data type holding local data to ccd_filter_wheel. This is the current position of each wheel.
 * <dl>
 * <dt>Position</dt> <dd>The position the filter wheel has attained.
 * <dt>Position_Count</dt> <dd>The number of filter positions in each wheel. Normally twelve.</dd>
 * <dt>Status</dt> <dd>What operation the filter wheel sub-system is performing.</dd>
 * </dl>
 * @see #CCD_FILTER_WHEEL_STATUS
 */
struct Filter_Wheel_Struct
{
	int Position;
	int Position_Count;
	enum CCD_FILTER_WHEEL_STATUS Status;
};

/* external variables */

/* internal variables */
/**
 * Variable holding error code of last operation performed by ccd_filter_wheel.
 */
static int Filter_Wheel_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Filter_Wheel_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";
/**
 * Data holding the current status of ccd_filter_wheel.
 * @see #Filter_Wheel_Struct
 * @see #FW_DEFAULT_POSITION_COUNT
 */
static struct Filter_Wheel_Struct Filter_Wheel_Data = 
{
	-1,FW_DEFAULT_POSITION_COUNT,CCD_FILTER_WHEEL_STATUS_NONE
};

/* internal function definitions */
static void Filter_Wheel_Print_Status(int fw_status,int fw_move_timeout_index);
static void Filter_Wheel_Print_Digital_Inputs(int fw_dig_in);
static void Filter_Wheel_Print_Proximity(int fw_dig_in,char *description_string);
static void Filter_Wheel_Print_Digital_Outputs(int fw_dig_out);

/* -----------------------------------------------------------------------------
**     external functions 
** ----------------------------------------------------------------------------- */
/**
 * This routine sets up ccd_filter_wheel internal variables.
 * It should be called at startup.
 * @return This routine returns TRUE for success.
 * @see #FW_DEFAULT_POSITION_COUNT
 * @see #CCD_FILTER_WHEEL_STATUS
 */
int CCD_Filter_Wheel_Initialise(void)
{
	Filter_Wheel_Error_Number = 0;
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Filter_Wheel_Initialise:%s.\n",rcsid);
#ifdef CCD_FILTER_WHEEL_FAKE
	fprintf(stdout,"CCD_Filter_Wheel_Initialise:Compiled to fake filter wheel movement.\n");
#endif
	Filter_Wheel_Data.Position = -1;
	Filter_Wheel_Data.Position_Count = FW_DEFAULT_POSITION_COUNT;
	Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
	return TRUE;
}

/**
 * Routine to set how many positions in the filter wheel.
 * @param position_count How many positions. This is nominally twelve.
 * @return The routine returns TRUE if it succeeded, and FALSE if it failed.
 */
int CCD_Filter_Wheel_Position_Count_Set(int position_count)
{
	Filter_Wheel_Error_Number = 0;
	Filter_Wheel_Data.Position_Count = position_count;
	return TRUE;
}

/**
 * Routine to get how many positions in the filter wheel.
 * @return The number of positions. This is normally twelve.
 */
int CCD_Filter_Wheel_Position_Count_Get(void)
{
	return Filter_Wheel_Data.Position_Count;
}


/**
 * Routine to reset the filter wheel. This drives the wheel into the next position, and works out where
 * that position is.
 * <ul>
 * <li>We change the Filter_Wheel_Data.Position to -1 whilst we are moving the wheel.
 * <li>We set the filter wheel status to MOVING.
 * <li>We call CCD_DSP_Command_FWR to call the FWR command.
 * <li>We enter a loop:
 *     <ul>
 *     <li>We use CCD_DSP_Command_RDM to get the X:STATUS word.
 *     <li>We get the current time, compare it with a timestamp stored before entering the loop, and exit
 *         with an error if we have timed out.
 *     <li>We use CCD_DSP_Command_RDM to get Y:DIG_IN, the current digitial inputs.
 *     <li>We use CCD_DSP_Command_RDM to get Y:DIG_OUT, the current digitial outputs.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_LAST_POS, which should be set to the absolute position of the wheel
 *         at the end of the FWR routine.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_ERROR_CODE, the filter wheel DSP code error code.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_MOVE_TIMEOUT_INDEX.
 *     <li>We update the filter wheel C status based upon the contents of X:STATUS.
 *     <li>We call Filter_Wheel_Print_Status to print the current status.
 *     <li>We call Filter_Wheel_Print_Digital_Inputs to print the current digital inputs.
 *     <li>We call Filter_Wheel_Print_Digital_Outputs to print the current digital outputs.
 *     <li>We exit the loop if X:STATUS no longer has FW_STATUS_BIT_RESET bit set:we are no longer reseting the wheel.
 *     </ul>
 * <li>We use CCD_DSP_Command_RDM to get Y:FW_ERROR_CODE, the filter wheel DSP code error code.
 * <li>If the returned error code is non-zero, we return an error.
 * <li>We use CCD_DSP_Command_RDM to get Y:DIG_IN, the current digitial inputs.
 * <li>We use CCD_DSP_Command_RDM to get Y:FW_LAST_POS, which contains the wheel absolute position 
 *     if FWR was successful.
 * <li>If the reset succeeded, we update Filter_Wheel_Data.Position to the contents of Y:FW_LAST_POS.
 * </ul>
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param position The position to move the specified wheel to. An integer greater or equal to zero and less
 * 	than the wheel position count.
 * @return The routine returns TRUE if the command succeeded and FALSE if the command failed or was aborted.
 * @see #FW_MEM_LOC_UTIL_BOARD_STATUS
 * @see #FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT
 * @see #FW_MEM_LOC_UTIL_BOARD_DIGITAL_OUTPUT
 * @see #FW_MEM_LOC_UTIL_BOARD_LAST_POS
 * @see #FW_MEM_LOC_UTIL_BOARD_OFFSET_POS
 * @see #FW_MEM_LOC_UTIL_BOARD_ERROR_CODE
 * @see #FW_MEM_LOC_UTIL_BOARD_MOVE_TIMEOUT_INDEX
 * @see #FW_MEM_LOC_UTIL_BOARD_TMP1
 * @see #CCD_FILTER_WHEEL_STATUS
 * @see #FW_STATUS_BIT_LOCATORS_OUT
 * @see #FW_STATUS_BIT_MOVE_IN_POSITION
 * @see #FW_STATUS_BIT_MOVE_OUT_POSITION
 * @see #FW_STATUS_BIT_LOCATORS_IN
 * @see #FW_INPUT_PROXIMITY_MASK
 * @see #Filter_Wheel_Data
 * @see #Filter_Wheel_Print_Status
 * @see #Filter_Wheel_Print_Digital_Inputs
 * @see #Filter_Wheel_Print_Proximity
 * @see #Filter_Wheel_Print_Digital_Outputs
 * @see ccd_dsp.html#CCD_DSP_Command_FWR
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_dsp.html#CCD_DSP_BOARD_ID
 * @see ccd_dsp.html#CCD_DSP_MEM_SPACE
 */
int CCD_Filter_Wheel_Reset(CCD_Interface_Handle_T* handle)
{
	struct timespec start_time,current_time,sleep_time;
	int done,fw_status,fw_dig_in,fw_dig_out,fw_last_pos=0,fw_offset_pos;
	int fw_error_code,fw_move_timeout_index,fw_tmp1;
	double timeout_length,length_secs;

	Filter_Wheel_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset() started.");
#endif
/* wheels position is indeterminate during reset */
	Filter_Wheel_Data.Position = -1;
/* do move */
#ifndef CCD_FILTER_WHEEL_FAKE
/* change state */
	Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_MOVING;
/* send command to start move */
	if(CCD_DSP_Command_FWR(handle) != CCD_DSP_DON)
	{
		Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
		Filter_Wheel_Error_Number = 9;
		sprintf(Filter_Wheel_Error_String,"Reset of wheel failed.");
		return FALSE;
	}
/* enter loop - monitoring for completion */
	done = FALSE;
        timeout_length = 120;/* diddly */
	clock_gettime(CLOCK_REALTIME,&start_time);
	while(done == FALSE)
	{
	/* sleep for 2 milliseconds */
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = 2*1000000;
		nanosleep(&sleep_time,NULL);
	/* get utility board X:STATUS word */
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting status word.");
#endif
		fw_status = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_X,
						FW_MEM_LOC_UTIL_BOARD_STATUS);
	/* if zero returned and ccd_dsp report an error, an error occured */
		if((fw_status == 0)&&(CCD_DSP_Get_Error_Number() > 0))
		{
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
			Filter_Wheel_Error_Number = 10;
			sprintf(Filter_Wheel_Error_String,"Reset:Getting utility status failed.");
			return FALSE;
		}
	/* monitor client side computer timeout */
		clock_gettime(CLOCK_REALTIME,&current_time);
		length_secs = fdifftime(current_time,start_time);
		if(length_secs > timeout_length)
		{
			/* diddly consider FWA here */
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
			Filter_Wheel_Error_Number = 11;
			sprintf(Filter_Wheel_Error_String,"Reset:Timeout (%.2f > %.2f) for wheel.",
				length_secs,timeout_length);
			return FALSE;
		}
		/* get filter wheel internal variables for debugging */
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:DIG_IN.");
#endif
		fw_dig_in = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:DIG_OUT.");
#endif
		fw_dig_out = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						 FW_MEM_LOC_UTIL_BOARD_DIGITAL_OUTPUT);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:FW_LAST_POS.");
#endif
		fw_last_pos = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					     FW_MEM_LOC_UTIL_BOARD_LAST_POS);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:FW_OFFSET_POS.");
#endif
		fw_offset_pos = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					     FW_MEM_LOC_UTIL_BOARD_OFFSET_POS);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:FW_ERROR_CODE.");
#endif
		fw_error_code = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						    FW_MEM_LOC_UTIL_BOARD_ERROR_CODE);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:FW_MOVE_TIMEOUT_INDEX.");
#endif
		fw_move_timeout_index = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						      FW_MEM_LOC_UTIL_BOARD_MOVE_TIMEOUT_INDEX);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting Y:FW_TMP1.");
#endif
		fw_tmp1 = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					      FW_MEM_LOC_UTIL_BOARD_TMP1);
		/* update filter wheel status */
		if((fw_status & FW_STATUS_BIT_LOCATORS_OUT) == FW_STATUS_BIT_LOCATORS_OUT)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_LOCATORS_OUT;
		if((fw_status & FW_STATUS_BIT_MOVE_IN_POSITION) == FW_STATUS_BIT_MOVE_IN_POSITION)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_MOVING;
		if((fw_status & FW_STATUS_BIT_MOVE_OUT_POSITION) == FW_STATUS_BIT_MOVE_OUT_POSITION)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_MOVING;
		if((fw_status & FW_STATUS_BIT_LOCATORS_IN) == FW_STATUS_BIT_LOCATORS_IN)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_LOCATORS_IN;
		/* print some debugging info */
#if LOGGING > 0
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Reset:Last position = %d.",fw_last_pos);
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Reset:Offset position = %d.",fw_offset_pos);
#endif
		Filter_Wheel_Print_Status(fw_status,fw_move_timeout_index);
		Filter_Wheel_Print_Digital_Inputs(fw_dig_in);
		Filter_Wheel_Print_Digital_Outputs(fw_dig_out);
#if LOGGING > 0
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Reset:Current filter wheel error code = %d.",fw_error_code);
#endif
	/* keep looping until utility board is no longer reseting wheel */
		done = ((fw_status&FW_STATUS_BIT_RESET)==0);
	}/* end while */
	Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
/* the move bit is no longer set. Why? */
	/* check filter wheel error code */
	fw_error_code = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					    FW_MEM_LOC_UTIL_BOARD_ERROR_CODE);
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
			      "CCD_Filter_Wheel_Reset:Final filter wheel error code = %d.",fw_error_code);
#endif
	if((fw_error_code == 0)&&(CCD_DSP_Get_Error_Number() > 0))
	{
		Filter_Wheel_Error_Number = 12;
		sprintf(Filter_Wheel_Error_String,"Reset:Getting filter wheel error code failed.");
		return FALSE;
	}
	if(fw_error_code != 0)
	{
		Filter_Wheel_Error_Number = 13;
		sprintf(Filter_Wheel_Error_String,"Reset:Failed with error code:%d.",fw_error_code);
		return FALSE;
	}
	/* get Y:FW_LAST_POS, which should have been set by the Reset code to the position we have driven to. */
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting final Y:FW_LAST_POS.");
#endif
	fw_last_pos = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					  FW_MEM_LOC_UTIL_BOARD_LAST_POS);
#if LOGGING > 1
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Y:FW_LAST_POS position = %d.",
			      fw_last_pos);
#endif

/* get current digital inputs */
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset:Getting final Y:DIG_IN.");
#endif
	fw_dig_in = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT);
	if((fw_dig_in == 0)&&(CCD_DSP_Get_Error_Number() > 0))
	{
		Filter_Wheel_Error_Number = 14;
		sprintf(Filter_Wheel_Error_String,"Reset:Getting utility digital inputs failed.");
		return FALSE;
	}
	Filter_Wheel_Print_Proximity(fw_dig_in,"Final Actual");
#else
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset: "
		       "CCD_FILTER_WHEEL_FAKE set at compile time: Faking reset.");
#endif
#endif /* end if we are faking filter wheel movement (#ifndef CCD_FILTER_WHEEL_FAKE) */
/* wheel is at position only if reset suceeded */
	Filter_Wheel_Data.Position = fw_last_pos;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Reset() returned TRUE.");
#endif
	return TRUE;
}

/**
 * Routine to move the filter wheel to an absolute location.
 * <ul>
 * <li>The parameters are checked.
 * <li>If the wheel is already in the correct position, we return TRUE.
 * <li>We change the Filter_Wheel_Data.Position to -1 whilst we are moving the wheel.
 * <li>We set the filter wheel status to MOVING.
 * <li>We call CCD_DSP_Command_FWM to call the FWM command with the passed in position parameter.
 * <li>We enter a loop:
 *     <ul>
 *     <li>We use CCD_DSP_Command_RDM to get the X:STATUS word.
 *     <li>We get the current time, compare it with a timestamp stored before entering the loop, and exit
 *         with an error if we have timed out.
 *     <li>We use CCD_DSP_Command_RDM to get Y:DIG_IN, the current digitial inputs.
 *     <li>We use CCD_DSP_Command_RDM to get Y:DIG_OUT, the current digitial outputs.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_LAST_POS, the absolute position we started from.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_TARGET_POS, the absolute position we are heading towards.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_OFFSET_POS, the number of position we have to move 
 *         to arrive at the target.
 *     <li>We use CCD_DSP_Command_RDM to get Y:TARGET_PROXIMITY_PATTERN, the target proximity switch pattern.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_ERROR_CODE, the filter wheel DSP code error code.
 *     <li>We use CCD_DSP_Command_RDM to get Y:FW_MOVE_TIMEOUT_INDEX.
 *     <li>We update the filter wheel C status based upon the contents of X:STATUS.
 *     <li>We call Filter_Wheel_Print_Status to print the current status.
 *     <li>We call Filter_Wheel_Print_Digital_Inputs to print the current digital inputs.
 *     <li>We call Filter_Wheel_Print_Proximity to print the TARGET proximity pattern.
 *     <li>We call Filter_Wheel_Print_Digital_Outputs to print the current digital outputs.
 *     <li>We exit the loop if X:STATUS no longer has FW_STATUS_BIT_MOVE bit set:we are no longer moving the wheel.
 *     </ul>
 * <li>We use CCD_DSP_Command_RDM to get Y:FW_ERROR_CODE, the filter wheel DSP code error code.
 * <li>If the returned error code is non-zero, we return an error.
 * <li>We use CCD_DSP_Command_RDM to get Y:DIG_IN, the current digitial inputs.
 * <li>We call Filter_Wheel_Print_Proximity to print the FINAL ACTUAL proximity pattern.
 * <li>We use CCD_DSP_Command_RDM to get Y:TARGET_PROXIMITY_PATTERN, the final target proximity switch pattern.
 * <li>We call Filter_Wheel_Print_Proximity to print the FINAL TARGET proximity pattern.
 * <li>If the digital inputs ANDed with the proximity mask does not equal the target proximity pattern,
 *     we are in the wrong position and return an error.
 * <li>If the move succeeded, we update Filter_Wheel_Data.Position to the new position.
 * </ul>
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param position The position to move the specified wheel to. An integer greater or equal to zero and less
 * 	than the wheel position count (0..N-1).
 * @return The routine returns TRUE if the command succeeded and FALSE if the command failed or was aborted.
 * @see #FW_MEM_LOC_UTIL_BOARD_STATUS
 * @see #FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT
 * @see #FW_MEM_LOC_UTIL_BOARD_DIGITAL_OUTPUT
 * @see #FW_MEM_LOC_UTIL_BOARD_LAST_POS
 * @see #FW_MEM_LOC_UTIL_BOARD_TARGET_POS
 * @see #FW_MEM_LOC_UTIL_BOARD_OFFSET_POS
 * @see #FW_MEM_LOC_UTIL_BOARD_TARGET_PROXIMITY_PATTERN
 * @see #FW_MEM_LOC_UTIL_BOARD_ERROR_CODE
 * @see #FW_MEM_LOC_UTIL_BOARD_MOVE_TIMEOUT_INDEX
 * @see #CCD_FILTER_WHEEL_STATUS
 * @see #FW_STATUS_BIT_LOCATORS_OUT
 * @see #FW_STATUS_BIT_MOVE_IN_POSITION
 * @see #FW_STATUS_BIT_MOVE_OUT_POSITION
 * @see #FW_STATUS_BIT_LOCATORS_IN
 * @see #FW_INPUT_PROXIMITY_MASK
 * @see #Filter_Wheel_Data
 * @see #Filter_Wheel_Print_Status
 * @see #Filter_Wheel_Print_Digital_Inputs
 * @see #Filter_Wheel_Print_Proximity
 * @see #Filter_Wheel_Print_Digital_Outputs
 * @see ccd_dsp.html#CCD_DSP_Command_FWM
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_dsp.html#CCD_DSP_BOARD_ID
 * @see ccd_dsp.html#CCD_DSP_MEM_SPACE
 */
int CCD_Filter_Wheel_Move(CCD_Interface_Handle_T* handle,int position)
{
	struct timespec start_time,current_time,sleep_time;
	int done,fw_status,fw_dig_in,fw_dig_out,fw_last_pos,fw_target_pos,fw_offset_pos,fw_target_proximity_pattern;
	int fw_error_code,fw_move_timeout_index;
	double timeout_length,length_secs;

	Filter_Wheel_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move(position=%d) started.",position);
#endif
/* check parameters */
	if((position < 0)||(position >= Filter_Wheel_Data.Position_Count))
	{
		Filter_Wheel_Error_Number = 4;
		sprintf(Filter_Wheel_Error_String,"Move:Illegal Position '%d' (0,%d).",position,
			Filter_Wheel_Data.Position_Count);
		return FALSE;
	}
/* if we are already at the correct position, don't move and return success now */
	if(position == Filter_Wheel_Data.Position)
	{
#if LOGGING > 0
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Move(position=%d) finished:already in position.",position);
#endif
		return TRUE;
	}
	/* if the last reset/move operation failed, try to reset the wheel to a known position */
	if(Filter_Wheel_Data.Position == -1)
	{
#if LOGGING > 0
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Move(position=%d) : wheel in indetermine start position:"
				      "attempting reset to drive wheel into known position.",position);
#endif
		if(!CCD_Filter_Wheel_Reset(handle))
			return FALSE;
	}
/* wheels position is indeterminate during move */
	Filter_Wheel_Data.Position = -1;
/* do move */
#ifndef CCD_FILTER_WHEEL_FAKE
/* change state */
	Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_MOVING;
/* send command to start move */
	if(CCD_DSP_Command_FWM(handle,position) != CCD_DSP_DON)
	{
		Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
		Filter_Wheel_Error_Number = 5;
		sprintf(Filter_Wheel_Error_String,"Move of wheel failed (%d).",position);
		return FALSE;
	}
/* enter loop - monitoring for completion */
	done = FALSE;
        timeout_length = 120;/* diddly */
	clock_gettime(CLOCK_REALTIME,&start_time);
	while(done == FALSE)
	{
	/* sleep for 2 milliseconds */
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = 2*1000000;
		nanosleep(&sleep_time,NULL);
	/* get utility board X:STATUS word */
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting status word.");
#endif
		fw_status = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_X,
						FW_MEM_LOC_UTIL_BOARD_STATUS);
	/* if zero returned and ccd_dsp report an error, an error occured */
		if((fw_status == 0)&&(CCD_DSP_Get_Error_Number() > 0))
		{
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
			Filter_Wheel_Error_Number = 19;
			sprintf(Filter_Wheel_Error_String,"Move:Getting utility status failed.");
			return FALSE;
		}
	/* monitor client side computer timeout */
		clock_gettime(CLOCK_REALTIME,&current_time);
		length_secs = fdifftime(current_time,start_time);
		if(length_secs > timeout_length)
		{
			/* diddly consider FWA here */
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
			Filter_Wheel_Error_Number = 32;
			sprintf(Filter_Wheel_Error_String,"Move:Timeout (%.2f > %.2f) for wheel.",
				length_secs,timeout_length);
			return FALSE;
		}
		/* get filter wheel internal variables for debugging */
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:DIG_IN.");
#endif
		fw_dig_in = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:DIG_OUT.");
#endif
		fw_dig_out = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						 FW_MEM_LOC_UTIL_BOARD_DIGITAL_OUTPUT);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:FW_LAST_POS.");
#endif
		fw_last_pos = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					     FW_MEM_LOC_UTIL_BOARD_LAST_POS);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:FW_TARGET_POS.");
#endif
		fw_target_pos = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					     FW_MEM_LOC_UTIL_BOARD_TARGET_POS);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:FW_OFFSET_POS.");
#endif
		fw_offset_pos = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					     FW_MEM_LOC_UTIL_BOARD_OFFSET_POS);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "CCD_Filter_Wheel_Move:Getting Y:FW_TARGET_PROXIMITY_PATTERN.");
#endif
		fw_target_proximity_pattern = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
								  FW_MEM_LOC_UTIL_BOARD_TARGET_PROXIMITY_PATTERN);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:FW_ERROR_CODE.");
#endif
		fw_error_code = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						    FW_MEM_LOC_UTIL_BOARD_ERROR_CODE);
#if LOGGING > 0
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting Y:FW_MOVE_TIMEOUT_INDEX.");
#endif
		fw_move_timeout_index = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
						      FW_MEM_LOC_UTIL_BOARD_MOVE_TIMEOUT_INDEX);
		/* update filter wheel status */
		if((fw_status & FW_STATUS_BIT_LOCATORS_OUT) == FW_STATUS_BIT_LOCATORS_OUT)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_LOCATORS_OUT;
		if((fw_status & FW_STATUS_BIT_MOVE_IN_POSITION) == FW_STATUS_BIT_MOVE_IN_POSITION)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_MOVING;
		if((fw_status & FW_STATUS_BIT_MOVE_OUT_POSITION) == FW_STATUS_BIT_MOVE_OUT_POSITION)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_MOVING;
		if((fw_status & FW_STATUS_BIT_LOCATORS_IN) == FW_STATUS_BIT_LOCATORS_IN)
			Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_LOCATORS_IN;
		/* print some debugging info */
#if LOGGING > 0
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Move:Last position = %d.",fw_last_pos);
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Move:Target position = %d.",fw_target_pos);
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Move:Offset position = %d.",fw_offset_pos);
#endif
		Filter_Wheel_Print_Status(fw_status,fw_move_timeout_index);
		Filter_Wheel_Print_Digital_Inputs(fw_dig_in);
		Filter_Wheel_Print_Proximity(fw_target_proximity_pattern,"Target");
		Filter_Wheel_Print_Digital_Outputs(fw_dig_out);
#if LOGGING > 0
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "CCD_Filter_Wheel_Move:Current filter wheel error code = %d.",fw_error_code);
#endif
	/* keep looping until utility board is no longer moving wheel */
		done = ((fw_status&FW_STATUS_BIT_MOVE)==0);
	}/* end while */
	Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_NONE;
/* the move bit is no longer set. Why? */
	/* check filter wheel error code */
	fw_error_code = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					    FW_MEM_LOC_UTIL_BOARD_ERROR_CODE);
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
			      "CCD_Filter_Wheel_Move:Final filter wheel error code = %d.",fw_error_code);
#endif
	if((fw_error_code == 0)&&(CCD_DSP_Get_Error_Number() > 0))
	{
		Filter_Wheel_Error_Number = 1;
		sprintf(Filter_Wheel_Error_String,"Move:Getting filter wheel error code failed.");
		return FALSE;
	}
	if(fw_error_code != 0)
	{
		Filter_Wheel_Error_Number = 2;
		sprintf(Filter_Wheel_Error_String,"Move:Failed with error code:%d.",fw_error_code);
		return FALSE;
	}
/* get current digital inputs */
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move:Getting final Y:DIG_IN.");
#endif
	fw_dig_in = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					FW_MEM_LOC_UTIL_BOARD_DIGITAL_INPUT);
	if((fw_dig_in == 0)&&(CCD_DSP_Get_Error_Number() > 0))
	{
		Filter_Wheel_Error_Number = 20;
		sprintf(Filter_Wheel_Error_String,"Move:Getting utility digital inputs failed.");
		return FALSE;
	}
	Filter_Wheel_Print_Proximity(fw_dig_in,"Final Actual");
	/* get target proximity inputs */
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
		       "CCD_Filter_Wheel_Move:Getting final Y:TARGET_PROXIMITY_PATTERN.");
#endif
	fw_target_proximity_pattern = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
							  FW_MEM_LOC_UTIL_BOARD_TARGET_PROXIMITY_PATTERN);
	if((fw_target_proximity_pattern == 0)&&(CCD_DSP_Get_Error_Number() > 0))
	{
		Filter_Wheel_Error_Number = 3;
		sprintf(Filter_Wheel_Error_String,"Move:Getting utility target proximity pattern failed.");
		return FALSE;
	}
	Filter_Wheel_Print_Proximity(fw_target_proximity_pattern,"Final Target");
	/* does current proximity pattern match target? */
	if((fw_dig_in & FW_INPUT_PROXIMITY_MASK) != fw_target_proximity_pattern)
	{
		Filter_Wheel_Error_Number = 7;
		sprintf(Filter_Wheel_Error_String,"Move:Target/Actual proximity pattern mismatch (%#x,%#x).",
			fw_target_proximity_pattern,fw_dig_in);
		return FALSE;
	}
#else
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move: "
		       "CCD_FILTER_WHEEL_FAKE set at compile time: Faking move.");
#endif
#endif /* end if we are faking filter wheel movement (#ifndef CCD_FILTER_WHEEL_FAKE) */
/* wheel is at position only if move suceeded */
	Filter_Wheel_Data.Position = position;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Move() returned TRUE.");
#endif
	return TRUE;
}



/**
 * This routine aborts a filter wheel reset or move that is in progress, and causes them to return an error.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE if the command succeeded and FALSE if the command failed.
 * @see ccd_dsp.html#CCD_DSP_Command_FWA
 */
int CCD_Filter_Wheel_Abort(CCD_Interface_Handle_T* handle)
{
	int retval;

	Filter_Wheel_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Abort() started.");
#endif
#ifndef CCD_FILTER_WHEEL_FAKE
/* send abort */
	retval = CCD_DSP_Command_FWA(handle);
	if(retval != CCD_DSP_DON)
	{
		Filter_Wheel_Error_Number = 6;
		sprintf(Filter_Wheel_Error_String,"Abort failed.");
		return FALSE;
	}
#endif /* we are faking filter wheel movements (#ifndef CCD_FILTER_WHEEL_FAKE) */
	Filter_Wheel_Data.Status = CCD_FILTER_WHEEL_STATUS_ABORTED;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Abort() returned TRUE.");
#endif
	return TRUE;
}

/**
 * A routine to get the current absolute position of a filter wheel.
 * @param position The address of an integer to hold the wheel's current position. The current position is either
 * 	a number between zero (inclusive) and the number of positions in the wheel (exclusive), indicating
 * 	the current absolute position, or '-1', which means the position is indeterminate (the wheel is in motion,
 *	or a move/reset operation has failed).
 * @return The routine returns TRUE if the command succeeded and FALSE if the command failed.
 */
int CCD_Filter_Wheel_Get_Position(int *position)
{
	Filter_Wheel_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Get_Position() started.");
#endif
/* check parameters */
	if(position == NULL)
	{
		Filter_Wheel_Error_Number = 8;
		sprintf(Filter_Wheel_Error_String,"Get Position:Position pointer was NULL.");
		return FALSE;
	}
	(*position) = Filter_Wheel_Data.Position;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Filter_Wheel_Get_Position(position=%d) "
			      "returned TRUE.",(*position));
#endif
	return TRUE;
}

/**
 * Routine to return the current filter wheel status.
 * @see #Filter_Wheel_Data
 * @see #CCD_FILTER_WHEEL_STATUS
 * @see #CCD_Filter_Wheel_Move
 * @see #CCD_Filter_Wheel_Abort
 */
enum CCD_FILTER_WHEEL_STATUS CCD_Filter_Wheel_Get_Status(void)
{
	return Filter_Wheel_Data.Status;
}

/**
 * Get the current value of the error number.
 * @return The current value of the error number.
 * @see #Filter_Wheel_Error_Number
 */
int CCD_Filter_Wheel_Get_Error_Number(void)
{
	return Filter_Wheel_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_filter_wheel in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 * @see #Filter_Wheel_Error_Number
 * @see #Filter_Wheel_Error_String
 */
void CCD_Filter_Wheel_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Filter_Wheel_Error_Number == 0)
		sprintf(Filter_Wheel_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Filter_Wheel:Error(%d) : %s\n",time_string,
		Filter_Wheel_Error_Number,Filter_Wheel_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_filter_wheel in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * 	being passed to this routine. The routine will try to concatenate it's error string onto the end
 * 	of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Filter_Wheel_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Filter_Wheel_Error_Number == 0)
		sprintf(Filter_Wheel_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Filter_Wheel:Error(%d) : %s\n",time_string,
		Filter_Wheel_Error_Number,Filter_Wheel_Error_String);
}

/* -----------------------------------------------------------------------------
** 	internal functions 
** ----------------------------------------------------------------------------- */
/**
 * Debug and print status contained in the utility board X:STATUS register.
 * @param fw_status The value in the utility board X:STATUS register read by an RDM command.
 * @param fw_move_timeout_index The value in the utility board Y:FW_MOVE_TIMEOUT_INDEX register read by an RDM command.
 *        This is used to timeout of individual stages of the move command, so is a good indication of how
 *        many 0.8ms ISRs have been spent doing the current operation.
 * @see #FW_STATUS_BIT_MOVE
 * @see #FW_STATUS_BIT_LOCATORS_OUT
 * @see #FW_STATUS_BIT_MOVE_IN_POSITION
 * @see #FW_STATUS_BIT_MOVE_OUT_POSITION
 * @see #FW_STATUS_BIT_LOCATORS_IN
 */
static void Filter_Wheel_Print_Status(int fw_status,int fw_move_timeout_index)
{
#if LOGGING > 0
	if((fw_status & FW_STATUS_BIT_MOVE) == FW_STATUS_BIT_MOVE)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Status:Filter wheel is performing a move operation.");

	}
	if((fw_status & FW_STATUS_BIT_RESET) == FW_STATUS_BIT_RESET)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Status:Filter wheel is performing a reset operation.");

	}
 	if((fw_status & FW_STATUS_BIT_LOCATORS_OUT) == FW_STATUS_BIT_LOCATORS_OUT)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Status:Locators are moving out(%d).",fw_move_timeout_index);

	}
 	if((fw_status & FW_STATUS_BIT_MOVE_IN_POSITION) == FW_STATUS_BIT_MOVE_IN_POSITION)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Status:Wheel is being moved into a position(%d).",
				      fw_move_timeout_index);

	}
 	if((fw_status & FW_STATUS_BIT_MOVE_OUT_POSITION) == FW_STATUS_BIT_MOVE_OUT_POSITION)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Status:Wheel is being moved out of a position(%d).",
				      fw_move_timeout_index);

	}
 	if((fw_status & FW_STATUS_BIT_LOCATORS_IN) == FW_STATUS_BIT_LOCATORS_IN)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Status:Locators are moving in(%d).",fw_move_timeout_index);

	}
#endif
}

/**
 * Debug and print digital inputs contained in the utility board Y:DIG_IN register.
 * @param fw_dig_in The value in the utility board Y:DIG_IN register read by an RDM command.
 * @see #Filter_Wheel_Print_Proximity
 * @see #FW_INPUT_LOCATOR_1_IN
 * @see #FW_INPUT_LOCATOR_1_OUT
 * @see #FW_INPUT_LOCATOR_2_IN
 * @see #FW_INPUT_LOCATOR_2_OUT
 * @see #FW_INPUT_LOCATOR_3_IN
 * @see #FW_INPUT_LOCATOR_3_OUT
 * @see #FW_INPUT_CLUTCH_ENGAGED
 * @see #FW_INPUT_CLUTCH_DISENGAGED
 */
static void Filter_Wheel_Print_Digital_Inputs(int fw_dig_in)
{
#if LOGGING > 0
	/* locators sensors are active when the input bit is CLEAR */
	if((fw_dig_in & FW_INPUT_LOCATOR_1_IN) == 0)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 1 is in.");
	}
	else if((fw_dig_in & FW_INPUT_LOCATOR_1_OUT) == 0)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 1 is out.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 1 is neither in nor out.");
	}
	if((fw_dig_in & FW_INPUT_LOCATOR_2_IN) == 0)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 2 is in.");
	}
	else if((fw_dig_in & FW_INPUT_LOCATOR_2_OUT) == 0)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 2 is out.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 2 is neither in nor out.");
	}
	if((fw_dig_in & FW_INPUT_LOCATOR_3_IN) == 0)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 3 is in.");
	}
	else if((fw_dig_in & FW_INPUT_LOCATOR_3_OUT) == 0)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 3 is out.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Locator 3 is neither in nor out.");
	}
	if((fw_dig_in & FW_INPUT_CLUTCH_ENGAGED) == 0)/* diddly 0 means sesnor made according to tests */
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Motor clutch in engaged.");
	}
	else if((fw_dig_in & FW_INPUT_CLUTCH_DISENGAGED) == 0)/* diddly 0 means sesnor made according to tests */
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Motor clutch in disengaged.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Inputs:Motor clutch is neither engaged nor disengaged.");
	}
	Filter_Wheel_Print_Proximity(fw_dig_in,"Filter_Wheel_Print_Digital_Inputs");
#endif
}

/**
 * Debug and print proximity sensor digital inputs contained in the utility board Y:DIG_IN register,
 * or in a target proximity variable (Y:FW_TARGET_PROXIMITY_PATTERN).
 * Note the proximity sensor is reading proximity when the digital input bit is <b>CLEAR</b>.
 * @param fw_dig_in The value in the utility board Y:DIG_IN register or target proximity variable
 *        (Y:FW_TARGET_PROXIMITY_PATTERN)read by an RDM command.
 * @param description_string How to describe the proximity bits printed.
 * @see #FW_INPUT_PROXIMITY_1_ON
 * @see #FW_INPUT_PROXIMITY_2_ON
 * @see #FW_INPUT_PROXIMITY_3_ON
 * @see #FW_INPUT_PROXIMITY_4_ON
 * @see #FW_INPUT_PROXIMITY_5_ON
 * @see #FW_INPUT_PROXIMITY_6_ON
 */
static void Filter_Wheel_Print_Proximity(int fw_dig_in,char *description_string)
{
#if LOGGING > 0
	if((fw_dig_in & FW_INPUT_PROXIMITY_1_ON) == 0)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 1 is on.",description_string);
	}
	else
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 1 is off.",description_string);
	}
	if((fw_dig_in & FW_INPUT_PROXIMITY_2_ON) == 0)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 2 is on.",description_string);
	}
	else
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 2 is off.",description_string);
	}
	if((fw_dig_in & FW_INPUT_PROXIMITY_3_ON) == 0)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 3 is on.",description_string);
	}
	else
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 3 is off.",description_string);
	}
	if((fw_dig_in & FW_INPUT_PROXIMITY_4_ON) == 0)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 4 is on.",description_string);
	}
	else
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 4 is off.",description_string);
	}
	if((fw_dig_in & FW_INPUT_PROXIMITY_5_ON) == 0)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 5 is on.",description_string);
	}
	else
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 5 is off.",description_string);
	}
	if((fw_dig_in & FW_INPUT_PROXIMITY_6_ON) == 0)
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 6 is on.",description_string);
	}
	else
	{
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
				      "Filter_Wheel_Print_Proximity:%s:Proximity Sensor 6 is off.",description_string);
	}
#endif
}

/**
 * Debug and print digital outputs contained in the utility board Y:DIG_OUT register.
 * @param fw_dig_in The value in the utility board Y:DIG_OUT register read by an RDM command.
 * @see #FW_OUTPUT_LOCATORS_IN
 * @see #FW_OUTPUT_MOTOR_ON
 * @see #FW_OUTPUT_CLUTCH_ENGAGE
 */
static void Filter_Wheel_Print_Digital_Outputs(int fw_dig_out)
{
#if LOGGING > 0
	if((fw_dig_out & FW_OUTPUT_LOCATORS_IN) == FW_OUTPUT_LOCATORS_IN)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Outputs:Locators are set to drive in.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Outputs:Locators are set to drive out.");
	}
	if((fw_dig_out & FW_OUTPUT_MOTOR_ON) == FW_OUTPUT_MOTOR_ON)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Outputs:Motor is turned on.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Outputs:Motor is turned off.");
	}
	if((fw_dig_out & FW_OUTPUT_CLUTCH_DISENGAGE) == FW_OUTPUT_CLUTCH_DISENGAGE)
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Outputs:Clutch is set to disengage.");
	}
	else
	{
		CCD_Global_Log(LOG_VERBOSITY_VERY_VERBOSE,
			       "Filter_Wheel_Print_Digital_Outputs:Clutch is set to engage.");
	}
#endif
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.1  2011/11/23 10:59:52  cjm
** Initial revision
**
*/

