/* ccd_dsp.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_dsp.h,v 1.3 2012-07-17 16:53:43 cjm Exp $
*/
#ifndef CCD_DSP_H
#define CCD_DSP_H
#include <time.h>
#include "ccd_interface.h"

/**
 * This enum defines board identifiers.
 * These are usually written to the PCI boards using CCD_PCI_IOCTL_SET_DESTINATION, after being ored with
 * the number of arguments left shifted 16 bits.
 * <ul>
 * <li>CCD_DSP_HOST_BOARD_ID The source/destination byte for the host computer.
 * <li>CCD_DSP_INTERFACE_BOARD_ID The source/destination byte for the computer/controller interface device. 
 * <li>CCD_DSP_TIM_BOARD_ID The source/destination byte for the timing board.
 * <li>CCD_DSP_UTIL_BOARD_ID The source/destination byte for the utility board.
 * </ul>
 */
enum CCD_DSP_BOARD_ID
{
	CCD_DSP_HOST_BOARD_ID=0,CCD_DSP_INTERFACE_BOARD_ID=1,CCD_DSP_TIM_BOARD_ID=2,
	CCD_DSP_UTIL_BOARD_ID=3
};

/**
 * Macro to check whether the board_id is a legal board_id. Note, currently does not include the
 * manual board in the test, this should never be used.
 */
#define CCD_DSP_IS_BOARD_ID(board_id)	(((board_id) == CCD_DSP_HOST_BOARD_ID)|| \
				((board_id) == CCD_DSP_INTERFACE_BOARD_ID)||((board_id) == CCD_DSP_TIM_BOARD_ID)|| \
				((board_id) == CCD_DSP_UTIL_BOARD_ID))

/**
 * Enum with values identifying which memory space an address refers to, for operations sich as reading and writing
 * memory locations.
 * <ul>
 * <li>CCD_DSP_MEM_SPACE_R is the ROM memory space.
 * <li>CCD_DSP_MEM_SPACE_P is the P DSP memory space.
 * <li>CCD_DSP_MEM_SPACE_X is the X DSP memory space.
 * <li>CCD_DSP_MEM_SPACE_Y is the Y DSP memory space.
 * </ul>
 * @see #CCD_DSP_Command_RDM
 * @see #CCD_DSP_Command_WRM
 */
enum CCD_DSP_MEM_SPACE
{
	CCD_DSP_MEM_SPACE_P=0x100000,	/*  Bit 20 */
	CCD_DSP_MEM_SPACE_X=0x200000,	/*  Bit 21 */
	CCD_DSP_MEM_SPACE_Y=0x400000,	/*  Bit 22 */
	CCD_DSP_MEM_SPACE_R=0x800000	/*  Bit 23 */
};

/**
 * Macro to check whether the mem_space  is a legal memory space.
 */
#define CCD_DSP_IS_MEMORY_SPACE(mem_space)	(((mem_space) == CCD_DSP_MEM_SPACE_P)|| \
	((mem_space) == CCD_DSP_MEM_SPACE_X)||((mem_space) == CCD_DSP_MEM_SPACE_Y)|| \
	((mem_space) == CCD_DSP_MEM_SPACE_R))

/* These enum definitions should match with those in CCDLibrary.java */
/**
 * These are allowable parameters for the gains (Gen two only).  Please
 * note that unlike the other commmands listed in this file, the hex numbers
 * are NOT the ASCII (character) values of the commands.
 * Gain parameter sent with the <a href="#CCD_DSP_SGN">SGN</a>(set gain) command. 
 * <ul>
 * <li>CCD_DSP_GAIN_ONE Sets the gain to 1.
 * <li>CCD_DSP_GAIN_TWO Sets the gain to 2.
 * <li>CCD_DSP_GAIN_FOUR Sets the gain to 4.75.
 * <li>CCD_DSP_GAIN_NINE Sets the gain to 9.5.
 * </ul>
 */
enum CCD_DSP_GAIN
{
	CCD_DSP_GAIN_ONE=0x1,
	CCD_DSP_GAIN_TWO=0x2,	/* 2 gain */
	CCD_DSP_GAIN_FOUR=0x5,	/* 4.75 gain */
	CCD_DSP_GAIN_NINE=0xa	/* 9.5 gain */
};

