/* ccd_setup.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_setup.c,v 1.6 2022-03-21 13:21:02 cjm Exp $
*/
/**
 * ccd_setup.c contains routines to perform the setting of the SDSU CCD Controller, prior to performing
 * exposures.
 * @author SDSU, Chris Mottram
 * @version $Revision: 1.6 $
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
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#ifndef _POSIX_TIMERS
#include <sys/time.h>
#endif
#include "log_udp.h"
#include "ccd_global.h"
#include "ccd_dsp.h"
#include "ccd_dsp_download.h"
#include "ccd_interface.h"
#include "ccd_interface_private.h"
#include "ccd_temperature.h"
#include "ccd_setup.h"
#include "ccd_setup_private.h"

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_setup.c,v 1.6 2022-03-21 13:21:02 cjm Exp $";

/* #defines */
/**
 * Used when performing a short hardware test (in <a href="#CCD_Setup_Hardware_Test">CCD_Setup_Hardware_Test</a>),
 * This is the number of times each board's data link is tested using the 
 * <a href="ccd_dsp.html#CCD_DSP_TDL">TDL</a> command.
 */
#define SETUP_SHORT_TEST_COUNT		(10)

/**
 * The maximum value that can be sent as a data value argument to the Test Data Link SDSU command.
 * According to the documentation, this can be any 24bit number.
 */
#define TDL_MAX_VALUE 			(16777216)	/* 2^24 */

/**
 * The bit on the timing board, X memory space, location 0, which is set when the 
 * timing board is idling between exposures (idle clocking the CCD).
 */
#define SETUP_TIMING_IDLMODE		(1<<2)
/**
 * The SDSU controller address, on the Timing board, Y memory space, 
 * where the number of columns (binned) to be read out  is stored.
 * See tim.asm: NSR.
 */
#define SETUP_ADDRESS_DIMENSION_COLS	(0x1)
/**
 * The SDSU controller address, on the Timing board, Y memory space, 
 * where the number of rows (binned) to be read out  is stored.
 * See tim.asm: NPR.
 */
#define SETUP_ADDRESS_DIMENSION_ROWS	(0x2)
/**
 * The SDSU controller address, on the Timing board, Y memory space, 
 * where the X (Serial) Binning factor is stored.
 * See tim.asm: NSBIN
 */
#define SETUP_ADDRESS_BIN_X		(0x5)
/**
 * The SDSU controller address, on the Timing board, Y memory space, 
 * where the Y (Parallel) Binning factor is stored.
 * See tim.asm: NPBIN
 */
#define SETUP_ADDRESS_BIN_Y		(0x6)
/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the high voltage (+36v) supply voltage are stored.
 */
#define SETUP_HIGH_VOLTAGE_ADDRESS	(0x8)
/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the low voltage (+15v) supply voltage are stored.
 */
#define SETUP_LOW_VOLTAGE_ADDRESS	(0x9)
/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the negative low voltage (-15v) supply voltage are stored.
 */
#define SETUP_MINUS_LOW_VOLTAGE_ADDRESS	(0xa)
/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the high voltage (+36v) supply voltage from Power ON are stored.
 */
#define SETUP_HIGH_VOLTAGE_POWER_ON_ADDRESS	 (0x25)
/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the low voltage (+15v) supply voltage from Power ON are stored.
 */
#define SETUP_LOW_VOLTAGE_POWER_ON_ADDRESS	 (0x26)
/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the negative low voltage (-15v) supply voltage from Power ON are stored.
 */
#define SETUP_MINUS_LOW_VOLTAGE_POWER_ON_ADDRESS (0x27)

/**
 * The SDSU controller address, on the Utility board, Y memory space, 
 * where the digitized ADU counts for the vacuum gauge (if present) are stored.
 */
#define SETUP_VACUUM_GAUGE_ADDRESS	(0xf)
/**
 * How many times to read the vacuum guage memory on the utility board, when
 * sampling the voltage returned by the vacuum gauge. 
 */
#define SETUP_VACUUM_GAUGE_SAMPLE_COUNT	(10)
/**
 * The width of bias strip to use when windowing.
 */
#define SETUP_WINDOW_BIAS_WIDTH		(53)

/* data types */

/* external variables */

/* local variables */
/**
 * Variable holding error code of last operation performed by ccd_setup.
 */
static int Setup_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Setup_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";

/* local function definitions */
static int Setup_Reset_Controller(CCD_Interface_Handle_T* handle);
static int Setup_PCI_Board(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE load_type,char *filename);
static int Setup_Timing_Board(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE load_type,
			      int load_application_number,char *filename);
static int Setup_Utility_Board(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE load_type,
			       int load_application_number,char *filename);
static int Setup_Power_On(CCD_Interface_Handle_T* handle);
static int Setup_Power_Off(CCD_Interface_Handle_T* handle);
static int Setup_Gain(CCD_Interface_Handle_T* handle,enum CCD_DSP_GAIN gain,int speed);
static int Setup_Idle(CCD_Interface_Handle_T* handle,int idle);
static int Setup_Window_List(CCD_Interface_Handle_T* handle,int window_flags,
			     struct CCD_Setup_Window_Struct window_list[]);
static int Setup_Controller_Windows(CCD_Interface_Handle_T* handle);

/* external functions */
/**
 * This routine sets up ccd_setup internal variables.
 * It should be called at startup.
 */
void CCD_Setup_Initialise(void)
{
	Setup_Error_Number = 0;
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Setup_Initialise:%s.\n",rcsid);
#ifdef CCD_SETUP_TIMING_DOWNLOAD_IDLE
	fprintf(stdout,"CCD_Setup_Initialise:Stop timing board Idling whilst downloading timing board DSP code.\n");
#else
	fprintf(stdout,"CCD_Setup_Initialise:NOT Stopping timing board Idling "
		"whilst downloading timing board DSP code.\n");
#endif
}

/**
 * Routine to initialise the setup data in the interface handle. 
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_setup_private.html#CCD_Setup_Struct
 */
void CCD_Setup_Data_Initialise(CCD_Interface_Handle_T* handle)
{
	int i;

	handle->Setup_Data.NCols = 0;
	handle->Setup_Data.NRows = 0;
	handle->Setup_Data.Binned_NCols = 0;
	handle->Setup_Data.Binned_NRows = 0;
	handle->Setup_Data.NSBin = 0;
	handle->Setup_Data.NPBin = 0;
	handle->Setup_Data.Gain = CCD_DSP_GAIN_ONE;
	handle->Setup_Data.Amplifier = CCD_DSP_AMPLIFIER_BOTTOM_LEFT;
	handle->Setup_Data.Is_Dummy = FALSE;
	handle->Setup_Data.Idle = FALSE;
	handle->Setup_Data.Window_Flags = 0;
	for(i=0;i<CCD_SETUP_WINDOW_COUNT;i++)
	{
		handle->Setup_Data.Window_List[i].X_Start = -1;
		handle->Setup_Data.Window_List[i].Y_Start = -1;
		handle->Setup_Data.Window_List[i].X_End = -1;
		handle->Setup_Data.Window_List[i].Y_End = -1;
	}
	handle->Setup_Data.Power_Complete = FALSE;
	handle->Setup_Data.PCI_Complete = FALSE;
	handle->Setup_Data.Timing_Complete = FALSE;
	handle->Setup_Data.Utility_Complete = FALSE;
	handle->Setup_Data.Dimension_Complete = FALSE;
}