/**
 * Macro to check whether the gain is a legal value to passed into the <a href="#CCD_DSP_SGN">SGN</a> command.
 */
#define CCD_DSP_IS_GAIN(gain)	(((gain) == CCD_DSP_GAIN_ONE)||((gain) == CCD_DSP_GAIN_TWO)|| \
	((gain) == CCD_DSP_GAIN_FOUR)||((gain) == CCD_DSP_GAIN_NINE))

/* These #define/enum definitions should match with those in CCDLibrary.java */
/**
 * The maximum signed integer that the controller can hold.
 * This is limited by the size of a DSP word (24 bits). The word is signed, so this value is
 * (2^23)-1.
 */
#define CCD_DSP_MAX_SIGNED_INT			(8388607)
/**
 * The maximum exposure length that the controller can expose the CCD for, in milliseconds.
 * This is limited by the size of a DSP word (24 bits). The word is signed, so this value is
 * (2^23)-1. This is 8388 seconds, or 2 hours 19 minutes, 48.607 seconds.
 */
#define CCD_DSP_EXPOSURE_MAX_LENGTH		(CCD_DSP_MAX_SIGNED_INT)

/* These enum definitions should match with those in CCDLibrary.java */
/**
 * Deinterlace type. The possible values are:
 * <ul>
 * <li>CCD_DSP_DEINTERLACE_SINGLE - This setting does no deinterlacing, 
 * 	as the CCD was read out from a single readout.
 * <li>CCD_DSP_DEINTERLACE_FLIP_X - This setting flips the output image in X, if the CCD was readout from the
 *     "wrong" amplifier, i.e. to ensure east is to the left.
 * <li>CCD_DSP_DEINTERLACE_FLIP_Y - This setting flips the output image in Y, if the CCD was readout from the
 *     "wrong" amplifier, i.e. to ensure north is to the top.
 * <li>CCD_DSP_DEINTERLACE_FLIP_XY - This setting flips the output image in X and Y, if the CCD was readout from the
 *     "wrong" amplifier, i.e. to ensure east is to the left and north is to the top.
 * <li>CCD_DSP_DEINTERLACE_SPLIT_PARALLEL - This setting deinterlaces split parallel readout.
 * <li>CCD_DSP_DEINTERLACE_SPLIT_SERIAL - This setting deinterlaces split serial readout.
 * <li>CCD_DSP_DEINTERLACE_SPLIT_QUAD - This setting deinterlaces split quad readout.
 * </ul>
 */
enum CCD_DSP_DEINTERLACE_TYPE
{
	CCD_DSP_DEINTERLACE_SINGLE,CCD_DSP_DEINTERLACE_FLIP_X,CCD_DSP_DEINTERLACE_FLIP_Y,
	CCD_DSP_DEINTERLACE_FLIP_XY,CCD_DSP_DEINTERLACE_SPLIT_PARALLEL,
	CCD_DSP_DEINTERLACE_SPLIT_SERIAL,CCD_DSP_DEINTERLACE_SPLIT_QUAD
};

/**
 * Macro to check whether the deinterlace type is a legal value.
 */
#define CCD_DSP_IS_DEINTERLACE_TYPE(type)	(((type) == CCD_DSP_DEINTERLACE_SINGLE)|| \
	((type) == CCD_DSP_DEINTERLACE_FLIP_X)||((type) == CCD_DSP_DEINTERLACE_FLIP_Y)|| \
	((type) == CCD_DSP_DEINTERLACE_FLIP_XY)||((type) == CCD_DSP_DEINTERLACE_SPLIT_PARALLEL)|| \
        ((type) == CCD_DSP_DEINTERLACE_SPLIT_SERIAL)||((type) == CCD_DSP_DEINTERLACE_SPLIT_QUAD))