/**
 * The routine that sets up the SDSU CCD Controller. This routine does the following:
 * <ul>
 * <li>Resets setup completion flags.</li>
 * <li>Loads a PCI board program from ROM/file.</li>
 * <li>Resets the SDSU CCD Controller.</li>
 * <li>Does a hardware test on the data links to each board in the controller. This is done
 * 	SETUP_SHORT_TEST_COUNT times.</li>
 * <li>Loads a timing board program from ROM/application/file.</li>
 * <li>Loads a utility board program from ROM/application/file.</li>
 * <li>Switches the boards analogue power on.</li>
 * <li>Setup the array's gain and readout speed.</li>
 * <li>Sets the arrays target temperature.</li>
 * <li>Setup the readout clocks to idle(or not!).</li>
 * </ul>
 * Array dimension information also needs to be setup before the controller can take exposures 
 * (see CCD_Setup_Dimensions).
 * This routine can be aborted with CCD_Setup_Abort.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param pci_load_type Where the routine is going to load the timing board application from. One of
 * 	<a href="#CCD_SETUP_LOAD_TYPE">CCD_SETUP_LOAD_TYPE</a>:
 * 	CCD_SETUP_LOAD_ROM or
 * 	CCD_SETUP_LOAD_FILENAME. The PCI DSP has no applications.
 * @param pci_filename The filename of the DSP code on disc that will be loaded if the
 * 	pci_load_type is CCD_SETUP_LOAD_FILENAME.
 * @param memory_map_length The length of memory to map for image readout, in bytes.
 * @param load_timing_software A boolean, TRUE  to load the timing board application DSP software.
 * @param timing_load_type Where the routine is going to load the timing board application from. One of
 * 	<a href="#CCD_SETUP_LOAD_TYPE">CCD_SETUP_LOAD_TYPE</a>:
 * 	CCD_SETUP_LOAD_ROM, CCD_SETUP_LOAD_APPLICATION or
 * 	CCD_SETUP_LOAD_FILENAME.
 * @param timing_application_number The application number of the DSP code on EEPROM that will be loaded if the 
 * 	timing_load_type is CCD_SETUP_LOAD_APPLICATION.
 * @param timing_filename The filename of the DSP code on disc that will be loaded if the
 * 	timing_load_type is CCD_SETUP_LOAD_FILENAME.
 * @param load_utility_software A boolean, TRUE  to load the utility board application DSP software.
 * @param utility_load_type Where the routine is going to load the utility board application from. One of
 * 	<a href="#CCD_SETUP_LOAD_TYPE">CCD_SETUP_LOAD_TYPE</a>:
 * 	CCD_SETUP_LOAD_APPLICATION or
 * 	CCD_SETUP_LOAD_FILENAME.
 * @param utility_application_number The application number of the DSP code on EEPROM that will be loaded if the 
 * 	utility_load_type is CCD_SETUP_LOAD_APPLICATION.
 * @param utility_filename The filename of the DSP code on disc that will be loaded if the
 * 	utility_load_type is CCD_SETUP_LOAD_FILENAME.
 * @param set_temperature A boolean, TRUE  to set the CCD target temperature.
 * @param target_temperature Specifies the target temperature the CCD is meant to run at. 
 * @param gain Specifies the gain to use for the CCD video processors. Acceptable values are
 * 	<a href="ccd_dsp.html#CCD_DSP_GAIN">CCD_DSP_GAIN</a>:
 * 	CCD_DSP_GAIN_ONE, CCD_DSP_GAIN_TWO,
 * 	CCD_DSP_GAIN_FOUR and CCD_DSP_GAIN_NINE.
 * @param gain_speed Set to true for fast integrator speed, false for slow integrator speed.
 * @param idle If true puts CCD clocks in readout sequence, but not transferring any data, whenever a
 * 	command is not executing.
 * @return Returns TRUE if the setup is successfully completed, FALSE if the setup fails or is aborted.
 * @see #Setup_Reset_Controller
 * @see #CCD_Setup_Hardware_Test
 * @see #Setup_PCI_Board
 * @see #Setup_Timing_Board
 * @see #Setup_Utility_Board
 * @see #Setup_Power_On
 * @see #CCD_Temperature_Set
 * @see #Setup_Gain
 * @see #Setup_Idle
 * @see #SETUP_SHORT_TEST_COUNT
 * @see #CCD_SETUP_MEMORY_BUFFER_SIZE
 * @see #CCD_Setup_Dimensions
 * @see #CCD_Setup_Abort
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_dsp.html#CCD_DSP_Command_Flush_Reply_Buffer
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Startup(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE pci_load_type,char *pci_filename,
		      long memory_map_length,int load_timing_software,
		      enum CCD_SETUP_LOAD_TYPE timing_load_type,int timing_application_number,char *timing_filename,
		      int load_utility_software,
		      enum CCD_SETUP_LOAD_TYPE utility_load_type,int utility_application_number,char *utility_filename,
		      int set_temeprature,double target_temperature,enum CCD_DSP_GAIN gain,int gain_speed,int idle)
{
	Setup_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Startup(handle=%p,pci_load_type=%d,"
			      "memory_map_length=%ld,"
			      "load_timing_software=%d,timing_load_type=%d,timing_application=%d,"
			      "load_utility_software=%d,utility_load_type=%d,utility_application=%d,"
			      "set_temeprature=%d,temperature=%.2f,gain=%d,gain_speed=%d,idle=%d) started.",
			      handle,pci_load_type,memory_map_length,
			      load_timing_software,timing_load_type,timing_application_number,
			      load_utility_software,utility_load_type,utility_application_number,
			      set_temeprature,target_temperature,gain,gain_speed,idle);
	if(pci_filename != NULL)
		CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Startup has pci_filename=%s.",pci_filename);
	if(timing_filename != NULL)
		CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Startup has timing_filename=%s.",
				      timing_filename);
	if(utility_filename != NULL)
		CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Startup has utility_filename=%s.",
				      utility_filename);
#endif
/* we are in a setup routine */
	handle->Setup_Data.Setup_In_Progress = TRUE;
/* reset abort flag - we havn't aborted yet! */
	CCD_DSP_Set_Abort(FALSE);
/* reset completion flags - even dimension flag is reset, as the controller itself is reset */
	handle->Setup_Data.Power_Complete = FALSE;
	handle->Setup_Data.PCI_Complete = FALSE;
	handle->Setup_Data.Timing_Complete = FALSE;
	handle->Setup_Data.Utility_Complete = FALSE;
	handle->Setup_Data.Dimension_Complete = FALSE;
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 67;
		sprintf(Setup_Error_String,"CCD_Setup_Started:Aborted");
		return FALSE;
	}
/* load a PCI interface board ROM or a filename */
	if(!Setup_PCI_Board(handle,pci_load_type,pci_filename))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return FALSE;
	}
	else   /*acknowlege PCI load complete*/
		handle->Setup_Data.PCI_Complete = TRUE;
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 68;
		sprintf(Setup_Error_String,"CCD_Setup_Started:Aborted");
		return FALSE;
	}
/* memory map initialisation */
/* done after PCI download, as astropci sends a WRITE_PCI_ADDRESS HCVR command to the PCI board
** in response to a mmap call. */
	if(!CCD_Interface_Memory_Map(handle,memory_map_length))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 41;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Memory Map failed.");
		return FALSE;

	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 69;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* reset controller */
	if(!Setup_Reset_Controller(handle))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return(FALSE);
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 70;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* do a hardware test (data link) */
	if(!CCD_Setup_Hardware_Test(handle,SETUP_SHORT_TEST_COUNT,load_timing_software,load_utility_software))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return FALSE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 71;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* load a timing board application from ROM or a filename */
	if(load_timing_software)
	{
		if(!Setup_Timing_Board(handle,timing_load_type,timing_application_number,timing_filename))
		{
			handle->Setup_Data.Setup_In_Progress = FALSE;
			return FALSE;
		}
		else   /*acknowlege timing load complete*/
			handle->Setup_Data.Timing_Complete = TRUE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 72;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* load a utility board application from ROM or a filename */
	if(load_utility_software)
	{
		if(!Setup_Utility_Board(handle,utility_load_type,utility_application_number,utility_filename))
		{
			handle->Setup_Data.Setup_In_Progress = FALSE;
			return FALSE;
		}
		else /*acknowlege utility load complete*/ 
			handle->Setup_Data.Utility_Complete = TRUE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 73;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* turn analogue power on */
	if(!Setup_Power_On(handle))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return FALSE;
	}
	else /*acknowlege power on complete*/ 
		handle->Setup_Data.Power_Complete = TRUE;
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 74;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* setup gain */
	if(!Setup_Gain(handle,gain,gain_speed))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return FALSE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 75;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* set the temperature */
	if(set_temeprature)
	{
		if(!CCD_Temperature_Set(handle,target_temperature))
		{
			handle->Setup_Data.Setup_In_Progress = FALSE;
			Setup_Error_Number = 2;
			sprintf(Setup_Error_String,"CCD_Setup_Startup:CCD_Temperature_Set failed(%.2f)",
				target_temperature);
			return FALSE;
		}
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 76;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* setup idling of the readout clocks */
	if(!Setup_Idle(handle,idle))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return FALSE;
	}
/* tidy up flags and return */
	handle->Setup_Data.Setup_In_Progress = FALSE;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Startup(handle=%p) returned TRUE.",handle);
#endif
	return TRUE;
}

/**
 * Routine to shut down the SDSU CCD Controller board. This consists of:
 * <ul>
 * <li>Reseting the setup completion flags.
 * <li>Performing a power off command to switch off analogue voltages.
 * </ul>
 * It then just remains to close the connection to the astro device driver.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @see #CCD_Setup_Startup
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Shutdown(CCD_Interface_Handle_T* handle)
{

	Setup_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Stutdown(handle=%p) started.",handle);
#endif
/* reset abort flag */
	CCD_DSP_Set_Abort(FALSE);
/* perform a power off */
	if(!Setup_Power_Off(handle))
	{
		return FALSE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 77;
		sprintf(Setup_Error_String,"CCD_Setup_Startup:Aborted");
		return FALSE;
	}
/* memory map un-mapped */
	if(!CCD_Interface_Memory_UnMap(handle))
	{
		Setup_Error_Number = 50;
		sprintf(Setup_Error_String,"CCD_Setup_Shutdown:Memory UnMap failed.");
		return FALSE;
	}
/* reset completion flags  */
	handle->Setup_Data.Power_Complete = FALSE;
	handle->Setup_Data.PCI_Complete = FALSE;
	handle->Setup_Data.Timing_Complete = FALSE;
	handle->Setup_Data.Utility_Complete = FALSE;
	handle->Setup_Data.Dimension_Complete = FALSE;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Stutdown(handle=%p) returned TRUE.",handle);
#endif
	return TRUE;
}

/**
 * Routine to setup dimension information in the controller. This needs to be setup before an exposure
 * can take place. This routine must be called <b>after</b> the CCD_Setup_Startup routine.
 * This routine can be aborted with CCD_Setup_Abort.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param ncols The number of unbinned columns in the image.
 * @param nrows The number of unbinned rows in the image to be readout from the CCD.
 * @param nsbin The amount of binning applied to pixels in columns. This parameter will change internally
 *	ncols.
 * @param npbin The amount of binning applied to pixels in rows.This parameter will change internally
 *	nrows.
 * @param amplifier Which amplifier to use when reading out data from the CCD. Possible values come from
 * 	the CCD_DSP_AMPLIFIER enum.
 * @param window_flags Information on which of the sets of window positions supplied contain windows to be used.
 * @param window_list A list of CCD_Setup_Window_Structs defining the position of the windows. The list should
 * 	<b>always</b> contain <b>four</b> entries, one for each possible window. The window_flags parameter
 * 	determines which items in the list are used.
 * @return The routine returns TRUE on success and FALSE if an error occured.
 * @see #CCD_Setup_Startup
 * @see #Setup_Window_List
 * @see #CCD_Setup_Abort
 * @see #CCD_Setup_Window_Struct
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_dsp.html#CCD_DSP_Command_WRM
 * @see ccd_dsp.html#CCD_DSP_Command_SOS
 * @see ccd_dsp.html#CCD_DSP_AMPLIFIER
 * @see ccd_dsp.html#CCD_DSP_IS_AMPLIFIER
 * @see ccd_dsp.html#CCD_DSP_IS_DUMMY_AMPLIFIER
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #SETUP_ADDRESS_BIN_X
 * @see #SETUP_ADDRESS_BIN_Y
 * @see #SETUP_ADDRESS_DIMENSION_COLS
 * @see #SETUP_ADDRESS_DIMENSION_ROWS
 */
int CCD_Setup_Dimensions(CCD_Interface_Handle_T* handle,int ncols,int nrows,int nsbin,int npbin,
	enum CCD_DSP_AMPLIFIER amplifier,int window_flags,struct CCD_Setup_Window_Struct window_list[])
{
	Setup_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Dimensions(handle=%p,"
			      "ncols(unbinned)=%d,nrows(unbinned)=%d,"
			      "nsbin=%d,npbin=%d,amplifier=%d(%s),window_flags=%d) started.",
			      handle,ncols,nrows,nsbin,npbin,amplifier,CCD_DSP_Command_Manual_To_String(amplifier),
			      window_flags);
#endif
/* we are in a setup routine */
	handle->Setup_Data.Setup_In_Progress = TRUE;
/* reset abort flag - we havn't aborted yet! */
	CCD_DSP_Set_Abort(FALSE);
/* reset dimension flag */
	handle->Setup_Data.Dimension_Complete = FALSE;
/* check and setup internal variables for the image dimensions/binning and amplifier. */
	if(nrows <= 0)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 24;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Illegal value:Number of Rows '%d'",nrows);
		return FALSE;
	}
	handle->Setup_Data.NRows = nrows;
	if(ncols <= 0)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 25;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Illegal value:Number of Columns '%d'",ncols);
		return FALSE;
	}
	handle->Setup_Data.NCols = ncols;
	if(nsbin <= 0)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 18;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Illegal value:Serial Binning '%d'",nsbin);
		return FALSE;
	}
	handle->Setup_Data.NSBin = nsbin;
	if(npbin <= 0)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 19;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Illegal value:Parallel Binning '%d'",nsbin);
		return FALSE;
	}
	handle->Setup_Data.NPBin = npbin;
/* amplifier */
	if(!CCD_DSP_IS_AMPLIFIER(amplifier))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 42;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Illegal value:Amplifier '%d'/'%#x'/'%s'",
			amplifier,amplifier,CCD_DSP_Command_Manual_To_String(amplifier));
		return FALSE;
	}
	handle->Setup_Data.Amplifier = amplifier;
	handle->Setup_Data.Is_Dummy = CCD_DSP_IS_DUMMY_AMPLIFIER(handle->Setup_Data.Amplifier);
	/* send binning values to the timing board */
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_ADDRESS_BIN_X,
			       handle->Setup_Data.NSBin) != CCD_DSP_DON)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 16;
		sprintf(Setup_Error_String,"Setting Column Binning failed(%d)",handle->Setup_Data.NSBin);
		return FALSE;
	}
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_ADDRESS_BIN_Y,
			       handle->Setup_Data.NPBin)!= CCD_DSP_DON)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 17;
		sprintf(Setup_Error_String,"Setting Row Binning failed(%d)",handle->Setup_Data.NPBin);
		return FALSE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 78;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Aborted");
		return FALSE;
	}
/* send output amplifier to the tiing board */
	if(CCD_DSP_Command_SOS(handle,handle->Setup_Data.Amplifier)!=CCD_DSP_DON)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 43;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Setting Amplifier to %#x failed",
			handle->Setup_Data.Amplifier);
		return FALSE;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 79;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Aborted");
		return FALSE;
	}
/* setup binned dimensions */
	handle->Setup_Data.Binned_NCols = handle->Setup_Data.NCols/handle->Setup_Data.NSBin;
	handle->Setup_Data.Binned_NRows = handle->Setup_Data.NRows/handle->Setup_Data.NPBin;
#if LOGGING > 7
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Setup_Dimensions:Binned NCols = %d, Binned NRows = %d.",
			      handle->Setup_Data.Binned_NCols,handle->Setup_Data.Binned_NRows);
#endif
	/* setup ncols/nrows to be sent to the SDSU boards.
	** This is normally the number of pixels we expect the readout to return.
	** This is normally the number of binned pixels, unless the amplifier setting includes dummy outputs,
	** in which case the number of columns is doubled as we expect twice as many pixels back. */
	/* If the amplifier has a dummy as well as real outputs, double the number of columns */
	if(handle->Setup_Data.Is_Dummy)
	{
		handle->Setup_Data.Final_NCols = handle->Setup_Data.Binned_NCols*2;
		handle->Setup_Data.Final_NRows = handle->Setup_Data.Binned_NRows;
	}
	else
	{
		handle->Setup_Data.Final_NCols = handle->Setup_Data.Binned_NCols;
		handle->Setup_Data.Final_NRows = handle->Setup_Data.Binned_NRows;
	}
#if LOGGING > 7
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
			      "CCD_Setup_Dimensions:Amplifier Is_Dummy = %d,Final NCols = %d, Final NRows = %d.",
			      handle->Setup_Data.Is_Dummy,
			      handle->Setup_Data.Final_NCols,handle->Setup_Data.Final_NRows);
#endif
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_ADDRESS_DIMENSION_COLS,
			       handle->Setup_Data.Final_NCols) != CCD_DSP_DON)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 22;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Column Setup failed(%d)",
			handle->Setup_Data.Final_NCols);
		return FALSE;
	}
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_ADDRESS_DIMENSION_ROWS,
			       handle->Setup_Data.Final_NRows) != CCD_DSP_DON)
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 23;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Row Setup failed(%d)",handle->Setup_Data.Final_NRows);
		return FALSE;
	}
	handle->Setup_Data.Dimension_Complete = TRUE;
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 80;
		sprintf(Setup_Error_String,"CCD_Setup_Dimensions:Aborted");
		return FALSE;
	}
/* setup windowing data */
	if(!Setup_Window_List(handle,window_flags,window_list))
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		return FALSE;
	}
/* reset in progress information */
	handle->Setup_Data.Setup_In_Progress = FALSE;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Dimensions(handle=%p) returned TRUE.",handle);
#endif
	return TRUE;
}

/**
 * Routine that performs a hardware test on the PCI, timing and utility boards. It does this by doing 
 * sending TDL commands to the boards and testing the results. This routine is called from
 * CCD_Setup_Startup.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param test_count The number of times to perform the TDL command <b>on each board</b>. The test is performed on
 * 	three boards, PCI, timing and utility.
 * @param test_timing_board Whether to do the timing board test.
 * @param test_utility_board Whether to do the utility board test.
 * @return If all the TDL commands fail to one of the boards it returns FALSE, otherwise
 *	it returns TRUE. If some commands fail a warning is given.
 * @see ccd_dsp.html#CCD_DSP_Command_TDL
 * @see #CCD_Setup_Startup
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Hardware_Test(CCD_Interface_Handle_T* handle,int test_count,int test_timing_board,int test_utility_board)
{
	int i;				/* loop number */
	int value;			/* value sent to tdl */
	int value_increment;		/* amount to increment value for each pass through the loop */
	int retval;			/* return value from dsp_command */
	int pci_errno,tim_errno,util_errno;	/* num of test encountered, per board */

	Setup_Error_Number = 0;
	CCD_DSP_Set_Abort(FALSE);
	value_increment = TDL_MAX_VALUE/test_count;

	/* test the PCI board test_count times */
	pci_errno = 0;
	value = 0;
	for(i=1; i<=test_count; i++)
	{
		retval = CCD_DSP_Command_TDL(handle,CCD_DSP_INTERFACE_BOARD_ID,value);
		if(retval != value)
			pci_errno++;
		value += value_increment;
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 81;
		sprintf(Setup_Error_String,"CCD_Setup_Hardware_Test:Aborted.");
		return FALSE;
	}
	/* test the timimg board test_count times */
	if(test_timing_board)
	{
		tim_errno = 0;
		value = 0;
		for(i=1; i<=test_count; i++)
		{
			retval = CCD_DSP_Command_TDL(handle,CCD_DSP_TIM_BOARD_ID,value);
			if(retval != value)
				tim_errno++;
			value += value_increment;
		}
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 82;
		sprintf(Setup_Error_String,"CCD_Setup_Hardware_Test:Aborted.");
		return FALSE;
	}
	/* test the utility board test_count times */
	if(test_utility_board)
	{
		util_errno = 0;
		value = 0;	
		for(i=1; i<=test_count; i++)
		{
			retval = CCD_DSP_Command_TDL(handle,CCD_DSP_UTIL_BOARD_ID,value);
			if(retval != value)
				util_errno++;
			value += value_increment;
		}
	}
/* if we have aborted - stop here */
	if(CCD_DSP_Get_Abort())
	{
		handle->Setup_Data.Setup_In_Progress = FALSE;
		Setup_Error_Number = 83;
		sprintf(Setup_Error_String,"CCD_Setup_Hardware_Test:Aborted.");
		return FALSE;
	}

	/* if some PCI errors occured, setup an error message and determine whether it was fatal or not */
	if(pci_errno > 0)
	{
		Setup_Error_Number = 36;
		sprintf(Setup_Error_String,"Interface Board Hardware Test:Failed %d of %d times",
			pci_errno,test_count);
		if(pci_errno < test_count)
			CCD_Setup_Warning();
		else
			return FALSE;
	}
	/* if some timing errors occured, setup an error message and determine whether it was fatal or not */
	if(tim_errno > 0)
	{
		Setup_Error_Number = 4;
		sprintf(Setup_Error_String,"Timing Board Hardware Test:Failed %d of %d times",
			tim_errno,test_count);
		if(tim_errno < test_count)
			CCD_Setup_Warning();
		else
			return FALSE;
	}
	/* if some utility errors occured, setup an error message and determine whether it was fatal or not */
	if(util_errno > 0)
	{
		Setup_Error_Number = 5;
		sprintf(Setup_Error_String,"Utility Board Hardware Test:Failed %d of %d times",
			util_errno,test_count);
		if(util_errno < test_count)
			CCD_Setup_Warning();
		else
			return FALSE;
	}
	return TRUE;
}