/* These enum definitions should match with those in CCDLibrary.java */
/**
 * Enum with values identifying which output amplifier to select for reading out the CCD.
 * Used with the CCD_DSP_SOS (select output source) manual command.
 * <ul>
 * <li>CCD_DSP_AMPLIFIER_TOP_LEFT selects the top left amplifier.
 * <li>CCD_DSP_AMPLIFIER_TOP_RIGHT selects the top right amplifier.
 * <li>CCD_DSP_AMPLIFIER_BOTTOM_LEFT selects the bottom left amplifier.
 * <li>CCD_DSP_AMPLIFIER_BOTTOM_RIGHT selects the bottom right amplifier.
 * <li>CCD_DSP_AMPLIFIER_BOTH_LEFT selects the top and bottom left amplifier.
 * <li>CCD_DSP_AMPLIFIER_BOTH_RIGHT selects the top and bottom right amplifier.
 * <li>CCD_DSP_AMPLIFIER_ALL selects all amplifiers.
 * </ul>
 * @see #CCD_DSP_SOS
 * @see #CCD_DSP_Command_SOS
 */
enum CCD_DSP_AMPLIFIER
{
	CCD_DSP_AMPLIFIER_TOP_LEFT    =0x5f5f41, 	/* Ascii __A */
	CCD_DSP_AMPLIFIER_TOP_RIGHT   =0x5f5f42, 	/* Ascii __B */
	CCD_DSP_AMPLIFIER_BOTTOM_LEFT =0x5f5f43, 	/* Ascii __C */
	CCD_DSP_AMPLIFIER_BOTTOM_RIGHT=0x5f5f44, 	/* Ascii __D */
	CCD_DSP_AMPLIFIER_BOTH_LEFT   =0x5f4143,        /* Ascii _AC */
	CCD_DSP_AMPLIFIER_BOTH_RIGHT  =0x5f4244,        /* Ascii _BD */
	CCD_DSP_AMPLIFIER_ALL         =0x414c4c  	/* Ascii ALL */
};

/**
 * Macro to check whether the amplifier is a legal value to passed into the <a href="#CCD_DSP_SOS">SOS</a> command.
 */
#define CCD_DSP_IS_AMPLIFIER(amplifier)	(((amplifier) == CCD_DSP_AMPLIFIER_TOP_LEFT)|| \
	((amplifier) == CCD_DSP_AMPLIFIER_TOP_RIGHT)||((amplifier) == CCD_DSP_AMPLIFIER_BOTTOM_LEFT)|| \
	((amplifier) == CCD_DSP_AMPLIFIER_BOTTOM_RIGHT)||((amplifier) == CCD_DSP_AMPLIFIER_BOTH_LEFT)|| \
        ((amplifier) == CCD_DSP_AMPLIFIER_BOTH_RIGHT)||((amplifier) == CCD_DSP_AMPLIFIER_ALL))

/* Various CCD_DSP routine return these values to indicate success/failure */
/**
 * Return value from the SDSU CCD Controller. This means the last command succeeded.
 */
#define CCD_DSP_DON		(0x444f4e) /* DON */
/**
 * Return value from the SDSU CCD Controller. This means the last command failed.
 */
#define CCD_DSP_ERR		(0x455252) /* ERR */
/**
 * Device Driver value, used by the device driver when it times out whilst waiting for a reply value.
 * This means the GET_REPLY command sent to the device driver did not receive a reply from the PCI DSP.
 */
#define CCD_DSP_TOUT		(0x544F5554) /* TOUT */
/**
 * Timing board command that means SYstem Reset. This is sent in reply to a reset
 * controller command.
 */
#define CCD_DSP_SYR		(0x535952) /* SYR */
/**
 * Another kind of reset command. This one isn't used at the moment.
 */
#define CCD_DSP_RST		(0x00525354) /* RST */

/* Manual DSP commands. */
/**
 * Manual command sent to any board, to Test the Data Link. Ensure we can communicate with the specified board.
 */
#define CCD_DSP_TDL		(0x54444C)	/* TDL */
/**
 * Read Memory command. Read a memory location on a specified SDSU controller board.
 */
#define CCD_DSP_RDM		(0x52444D)	/* RDM */
/**
 * Read CCD. Command to read out the CCD on the timing board.
 */