/**
 * Routine to abort a setup that is underway. This will cause CCD_Setup_Startup and CCD_Setup_Dimensions
 * to return FALSE as it will fail to complete the setup.
 * @see #CCD_Setup_Startup
 * @see #CCD_Setup_Dimensions
 */
void CCD_Setup_Abort(void)
{
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Abort() started.");
#endif
	CCD_DSP_Set_Abort(TRUE);
}

/**
 * Routine that returns the number of columns that the SDSU CCD Controller will readout. This takes
 * into account the binning, and also whether pixels from a dummy output will be returned alongside
 * the actual image pixels.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The number of columns.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Final_NCols(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Final_NCols;
}

/**
 * Routine that returns the number of binned rows that the SDSU CCD Controller will readout. This takes
 * into account the binning, and also whether pixels from a dummy output will be returned alongside
 * the actual image pixels.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The number of rows.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Final_NRows(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Final_NRows;
}

/**
 * Routine that returns the number of binned columns that the SDSU CCD Controller will readout. This is the
 * unbinned number passed into CCD_Setup_Dimensions divided by the binning factor nsbin. 
 * Some deinterlacing options require an even number of columns and may have modified the number further.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The number of columns.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Binned_NCols(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Binned_NCols;
}

/**
 * Routine that returns the number of binned rows that the SDSU CCD Controller will readout. This is the
 * unbinned number passed into CCD_Setup_Dimensions divided by the binning factor npbin. 
 * Some deinterlacing options require an even number of columns and may have modified the number further.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The number of rows.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Binned_NRows(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Binned_NRows;
}

/**
 * Routine that returns the column (serial) binning factor the last dimension setup has set the SDSU CCD Controller to.
 * This is the number passed into CCD_Setup_Dimensions.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The columns binning number.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_NSBin(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.NSBin;
}

/**
 * Routine that returns the row (parallel) binning factor the last dimension setup has set the SDSU CCD Controller to. 
 * This is the number passed into CCD_Setup_Dimensions.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The row binning number.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_NPBin(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.NPBin;
}

/**
 * Routine to return the number of pixels that will be read out from the CCD. 
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The number of pixels.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_SETUP_WINDOW_COUNT
 * @see #SETUP_WINDOW_BIAS_WIDTH
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Readout_Pixel_Count(CCD_Interface_Handle_T* handle)
{
	int pixel_count,i,bias_width,box_width,box_height;

	if(handle->Setup_Data.Window_Flags == 0)
	{
		/* use the final ncols and nrows. This takes account of binning, and dummy outputs. */
		pixel_count = handle->Setup_Data.Final_NCols*handle->Setup_Data.Final_NRows;
	}
	else
	{
		pixel_count = 0;
		for(i=0;i<CCD_SETUP_WINDOW_COUNT;i++)
		{
			/* Note, relies on CCD_SETUP_WINDOW_ONE == (1<<0),
			** CCD_SETUP_WINDOW_TWO	== (1<<1),
			** CCD_SETUP_WINDOW_THREE == (1<<2) and
			** CCD_SETUP_WINDOW_FOUR == (1<<3) */
			if(handle->Setup_Data.Window_Flags&(1<<i))
			{
				/* These next lines  must agree with Setup_Controller_Windows for this to work */
				bias_width = SETUP_WINDOW_BIAS_WIDTH;/* diddly - get this from parameters? */
				box_width = handle->Setup_Data.Window_List[i].X_End-
					handle->Setup_Data.Window_List[i].X_Start;
				box_height = handle->Setup_Data.Window_List[i].Y_End-
					handle->Setup_Data.Window_List[i].Y_Start;
				pixel_count += (box_width+bias_width)*box_height;
			}
		}/* end for */
	}
	return pixel_count;
}

/**
 * Routine to return the number of pixels in the specified window. 
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param window_index This is the index in the window list to return. The first window is at index zero
 * 	and the last at (CCD_SETUP_WINDOW_COUNT-1). This index must be within this range.
 * @return The number of pixels, or -1 if this window is not in use.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_SETUP_WINDOW_COUNT
 * @see #SETUP_WINDOW_BIAS_WIDTH
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Window_Pixel_Count(CCD_Interface_Handle_T* handle,int window_index)
{
	int pixel_count,bias_width,box_width,box_height;

	if((window_index < 0) || (window_index >= CCD_SETUP_WINDOW_COUNT))
	{
		Setup_Error_Number = 61;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Window_Pixel_Count:Window Index '%d' out of range:"
			"['%d' to '%d'] inclusive.",window_index,0,CCD_SETUP_WINDOW_COUNT-1);
		return -1;
	}
	/* Note, relies on CCD_SETUP_WINDOW_ONE == (1<<0),
	** CCD_SETUP_WINDOW_TWO	== (1<<1),
	** CCD_SETUP_WINDOW_THREE == (1<<2) and
	** CCD_SETUP_WINDOW_FOUR == (1<<3) */
	if(handle->Setup_Data.Window_Flags&(1<<window_index))
	{
		/* These next lines  must agree with Setup_Controller_Windows for this to work */
		bias_width = SETUP_WINDOW_BIAS_WIDTH;/* diddly - get this from parameters? */
		box_width = handle->Setup_Data.Window_List[window_index].X_End-
			handle->Setup_Data.Window_List[window_index].X_Start;
		box_height = handle->Setup_Data.Window_List[window_index].Y_End-
			handle->Setup_Data.Window_List[window_index].Y_Start;
		pixel_count = (box_width+bias_width)*box_height;
	}
	else
		pixel_count = -1;
	return pixel_count;
}

/**
 * Routine to return the width of the specified window. 
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param window_index This is the index in the window list to return. The first window is at index zero
 * 	and the last at (CCD_SETUP_WINDOW_COUNT-1). This index must be within this range.
 * @return The width of the window (including any added bias strips), or -1 if this window is not in use.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_SETUP_WINDOW_COUNT
 * @see #SETUP_WINDOW_BIAS_WIDTH
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Window_Width(CCD_Interface_Handle_T* handle,int window_index)
{
	int box_width,bias_width;

	if((window_index < 0) || (window_index >= CCD_SETUP_WINDOW_COUNT))
	{
		Setup_Error_Number = 62;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Window_Width:Window Index '%d' out of range:"
			"['%d' to '%d'] inclusive.",window_index,0,CCD_SETUP_WINDOW_COUNT-1);
		return -1;
	}
	/* Note, relies on CCD_SETUP_WINDOW_ONE == (1<<0),
	** CCD_SETUP_WINDOW_TWO	== (1<<1),
	** CCD_SETUP_WINDOW_THREE == (1<<2) and
	** CCD_SETUP_WINDOW_FOUR == (1<<3) */
	if(handle->Setup_Data.Window_Flags&(1<<window_index))
	{
		/* These next lines  must agree with Setup_Controller_Windows for this to work */
		bias_width = SETUP_WINDOW_BIAS_WIDTH;/* diddly - get this from parameters? */
		box_width = handle->Setup_Data.Window_List[window_index].X_End-
			handle->Setup_Data.Window_List[window_index].X_Start+bias_width;
	}
	else
		box_width = -1;
	return box_width;
}

/**
 * Routine to return the height of the specified window. 
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param window_index This is the index in the window list to return. The first window is at index zero
 * 	and the last at (CCD_SETUP_WINDOW_COUNT-1). This index must be within this range.
 * @return The height of the window, or -1 if this window is not in use.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_SETUP_WINDOW_COUNT
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Window_Height(CCD_Interface_Handle_T* handle,int window_index)
{
	int box_height;

	if((window_index < 0) || (window_index >= CCD_SETUP_WINDOW_COUNT))
	{
		Setup_Error_Number = 63;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Window_Height:Window Index '%d' out of range:"
			"['%d' to '%d'] inclusive.",window_index,0,CCD_SETUP_WINDOW_COUNT-1);
		return -1;
	}
	/* Note, relies on CCD_SETUP_WINDOW_ONE == (1<<0),
	** CCD_SETUP_WINDOW_TWO	== (1<<1),
	** CCD_SETUP_WINDOW_THREE == (1<<2) and
	** CCD_SETUP_WINDOW_FOUR == (1<<3) */
	if(handle->Setup_Data.Window_Flags&(1<<window_index))
	{
		/* These next lines  must agree with Setup_Controller_Windows for this to work */
		box_height = handle->Setup_Data.Window_List[window_index].Y_End-
			handle->Setup_Data.Window_List[window_index].Y_Start;
	}
	else
		box_height = -1;
	return box_height;
}