#define CCD_DSP_RDC		(0x524443)	/* RDC */
/**
 * Write memory command. Write a value to a specified location on a specified SDSU controller board.
 */
#define CCD_DSP_WRM		(0x57524D)	/* WRM */
/**
 * Start EXposure command.
 */
#define CCD_DSP_SEX		(0x534558)	/* SEX */
/**
 * Set Exposure Time command.
 */
#define CCD_DSP_SET		(0x534554)	/* SET */
/**
 * Pause EXposure command.
 */
#define CCD_DSP_PEX		(0x504558)	/* PEX */
/**
 * Resume EXposure command.
 */
#define CCD_DSP_REX		(0x524558)	/* REX */
/**
 * Read Elapsed exposure Time.
 */
#define CCD_DSP_RET		(0x524554)	/* RET */
/**
 * Abort EXposure command.
 */
#define CCD_DSP_AEX		(0x414558)	/* AEX */
/**
 * Power ON command.
 */
#define CCD_DSP_PON		(0x504F4E)	/* PON */
/**
 * Power OFf command.
 */
#define CCD_DSP_POF		(0x504F46)	/* POF */
/**
 * Timing board command that means Set Output Source. This sets which output amplifiers on the chip to read
 * out from when a readout is performed. It takes one argument, which selects the amplifier.
 * @see #CCD_DSP_AMPLIFIER
 */
#define CCD_DSP_SOS		(0x534f53)	/* SOS */
/**
 * Timing board command that means Set GaiN. This sets the gains of all the video processors. The integrator
 * speed is also set using this command to slow or fast.
 */
#define CCD_DSP_SGN		(0x53474e)	/* SGN */
/**
 * Set Subarray Size command.
 */
#define CCD_DSP_SSS		(0x535353)	/* SSS */
/**
 * Set Subarray Position command.
 */
#define CCD_DSP_SSP		(0x535350)	/* SSP */
/**
 * Load Application command.
 */
#define CCD_DSP_LDA		(0x4C4441)	/* LDA */
/**
 * Read Controller Configuration command.
 */
#define CCD_DSP_RCC		(0x524343)	/* RCC */
/**
 * CLeaR array command.
 */
#define CCD_DSP_CLR		(0x434C52)	/* CLR */
/**
 * IDLe clock the CCD array command.
 */
#define CCD_DSP_IDL		(0x49444C)	/* IDL */
/**
 * SToP idle clocking command.
 */
#define CCD_DSP_STP		(0x535450)	/* STP */
/**
 * Close SHutter command.
 */
#define CCD_DSP_CSH		(0x435348)	/* CSH */
/**
 * Open SHutter command.
 */
#define CCD_DSP_OSH		(0x4F5348)	/* OSH */
/**
 * Utility board command that means Filter Wheel Abort. This stops any filter wheel movement taking place. 
 * It takes no arguments.
 * @see #CCD_DSP_FWM
 */
#define CCD_DSP_FWA		(0x465741)	/* FWA */
/**
 * Utility board command that means Filter Wheel Move. This moves the filter wheel to a specified position. 
 * It takes one argument:
 * <ul>
 * <li><b>positions</b>. The absolute position to move the wheel, a number, between 0 and 11.
 * </ul>
 */
#define CCD_DSP_FWM		(0x46574d)	/* FWM */
/**
 * Utility board command that means Filter Wheel Reset. This moves the wheel to a known position. 
 * It takes no arguments.
 */
#define CCD_DSP_FWR		(0x465752)	/* FWR */
/**
 * Generate Waveform command.
 */
#define CCD_DSP_GWF		(0x475746)	/* GWF */

/**
 * This hash definition represents one of the bits present in the controller status word, which is on
 * the timing board in X memory at location 0.
 * This is retrieved using READ_MEMORY and set using WRITE_MEMORY.
 * When set, this bit means START_EXPOSURE commands sent to the controller will open the shutter.
 * The value should be shifted by the same number as the #SHUT defined in timboot.asm.
 */
#define CCD_DSP_CONTROLLER_STATUS_OPEN_SHUTTER_BIT	(1 << 11)