/**
 * Routine to return the current gain value used by the CCD Camera.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The current gain value, one of
 * 	<a href="ccd_dsp.html#CCD_DSP_GAIN">CCD_DSP_GAIN</a>:
 * 	CCD_DSP_GAIN_ONE, CCD_DSP_GAIN_TWO,
 * 	CCD_DSP_GAIN_FOUR and CCD_DSP_GAIN_NINE.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_dsp.html#CCD_DSP_GAIN
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
enum CCD_DSP_GAIN CCD_Setup_Get_Gain(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Gain;
}

/**
 * Routine to return the amplifier used by the CCD Camera.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The current amplifier, in the enum CCD_DSP_AMPLIFIER.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_dsp.html#CCD_DSP_AMPLIFIER
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
enum CCD_DSP_AMPLIFIER CCD_Setup_Get_Amplifier(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Amplifier;
}

/**
 * Routine that returns whether the controller is set to Idle or not.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return A boolean. This is TRUE if the controller is currently setup to idle clock the CCD, or FALSE if it
 * 	is not.
 * @see #CCD_Setup_Startup
 * @see #Setup_Idle
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Idle(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Idle;
}

/**
 * Routine that returns the window flags number of the last successful dimension setup.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The window flags.
 * @see #CCD_Setup_Dimensions
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Window_Flags(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Window_Flags;
}

/**
 * Routine to return one of the windows setup on the CCD chip. Use CCD_Setup_Get_Window_Flags to
 * determine whether the window is in use.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param window_index This is the index in the window list to return. The first window is at index zero
 * 	and the last at (CCD_SETUP_WINDOW_COUNT-1). This index must be within this range.
 * @param window An address of a structure to hold the window data. This is filled with the
 * 	requested window data.
 * @return This routine returns TRUE if the window_index is in range and the window data is filled in,
 * 	FALSE if the window_index is out of range (an error is setup).
 * @see #CCD_SETUP_WINDOW_COUNT
 * @see #CCD_Setup_Window_Struct
 * @see #CCD_Setup_Get_Window_Flags
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Window(CCD_Interface_Handle_T* handle,int window_index,struct CCD_Setup_Window_Struct *window)
{
	if((window_index < 0) || (window_index >= CCD_SETUP_WINDOW_COUNT))
	{
		Setup_Error_Number = 1;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Window:Window Index '%d' out of range:"
			"['%d' to '%d'] inclusive.",window_index,0,CCD_SETUP_WINDOW_COUNT-1);
		return FALSE;
	}
	if(window == NULL)
	{
		Setup_Error_Number = 49;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Window:Window Index '%d':Null pointer.",window_index);
		return FALSE;
	}
	(*window) = handle->Setup_Data.Window_List[window_index];
	return TRUE;
}

/**
 * Routine to return whether CCD_Setup_Startup and CCD_Setup_Dimensions completed successfully,
 * and the controller is in a state suitable to do an exposure. This is determined by examining the
 * Completion flags in Setup_Data.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return Returns TRUE if setup was completed, FALSE otherwise.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_Setup_Startup
 * @see #CCD_Setup_Dimensions
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Setup_Complete(CCD_Interface_Handle_T* handle)
{
	return (handle->Setup_Data.Power_Complete&&handle->Setup_Data.PCI_Complete&&
		handle->Setup_Data.Timing_Complete&&handle->Setup_Data.Utility_Complete&&
		handle->Setup_Data.Dimension_Complete);
}

/**
 * Routine to return whether a call to CCD_Setup_Startup or CCD_Setup_Dimensions is in progress. This is done
 * by examining Setup_In_Progress in Setup_Data.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return Returns TRUE if the SDSU CCD Controller is in the process of being setup, FALSE otherwise.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_Setup_Startup
 * @see #CCD_Setup_Dimensions
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Setup_In_Progress(CCD_Interface_Handle_T* handle)
{
	return handle->Setup_Data.Setup_In_Progress;
}

/**
 * Routine to get the Analogue to Digital digitized value of the High Voltage (+36v) supply voltage.
 * This is read from the SETUP_HIGH_VOLTAGE_ADDRESS memory location, in Y memory space on the utility board.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param hv_adu The address of an integer to store the adus.
 * return Returns TRUE if the adus were read, FALSE otherwise.
 * @see #SETUP_HIGH_VOLTAGE_ADDRESS
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_dsp.html#CCD_DSP_BOARD_ID
 * @see ccd_dsp.html#CCD_DSP_MEM_SPACE
 * @see ccd_dsp.html#CCD_DSP_Get_Error_Number
 * @see ccd_global.html#CCD_Global_Log
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_High_Voltage_Analogue_ADU(CCD_Interface_Handle_T* handle,int *hv_adu)
{
	int retval;

	Setup_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Get_High_Voltage_Analogue_ADU(handle=%p) started.",
			      handle);
#endif
	if(hv_adu == NULL)
	{
		Setup_Error_Number = 51;
		sprintf(Setup_Error_String,"CCD_Setup_Get_High_Voltage_Analogue_ADU:adu was NULL.");
		return FALSE;
	}
	retval = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_HIGH_VOLTAGE_ADDRESS);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		Setup_Error_Number = 52;
		sprintf(Setup_Error_String,"CCD_Setup_Get_High_Voltage_Analogue_ADU:Read memory failed.");
		return FALSE;
	}
	(*hv_adu) = retval;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,
			      "CCD_Setup_Get_High_Voltage_Analogue_ADU(handle=%p) returned %#x.",
			      handle,(*hv_adu));
#endif
	return TRUE;
}

/**
 * Routine to get the Analogue to Digital digitized value of the Low Voltage (+15v) supply voltage.
 * This is read from the SETUP_LOW_VOLTAGE_ADDRESS memory location, in Y memory space on the utility board.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param lv_adu The address of an integer to store the adus.
 * return Returns TRUE if the adus were read, FALSE otherwise.
 * @see #SETUP_LOW_VOLTAGE_ADDRESS
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_dsp.html#CCD_DSP_BOARD_ID
 * @see ccd_dsp.html#CCD_DSP_MEM_SPACE
 * @see ccd_dsp.html#CCD_DSP_Get_Error_Number
 * @see ccd_global.html#CCD_Global_Log
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Low_Voltage_Analogue_ADU(CCD_Interface_Handle_T* handle,int *lv_adu)
{
	int retval;

	Setup_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Setup_Get_Low_Voltage_Analogue_ADU(handle=%p) started.",
			      handle);
#endif
	if(lv_adu == NULL)
	{
		Setup_Error_Number = 53;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Low_Voltage_Analogue_ADU:adu was NULL.");
		return FALSE;
	}
	retval = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_LOW_VOLTAGE_ADDRESS);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		Setup_Error_Number = 54;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Low_Voltage_Analogue_ADU:Read memory failed.");
		return FALSE;
	}
	(*lv_adu) = retval;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,
			      "CCD_Setup_Get_Low_Voltage_Analogue_ADU(handle=%p) returned %#x.",
			      handle,(*lv_adu));
#endif
	return TRUE;
}

/**
 * Routine to get the Analogue to Digital digitized value of the Low Voltage Negative (-15v) supply voltage.
 * This is read from the SETUP_MINUS_LOW_VOLTAGE_ADDRESS memory location, in Y memory space on the utility board.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param minus_lv_adu The address of an integer to store the adus.
 * return Returns TRUE if the adus were read, FALSE otherwise.
 * @see #SETUP_MINUS_LOW_VOLTAGE_ADDRESS
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_dsp.html#CCD_DSP_BOARD_ID
 * @see ccd_dsp.html#CCD_DSP_MEM_SPACE
 * @see ccd_dsp.html#CCD_DSP_Get_Error_Number
 * @see ccd_global.html#CCD_Global_Log
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU(CCD_Interface_Handle_T* handle,int *minus_lv_adu)
{
	int retval;

	Setup_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,
			      "CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU(handle=%p) started.",handle);
#endif
	if(minus_lv_adu == NULL)
	{
		Setup_Error_Number = 55;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU:adu was NULL.");
		return FALSE;
	}
	retval = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_MINUS_LOW_VOLTAGE_ADDRESS);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		Setup_Error_Number = 56;
		sprintf(Setup_Error_String,"CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU:Read memory failed.");
		return FALSE;
	}
	(*minus_lv_adu) = retval;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,
			      "CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU(handle=%p) returned %#x.",
			      handle,(*minus_lv_adu));
#endif
	return TRUE;
}

/**
 * Get the current value of ccd_setup's error number.
 * @return The current value of ccd_setup's error number.
 */
int CCD_Setup_Get_Error_Number(void)
{
	return Setup_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_setup in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Setup_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Setup_Error_Number == 0)
		sprintf(Setup_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Setup:Error(%d) : %s\n",time_string,Setup_Error_Number,Setup_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_setup in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Setup_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Setup_Error_Number == 0)
		sprintf(Setup_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Setup:Error(%d) : %s\n",time_string,
		Setup_Error_Number,Setup_Error_String);
}

/**
 * The warning routine that reports any warnings occuring in ccd_setup in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Setup_Warning(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an warning message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an warning to display */
	if(Setup_Error_Number == 0)
		sprintf(Setup_Error_String,"Logic Error:No Warning defined");
	fprintf(stderr,"%s CCD_Setup:Warning(%d) : %s\n",time_string,Setup_Error_Number,Setup_Error_String);
}