/**
 * This hash definition represents one of the bits present in the timing board controller configuration word.
 * This is retrieved using RCC.
 * @see #CCD_DSP_RCC
 * @see #CCD_DSP_Command_RCC
 */
#define CCD_DSP_CONTROLLER_CONFIG_BIT_CCD_REV3B			(0x0)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_CCD_GENI			(0x1)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_IR_REV4C			(0x2)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_IR_COADDER		(0x3)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_TIM_REV4B			(0x0)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_TIM_GENI			(0x8)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_UTILITY_REV3		(0x20)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_SHUTTER			(0x80)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_NONLINEAR_TEMP_CONV	(0x100)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_LINEAR_TEMP_CONV		(0x200)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_SUBARRAY			(0x400)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_BINNING			(0x800)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_SERIAL_SPLIT		(0x1000)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_PARALLEL			(0x2000)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_BOTH_READOUTS		(0x3000)
#define CCD_DSP_CONTROLLER_CONFIG_BIT_MPP_CAPABLE		(0x4000)

extern int CCD_DSP_Initialise(void);
/* Boot commands */
extern int CCD_DSP_Command_LDA(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,int application_number);
extern int CCD_DSP_Command_RDM(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,enum CCD_DSP_MEM_SPACE mem_space,int address);
extern int CCD_DSP_Command_TDL(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,int data);
extern int CCD_DSP_Command_WRM(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,enum CCD_DSP_MEM_SPACE mem_space,int address,int data);
/* timing board commands */
extern int CCD_DSP_Command_ABR(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_CLR(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_RDC(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_IDL(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_SBV(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_SGN(CCD_Interface_Handle_T* handle,enum CCD_DSP_GAIN gain,int speed);
extern int CCD_DSP_Command_SOS(CCD_Interface_Handle_T* handle,enum CCD_DSP_AMPLIFIER amplifier);
extern int CCD_DSP_Command_SSP(CCD_Interface_Handle_T* handle,int y_offset,int x_offset,int bias_x_offset);
extern int CCD_DSP_Command_SSS(CCD_Interface_Handle_T* handle,int bias_width,int box_width,int box_height);
extern int CCD_DSP_Command_STP(CCD_Interface_Handle_T* handle);

extern int CCD_DSP_Command_AEX(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_CSH(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_OSH(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_PEX(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_PON(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_POF(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_REX(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_SEX(CCD_Interface_Handle_T* handle,struct timespec start_time,int exposure_length);
extern int CCD_DSP_Command_Reset(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_Get_HSTR(CCD_Interface_Handle_T* handle,int *value);
extern int CCD_DSP_Command_Get_Readout_Progress(CCD_Interface_Handle_T* handle,int *value);
extern int CCD_DSP_Command_RCC(CCD_Interface_Handle_T* handle,int *value);
extern int CCD_DSP_Command_GWF(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_PCI_Download(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_PCI_Download_Wait(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_PCI_PC_Reset(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_SET(CCD_Interface_Handle_T* handle,int msecs);
extern int CCD_DSP_Command_RET(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_FWA(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_FWM(CCD_Interface_Handle_T* handle,int position);
extern int CCD_DSP_Command_FWR(CCD_Interface_Handle_T* handle);
extern int CCD_DSP_Command_Manual(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,int command,
				  int *argument_list,int argument_count,int *reply_value);
extern char *CCD_DSP_Command_Manual_To_String(int manual_command);
extern int CCD_DSP_Command_String_To_Manual(char *command_string);
extern char *CCD_DSP_Print_Board_ID(enum CCD_DSP_BOARD_ID board_id);
extern char *CCD_DSP_Print_Mem_Space(enum CCD_DSP_MEM_SPACE mem_space);
extern char *CCD_DSP_Print_DeInterlace(enum CCD_DSP_DEINTERLACE_TYPE deinterlace);

extern int CCD_DSP_Get_Abort(void);
extern int CCD_DSP_Set_Abort(int value);
extern int CCD_DSP_Get_Error_Number(void);
extern void CCD_DSP_Error(void);
extern void CCD_DSP_Error_String(char *error_string);
extern void CCD_DSP_Warning(void);

#endif