/* ------------------------------------------------------------------
**	Internal Functions
** ------------------------------------------------------------------ */
/**
 * Routine to reset the SDSU controller. A CCD_DSP_Command_Reset command is issued, which returns
 * <a href="ccd_dsp.html#CCD_DSP_SYR">SYR</a> on success. This is non-standard.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return Returns TRUE if reset timing was successfully completed, 
 * 	FALSE if it failed in some way.
 * @see ccd_dsp.html#CCD_DSP_Command_Reset
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Reset_Controller(CCD_Interface_Handle_T* handle)
{
	if(CCD_DSP_Command_Reset(handle) != CCD_DSP_SYR)
	{
		Setup_Error_Number = 33;
		sprintf(Setup_Error_String,"Reset Controller Failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * Internal routine that loads a PCI board DSP program into the SDSU CCD Controller. The
 * program can come from either ROM or a file on disc. This routine is called from
 * CCD_Setup_Startup.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param load_type Where to load the application from, one of<a href="#CCD_SETUP_LOAD_TYPE">CCD_SETUP_LOAD_TYPE</a>:
 *	CCD_SETUP_LOAD_ROM (do nothing, use default program loaded at startup), 
 *	CCD_SETUP_LOAD_APPLICATION (load from (EEP)ROM) or
 *	CCD_SETUP_LOAD_FILENAME (load from a disc file).
 * @param filename If load_type is CCD_SETUP_LOAD_FILENAME,
 * 	this specifies a filename to load the application from.
 * @return The routine returns TRUE if the operation succeeded, or FALSE if it fails.
 * @see #CCD_Setup_Startup
 * @see ccd_dsp_download.html#CCD_DSP_Download
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_PCI_Board(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE load_type,char *filename)
{
	if(!CCD_SETUP_IS_LOAD_TYPE(load_type))
	{
		Setup_Error_Number = 35;
		sprintf(Setup_Error_String,"Setup_PCI_Board:PCI board has illegal load type(%d).",
			load_type);
		return FALSE;
	}
	if(load_type == CCD_SETUP_LOAD_APPLICATION)
	{
		Setup_Error_Number = 39;
		sprintf(Setup_Error_String,"Setup_PCI_Board:PCI board cannot use Application type.");
		return FALSE;
	}
	else if(load_type == CCD_SETUP_LOAD_FILENAME)
	{
		if(filename == NULL)
		{
			Setup_Error_Number = 37;
			sprintf(Setup_Error_String,"PCI Board:DSP Filename was NULL.");
			return FALSE;
		}
		if(!CCD_DSP_Download(handle,CCD_DSP_INTERFACE_BOARD_ID,filename))
		{
			Setup_Error_Number = 40;
			sprintf(Setup_Error_String,"PCI Board:Failed to download filename '%s'.",
				filename);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Internal routine that loads a timing board DSP application onto the SDSU CCD Controller. The
 * application can come from either (EEP)ROM or a file on disc. This routine is called from
 * CCD_Setup_Startup. 
 * If the CCD_SETUP_TIMING_DOWNLOAD_IDLE compile time directive has been set,
 * and if the timing board is in idle mode and load_type is
 * CCD_SETUP_LOAD_FILENAME, the timing board is stopped whilst the application is loaded.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param load_type Where to load the application from, one of<a href="#CCD_SETUP_LOAD_TYPE">CCD_SETUP_LOAD_TYPE</a>:
 *	CCD_SETUP_LOAD_ROM (do nothing, use default program loaded at startup), 
 *	CCD_SETUP_LOAD_APPLICATION (load from (EEP)ROM) or
 *	CCD_SETUP_LOAD_FILENAME (load from a disc file).
 * @param load_application_number If load_type is CCD_SETUP_LOAD_APPLICATION,
 * 	this specifies which application number to load.
 * @param filename If load_type is CCD_SETUP_LOAD_FILENAME,
 * 	this specifies a filename to load the application from.
 * @return The routine returns TRUE if the operation succeeded, or FALSE if it fails.
 * @see ccd_dsp.html#CCD_DSP_Command_LDA
 * @see ccd_dsp.html#CCD_DSP_Command_IDL
 * @see ccd_dsp.html#CCD_DSP_Command_STP
 * @see ccd_dsp_download.html#CCD_DSP_Download
 * @see #CCD_Setup_Startup
 * @see #SETUP_TIMING_IDLMODE
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Timing_Board(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE load_type,
			      int load_application_number,char *filename)
{
#ifdef CCD_SETUP_TIMING_DOWNLOAD_IDLE
	int value,bit_value;
#endif

	if(!CCD_SETUP_IS_LOAD_TYPE(load_type))
	{
		Setup_Error_Number = 29;
		sprintf(Setup_Error_String,"Setup_Timing_Board:Timing board has illegal load type(%d).",
			load_type);
		return FALSE;
	}
/* If the compile time directive has been set, check whether the timing board is in idle mode,
** and if so send a STP command. 
** We do this whether it is an application or a filename load, as voodoo does. */
#ifdef CCD_SETUP_TIMING_DOWNLOAD_IDLE
/* if we are currently in IDL mode - come out of this mode */
	value = CCD_DSP_Command_RDM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_X,0);
	bit_value = value & SETUP_TIMING_IDLMODE;
	if(bit_value > 0)
	{
		if(CCD_DSP_Command_STP(handle)!= CCD_DSP_DON)
		{
			Setup_Error_Number = 7;
			sprintf(Setup_Error_String,"Timing Board:Failed to load filename '%s':STP failed.",
				filename);
			return FALSE;
		}
	}
#endif
	if(load_type == CCD_SETUP_LOAD_APPLICATION)
	{
		if(CCD_DSP_Command_LDA(handle,CCD_DSP_TIM_BOARD_ID,load_application_number)!=CCD_DSP_DON)
		{
			Setup_Error_Number = 6;
			sprintf(Setup_Error_String,"Timing Board:Failed to load application %d.",
				load_application_number);
			return FALSE;
		}
	}
	else if(load_type == CCD_SETUP_LOAD_FILENAME)
	{
		if(filename == NULL)
		{
			Setup_Error_Number = 38;
			sprintf(Setup_Error_String,"Timing Board:DSP Filename was NULL.");
			return FALSE;
		}
		/* download the program from the file */
		if(!CCD_DSP_Download(handle,CCD_DSP_TIM_BOARD_ID,filename))
		{
			Setup_Error_Number = 8;
			sprintf(Setup_Error_String,"Timing Board:Failed to download filename '%s'.",
				filename);
			return FALSE;
		}
	}
#ifdef CCD_SETUP_TIMING_DOWNLOAD_IDLE
/* if neccessary restart IDL mode. Note voodoo does not do this! */
	if(bit_value > 0)
	{
		if(CCD_DSP_Command_IDL(handle)!=CCD_DSP_DON)
		{
			Setup_Error_Number = 9;
			sprintf(Setup_Error_String,"Timing Board:Failed to load filename '%s':IDL failed.",
				filename);
			return FALSE;
		}
	}
#endif
	return TRUE;
}

/**
 * Internal routine that loads a utility board DSP application onto the SDSU CCD Controller. The
 * application can come from either (EEP)ROM or a file on disc. This routine is called from
 * CCD_Setup_Startup.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param load_type Where to load the application from, one of<a href="#CCD_SETUP_LOAD_TYPE">CCD_SETUP_LOAD_TYPE</a>:
 *	CCD_SETUP_LOAD_ROM (do nothing, use default program loaded at startup), 
 *	CCD_SETUP_LOAD_APPLICATION (load from (EEP)ROM) or
 *	CCD_SETUP_LOAD_FILENAME (load from a disc file).
 * @param load_application_number If load_type is CCD_SETUP_LOAD_APPLICATION,
 * 	this specifies which application number to load.
 * @param filename If load_type is CCD_SETUP_LOAD_FILENAME,
 * 	this specifies a filename to load the application from.
 * @return The routine returns TRUE if the operation succeeded, or FALSE if it fails.
 * @see ccd_dsp.html#CCD_DSP_Command_LDA
 * @see ccd_dsp_download.html#CCD_DSP_Download
 * @see #CCD_Setup_Startup
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Utility_Board(CCD_Interface_Handle_T* handle,enum CCD_SETUP_LOAD_TYPE load_type,
			       int load_application_number,char *filename)
{
	if(!CCD_SETUP_IS_LOAD_TYPE(load_type))
	{
		Setup_Error_Number = 30;
		sprintf(Setup_Error_String,"Setup_Utility_Board:Utility board has illegal load type(%d).",
			load_type);
		return FALSE;
	}
	if(load_type == CCD_SETUP_LOAD_APPLICATION)
	{
		if(CCD_DSP_Command_LDA(handle,CCD_DSP_UTIL_BOARD_ID,load_application_number)!=CCD_DSP_DON)
		{
			Setup_Error_Number = 10;
			sprintf(Setup_Error_String,"Utility Board:Failed to load application %d.",
				load_application_number);
			return FALSE;
		}
	}
	else if(load_type == CCD_SETUP_LOAD_FILENAME)
	{
		if(filename == NULL)
		{
			Setup_Error_Number = 44;
			sprintf(Setup_Error_String,"Utility Board:DSP Filename was NULL.");
			return FALSE;
		}
		if(!CCD_DSP_Download(handle,CCD_DSP_UTIL_BOARD_ID,filename))
		{
			Setup_Error_Number = 11;
			sprintf(Setup_Error_String,"Utility Board:Failed to download filename '%s'.",
				filename);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Internal ccd_setup routine that turns on the analog power to the SDSU CCD Controller using the DSP
 * PON command. The command is sent to the utility board using the
 * <a href="ccd_dsp.html#CCD_DSP_Command_PON">CCD_DSP_Command_PON</a> routine.This routine is called from
 * CCD_Setup_Startup.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return Returns TRUE if the operation succeeded, FALSE if it failed.
 * @see #CCD_Setup_Startup
 * @see ccd_dsp.html#CCD_DSP_Command_PON
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Power_On(CCD_Interface_Handle_T* handle)
{
	if(CCD_DSP_Command_PON(handle)!=CCD_DSP_DON)
	{
		Setup_Error_Number = 12;
		sprintf(Setup_Error_String,"Power On failed");
		return FALSE;
	}
	return TRUE;
}

/**
 * Internal ccd_setup routine that turns off the analog power to the SDSU CCD Controller using the DSP
 * POF command. The command is sent to the utility board using the
 * <a href="ccd_dsp.html#CCD_DSP_Command_POF">CCD_DSP_Command_POF</a> routine.This routine is called from
 * CCD_Setup_Shutdown.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return Returns TRUE if the operation succeeded, FALSE if it failed.
 * @see #CCD_Setup_Shutdown
 * @see ccd_dsp.html#CCD_DSP_Command_POF
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Power_Off(CCD_Interface_Handle_T* handle)
{
	if(CCD_DSP_Command_POF(handle)!=CCD_DSP_DON)
	{
		Setup_Error_Number = 3;
		sprintf(Setup_Error_String,"Power Off failed");
		return FALSE;
	}
	return TRUE;
}

/**
 * Internal routine to setup the gain and speed of the SDSU CCD Controller. This routine is called from
 * CCD_Setup_Startup. The gain used is the one setup in Setup_Data.Gain (setup in CCD_Setup_Startup).
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param gain Specifies the gain to use for the CCD video processors. Acceptable values are
 * 	<a href="ccd_dsp.html#CCD_DSP_GAIN">CCD_DSP_GAIN</a>:
 * 	CCD_DSP_GAIN_ONE, CCD_DSP_GAIN_TWO,
 * 	CCD_DSP_GAIN_FOUR and CCD_DSP_GAIN_NINE.
 * @param speed The speed to set the video integrators to. If TRUE they are 'fast', if FALSE they are 'slow'.
 * @return returns TRUE if the operation succeeded, FALSE if it failed.
 * @see #CCD_Setup_Startup
 * @see ccd_dsp.html#CCD_DSP_Command_SGN
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Gain(CCD_Interface_Handle_T* handle,enum CCD_DSP_GAIN gain,int speed)
{
	int ret_val;

	if(!CCD_DSP_IS_GAIN(gain))
	{
		Setup_Error_Number = 34;
		sprintf(Setup_Error_String,"Setup_Gain:Gain '%d' has illegal value.",gain);
		return FALSE;
	}
	handle->Setup_Data.Gain = gain;
	if(!CCD_GLOBAL_IS_BOOLEAN(speed))
	{
		Setup_Error_Number = 31;
		sprintf(Setup_Error_String,"Setup_Gain:Gain Speed %d  (gain = %d)  has illegal value.",speed,gain);
		return FALSE;
	}
	ret_val = CCD_DSP_Command_SGN(handle,handle->Setup_Data.Gain,speed);
	if(ret_val!=CCD_DSP_DON)
	{
		Setup_Error_Number = 13;
		sprintf(Setup_Error_String,"Setup_Gain:Setting Gain to %d(speed:%d) failed",gain,speed);
	}
	return (ret_val==CCD_DSP_DON);
}

/**
 * Internal routine to set whether the SDSU CCD Controller timing board should
 * idle when not executing commands, using the IDL or STP DSP commands. 
 * This routine is called from CCD_Setup_Startup. The Setup_Data's Idle property is changed to
 * reflect the current status of idling on the controller.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param idle TRUE if the timing board is to idle when not executing commands, FALSE if it isn't to idle.
 * @return returns TRUE if the operation succeeded, FALSE if it failed.
 * @see #CCD_Setup_Startup
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see ccd_dsp.html#CCD_DSP_Command_IDL
 * @see ccd_dsp.html#CCD_DSP_Command_STP
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Idle(CCD_Interface_Handle_T* handle,int idle)
{
	if(!CCD_GLOBAL_IS_BOOLEAN(idle))
	{
		Setup_Error_Number = 32;
		sprintf(Setup_Error_String,"Setting Idle failed:Illegal idle value '%d'",idle);
		return FALSE;
	}
	if(idle)
	{
		if(CCD_DSP_Command_IDL(handle)!=CCD_DSP_DON)
		{
			Setup_Error_Number = 14;
			sprintf(Setup_Error_String,"Setting Idle failed");
			return FALSE;
		}
		handle->Setup_Data.Idle = TRUE;
	}
	else
	{
		if(CCD_DSP_Command_STP(handle)!=CCD_DSP_DON)
		{
			Setup_Error_Number = 15;
			sprintf(Setup_Error_String,"Setting Stop Idle failed");
			return FALSE;
		}
		handle->Setup_Data.Idle = FALSE;
	}
	return TRUE;
}

/**
 * This routine sets the Setup_Data.Window_List from the passed in list of windows.
 * The windows are checked to ensure they don't overlap in the y (row) direction, and that sub-images are
 * all the same size. Only windows which are included in the window_flags parameter are checked.
 * If the windows are OK, Setup_Controller_Windows is called to write the windows to the SDSU controller.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param window_flags Information on which of the sets of window positions supplied contain windows to be used.
 * @param window_list A list of CCD_Setup_Window_Structs defining the position of the windows. The list should
 * 	<b>always</b> contain <b>four</b> entries, one for each possible window. The window_flags parameter
 * 	determines which items in the list are used.
 * @return The routine returns TRUE on success and FALSE if an error occured.
 * @see ccd_setup_private.html#CCD_Setup_Struct
 * @see #CCD_Setup_Window_Struct
 * @see #Setup_Controller_Windows
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int Setup_Window_List(CCD_Interface_Handle_T* handle,int window_flags,
			     struct CCD_Setup_Window_Struct window_list[])
{
	int start_window_index,end_window_index,found;
	int start_x_size,start_y_size,end_x_size,end_y_size;

/* check non-overlapping in y (row) directions all sub-images are same size. */
	start_window_index = 0;
	end_window_index = 0;
	/* while there are active windows, keep checking */
	while((start_window_index < CCD_SETUP_WINDOW_COUNT)&&(end_window_index < CCD_SETUP_WINDOW_COUNT))
	{
	/* find a valid start window index */
		found = FALSE;
		while((found == FALSE)&&(start_window_index < CCD_SETUP_WINDOW_COUNT))
		{
			found = (window_flags&(1<<start_window_index));
			if(!found)
				start_window_index++;
		}
	/* find a valid end window index  after start window index */
		end_window_index = start_window_index+1;
		found = FALSE;
		while((found == FALSE)&&(end_window_index < CCD_SETUP_WINDOW_COUNT))
		{
			found = (window_flags&(1<<end_window_index));
			if(!found)
				end_window_index++;
		}
	/* if we found two valid windows, check the second does not overlap the first */
		if((start_window_index < CCD_SETUP_WINDOW_COUNT)&&(end_window_index < CCD_SETUP_WINDOW_COUNT))
		{
		/* is start window's Y_End greater or equal to end windows Y_Start? */
			if(window_list[start_window_index].Y_End >= window_list[end_window_index].Y_Start)
			{
				Setup_Error_Number = 46;
				sprintf(Setup_Error_String,"Setting Windows:Windows %d and %d overlap in Y (%d,%d)",
					start_window_index,end_window_index,window_list[start_window_index].Y_End,
					window_list[end_window_index].Y_Start);
				return FALSE;
			}
		/* check sub-images are the same size/are positive size */
			start_x_size = window_list[start_window_index].X_End-window_list[start_window_index].X_Start;
			start_y_size = window_list[start_window_index].Y_End-window_list[start_window_index].Y_Start;
			end_x_size = window_list[end_window_index].X_End-window_list[end_window_index].X_Start;
			end_y_size = window_list[end_window_index].Y_End-window_list[end_window_index].Y_Start;
			if((start_x_size != end_x_size)||(start_y_size != end_y_size))
			{
				Setup_Error_Number = 47;
				sprintf(Setup_Error_String,"Setting Windows:Windows are different sizes"
					"%d = (%d,%d),%d = (%d,%d).",
					start_window_index,start_x_size,start_y_size,
					end_window_index,end_x_size,end_y_size);
				return FALSE;
			}
		/* note both windows are same size, only need to check one for sensible size */
			if((start_x_size <= 0)||(start_y_size <= 0))
			{
				Setup_Error_Number = 48;
				sprintf(Setup_Error_String,"Setting Windows:Windows are too small(%d,%d).",
					start_x_size,start_y_size);
				return FALSE;
			}
		}
	/* check the next pair of windows, by setting the start point to the last end point in the list */
		start_window_index = end_window_index;
	}
/* copy parameters to Setup_Data.Window_List and Setup_Data.Window_Flags */
	if(window_flags&CCD_SETUP_WINDOW_ONE)
		handle->Setup_Data.Window_List[0] = window_list[0];
	if(window_flags&CCD_SETUP_WINDOW_TWO)
		handle->Setup_Data.Window_List[1] = window_list[1];
	if(window_flags&CCD_SETUP_WINDOW_THREE)
		handle->Setup_Data.Window_List[2] = window_list[2];
	if(window_flags&CCD_SETUP_WINDOW_FOUR)
		handle->Setup_Data.Window_List[3] = window_list[3];
	handle->Setup_Data.Window_Flags = window_flags;
/* write parameters to window table on timing board */
	if(!Setup_Controller_Windows(handle))
		return FALSE;
	return TRUE;
}

/**
 * Actually write the calculated Setup_Data windows to the SDSU controller, using SSS and SSP.
 * If no windowing is taking place, we use SSS to reset the window sizes to zero (turning them off in the DSP code).
 * We also call Setup_Dimensions to set NSR and NPR to an area equivalent to the total number of pixels
 * written back from the timing board to the PCI board.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE on success, and FALSE if something fails.
 * @see #CCD_Setup_Window_Struct
 * @see #SETUP_ADDRESS_DIMENSION_COLS
 * @see #SETUP_ADDRESS_DIMENSION_ROWS
 * @see #SETUP_WINDOW_BIAS_WIDTH
 * @see ccd_dsp.html#CCD_DSP_Command_WRM
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_setup_private.html#CCD_Setup_Struct
 */
static int Setup_Controller_Windows(CCD_Interface_Handle_T* handle)
{
	struct CCD_Setup_Window_Struct window_list[CCD_SETUP_WINDOW_COUNT];
	int bias_width,box_width,box_height,window_count;
	int y_offset,x_offset,bias_x_offset,i,total_ncols;

	/* if no windows - reset window sizes and return */
	if(handle->Setup_Data.Window_Flags == 0)
	{
		if(CCD_DSP_Command_SSS(handle,0,0,0) != CCD_DSP_DON)
		{
			Setup_Error_Number = 64;
			sprintf(Setup_Error_String,"Reseting Subarray Sizes to zero failed.");
			return FALSE;
		}
		return TRUE;
	}
	/* setup window_list/count, for each window defined in real list */
	window_count = 0;
	if(handle->Setup_Data.Window_Flags&CCD_SETUP_WINDOW_ONE)
		window_list[window_count++] = handle->Setup_Data.Window_List[0];
	if(handle->Setup_Data.Window_Flags&CCD_SETUP_WINDOW_TWO)
		window_list[window_count++] = handle->Setup_Data.Window_List[1];
	if(handle->Setup_Data.Window_Flags&CCD_SETUP_WINDOW_THREE)
		window_list[window_count++] = handle->Setup_Data.Window_List[2];
	if(handle->Setup_Data.Window_Flags&CCD_SETUP_WINDOW_FOUR)
		window_list[window_count++] = handle->Setup_Data.Window_List[3];
	/* setup SSS parameters - note we know from Setup_Window_List that all boxes have the same size */
	bias_width = SETUP_WINDOW_BIAS_WIDTH;/* diddly - get this from parameters? */
	box_width = window_list[0].X_End-window_list[0].X_Start;
	box_height = window_list[0].Y_End-window_list[0].Y_Start;
	if(CCD_DSP_Command_SSS(handle,bias_width,box_width,box_height) != CCD_DSP_DON)
	{
		Setup_Error_Number = 45;
		sprintf(Setup_Error_String,"Setting Subarray Sizes failed:(%d,%d,%d).",bias_width,box_width,
			box_height);
		return FALSE;
	}
	total_ncols = 0;
	/* send SSP for each window */
	for(i=0; i < window_count;i++)
	{
		if(i == 0)
			y_offset = window_list[i].Y_Start;
		else
			y_offset = window_list[i].Y_Start-window_list[i-1].Y_End;
		x_offset = window_list[i].X_Start;
		/* Use full width 4196 (4096 imaging pixels + 2 x 50 bias strips)
	        ** Full Width(4196)-bias strip width (50) = 4146 : correct calculation for this value. */
		bias_x_offset = 4146-window_list[i].X_End;
		if(CCD_DSP_Command_SSP(handle,y_offset,x_offset,bias_x_offset) != CCD_DSP_DON)
		{
			Setup_Error_Number = 60;
			sprintf(Setup_Error_String,"Setting Subarray Position failed:(%d,%d,%d).",y_offset,
				x_offset,bias_x_offset);
			return FALSE;
		}
		/* total up number of columns to readout.
		** We assume number of rows are identical in each case, this is a DSP restriction and
		** is tested for in Setup_Window_List */
		total_ncols += bias_width+box_width;
	}
	/* we now need to set the dimensions of the CCD.
	** For windowing, the total area of all the windows must be set as the CCD dimensions,
	** as NSR and NPR are written back from the timing board to the PCI board as part of the
	** RDA command, which tells the PCI board how many pixels is should expect from the timing board. */
#if LOGGING > 7
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
			      "CCD_Setup_Dimensions:Final NCols = %d, Final NRows = %d.",total_ncols,box_height);
#endif
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_ADDRESS_DIMENSION_COLS,
			       total_ncols) != CCD_DSP_DON)
	{
		Setup_Error_Number = 20;
		sprintf(Setup_Error_String,"Setup_Controller_Windows:Column Setup failed(%d)",total_ncols);
		return FALSE;
	}
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_TIM_BOARD_ID,CCD_DSP_MEM_SPACE_Y,SETUP_ADDRESS_DIMENSION_ROWS,
			       box_height) != CCD_DSP_DON)
	{
		Setup_Error_Number = 21;
		sprintf(Setup_Error_String,"Setup_Controller_Windows:Row Setup failed(%d)",box_height);
		return FALSE;
	}
	return TRUE;
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.5  2015/11/03 11:31:12  cjm
** Modified bias_x_offset windowing calulation in Setup_Controller_Windows for 4k chip.
**
** Revision 1.4  2013/03/25 15:15:03  cjm
** Removed deinterlace software.
** CCD_Setup_Dimensions parameters changed, deinterlace removed.
** CCD_Setup_Dimensions now has booleans to control whether the timing board
** sofware is loaded, whether the utility board software is loaded, and whether the temperature set point is set. These are used by command line utilities.
** CCD_Setup_Dimensions rewritten, so all image dimensions are processed in the
** main routine. Extra internal variables Final_NCols and Final_NRows added, which are the Binned_NCols and Binned_NRows modified for any dummy pixels
** expected to be received.
** CCD_Setup_Hardware_Test now has boolean parameters to test whether to test
** the timing board and utility board, for command line routines used when
** no utility board is present.
**
** Revision 1.3  2012/10/25 14:38:30  cjm
** Added memory map parameter size.
**
** Revision 1.2  2012/07/17 17:18:59  cjm
** Changed how binned and unbinned cols/rows are handled.
**
** Revision 1.1  2011/11/23 10:59:52  cjm
** Initial revision
**
** Revision 0.31  2009/02/05 11:40:27  cjm
** Swapped Bitwise for Absolute logging levels.
**
** Revision 0.30  2008/12/11 16:50:26  cjm
** Added handle logging for multiple CCD system.
**
** Revision 0.29  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 0.28  2006/05/17 18:01:59  cjm
** Fixed unused variables.
**
** Revision 0.27  2006/05/16 14:14:07  cjm
** gnuify: Added GNU General Public License.
**
** Revision 0.26  2004/11/04 15:59:35  cjm
** Added processing of CCD_DSP_DEINTERLACE_FLIP.
**
** Revision 0.25  2004/05/16 14:28:18  cjm
** Re-wrote abort code.
**
** Revision 0.24  2003/06/06 12:36:01  cjm
** CHanged vacuum gauge read implementation, now issues VON/VOF and
** averages multiple samples.
** Setup_Dimensions changed, and windowing code added (SSS/SSP).
**
** Revision 0.23  2003/03/26 15:44:48  cjm
** Added windowing implementation.
** Note Bias offsets etc hardcodeed at the moment.
**
** Revision 0.22  2002/12/16 16:49:36  cjm
** Removed Error routines resetting error number to zero.
**
** Revision 0.21  2002/12/03 17:13:19  cjm
** Added CCD_Setup_Get_Vacuum_Gauge_MBar, CCD_Setup_Get_Vacuum_Gauge_ADU routines.
**
** Revision 0.20  2002/11/08 10:35:43  cjm
** Reversed order of calls of CCD_Interface_Memory_Map and Setup_PCI_Board.
** CCD_Interface_Memory_Map calls mmap, which in the device driver calls a HCVR
** command. Normally this ordering makes no difference, but it does
** when the PCI rom is not correct.
**
** Revision 0.19  2002/11/07 19:13:39  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 0.18  2001/06/04 14:42:17  cjm
** Added LOGGING code.
**
** Revision 0.17  2001/02/05 17:04:48  cjm
** More work on windowing.
**
** Revision 0.16  2001/01/31 16:35:18  cjm
** Added tests for filename is NULL in DSP download code.
**
** Revision 0.15  2000/12/19 17:52:47  cjm
** New filter wheel code.
**
** Revision 0.14  2000/06/19 08:48:34  cjm
** Backup.
**
** Revision 0.13  2000/06/13 17:14:13  cjm
** Changes to make Ccs agree with voodoo.
**
** Revision 0.12  2000/05/26 08:56:09  cjm
** Added CCD_Setup_Get_Window.
**
** Revision 0.11  2000/05/25 08:44:46  cjm
** Gain settings now held in Setup_Data so that CCD_Setup_Get_Gain
** can return the current gain.
** Some changes to internal routines (parameter checking now in them where applicable).
**
** Revision 0.10  2000/04/13 13:06:59  cjm
** Added current time to error routines.
**
** Revision 0.10  2000/04/13 13:04:46  cjm
** Changed error routine to print out current time.
**
** Revision 0.9  2000/03/02 16:46:53  cjm
** Converted Setup_Hardware_Test from internal routine to external CCD_Setup_Hardware_Test.
**
** Revision 0.8  2000/03/01 15:44:41  cjm
** Backup.
**
** Revision 0.7  2000/02/23 11:54:00  cjm
** Removed setting reply buffer bit on startup/shutdown.
** This is now handled by the latest astropci driver.
**
** Revision 0.6  2000/02/14 17:09:35  cjm
** Added some get routines to get data from Setup_Data:
** CCD_Setup_Get_NSBin
** CCD_Setup_Get_NPBin
** CCD_Setup_Get_Window_Flags
** CCD_Setup_Get_Filter_Wheel_Position
** These are to be used for get status commands.
**
** Revision 0.5  2000/02/10 12:07:38  cjm
** Added interface board to hardware test.
**
** Revision 0.4  2000/02/10 12:01:19  cjm
** Modified CCD_Setup_Shutdown with call to CCD_DSP_Command_WRM_No_Reply to switch off controller replies.
**
** Revision 0.3  2000/02/02 15:58:32  cjm
** Changed SETUP_ATTR to struct Setup_Struct.
**
** Revision 0.2  2000/02/02 15:55:14  cjm
** Binning and windowing addded.
**
** Revision 0.1  2000/01/25 14:57:27  cjm
** initial revision (PCI version).
**
*/
