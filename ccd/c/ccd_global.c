/* ccd_global.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_global.c,v 1.2 2013-03-25 15:15:03 cjm Exp $
*/
/**
 * ccd_global.c contains routines that tie together all the modules that make up libccd.
 * @author SDSU, Chris Mottram
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
/**
 * This hash define is needed to make header files include X/Open UNIX entensions.
 * This allows us to use BSD/SVr4 function calls even when  _POSIX_C_SOURCE is defined.
 * We conditionally use <b>getpriority</b> and <b>setpriority</b> in this module 
 * (CCD_GLOBAL_READOUT_PRIORITY dependant).
 * If these lines are not included, the fact that _POSIX_C_SOURCE is defined
 * causes struct timeval not to be defined in time.h, and then resource.h complains about this (under Solaris).
 */
#define _XOPEN_SOURCE		(1)
/**
 * This hash define is needed to make header files include X/Open UNIX entensions.
 * This allows us to use BSD/SVr4 function calls even when  _POSIX_C_SOURCE is defined.
 * We conditionally use <b>getpriority</b> and <b>setpriority</b> in this module 
 * (CCD_GLOBAL_READOUT_PRIORITY dependant).
 * If these lines are not included, the fact that _POSIX_C_SOURCE is defined
 * causes struct timeval not to be defined in time.h, and then resource.h complains about this (under Solaris).
 */
#define _XOPEN_SOURCE_EXTENDED 	(1)

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#if CCD_GLOBAL_READOUT_PRIORITY == 0
/* include nothing for normal priority readout */
#elif CCD_GLOBAL_READOUT_PRIORITY == 1
#include <sched.h>
#elif CCD_GLOBAL_READOUT_PRIORITY == 2
#include <sys/time.h>
#include <sys/resource.h>
#else
#error "ccd_global.c:CCD_GLOBAL_READOUT_PRIORITY has an illegal value - please define to 0/1/2."
#endif /* CCD_GLOBAL_READOUT_PRIORITY */
#ifdef CCD_GLOBAL_READOUT_MLOCK
#include <sys/mman.h>
#endif /* CCD_GLOBAL_READOUT_MLOCK */
#include "log_udp.h"
#include "ccd_dsp.h"
#include "ccd_dsp_download.h"
#include "ccd_exposure.h"
#include "ccd_filter_wheel.h"
#include "ccd_global.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_pixel_stream.h"
#include "ccd_setup.h"
#include "ccd_text.h"
#include "ccd_temperature.h"

/* hash definitions */
/**
 * This is used when increasing process priority during readout. We find the maximum priority the
 * scheduler we intend to use supports, and then set our process priority to the maximum priority minus
 * this value. This means that there are higher process priorities than the one are process is set to,
 * so we can (theoretically) interrupt the process if neccessary.
 */
#define GLOBAL_PRIORITY_OFFSET			(5)
#ifndef PAGESIZE
/**
 * Horrible bodge definition. If PAGESIZE is not defined in limits.h, we define it here
 * using sysconf to retrieve it's value. This means extra function calls that we don't want.
 */
#define PAGESIZE	(sysconf(_SC_PAGESIZE))
#endif
/**
 * Macro to round an address down to the start of a page of memory. Uses PAGESIZE in limits.h.
 * Based on macro in <i>Programming for the real world, POSIX.4</i>,P 197.
 * @param v A memory address.
 * @return The memory address is returned, rounded down to the start of the page.
 * @see #PAGESIZE
 */
#define GLOBAL_ROUND_DOWN_TO_PAGE(v)	((unsigned long)(v) & ~(PAGESIZE-1))
/**
 * Macro to round an address up to a start of a page of memory. Uses PAGESIZE in limits.h.
 * Based on macro in <i>Programming for the real world, POSIX.4</i>,P 197.
 * @param v A memory address, or length of a buffer.
 * @return The memory address/size is returned, rounded up to the start of a page.
 * @see #PAGESIZE
 */
#define GLOBAL_ROUND_UP_TO_PAGE(v)		(((unsigned long)(v) + PAGESIZE -1)& ~(PAGESIZE-1))

/* data types */
/**
 * Data type holding local data to ccd_global. This consists of the following:
 * <dl>
 * <dt>Saved_Scheduling_Parameters</dt><dd>This field only exists if the data is compiled with 
 * 	CCD_GLOBAL_READOUT_PRIORITY = 1. This saves the current scheduling parameters before they are changed
 * 	for reading out data, if we are using POSIX.4 scheduling parameters.</dd>
 * <dt>Saved_Scheduling_Algorithm</dt><dd>This field only exists if the data is compiled with 
 * 	CCD_GLOBAL_READOUT_PRIORITY = 1. This saves the current scheduling algorithm before it is changed
 * 	for reading out data, if we are using POSIX.4 scheduling parameters.</dd>
 * <dt>Old_Priority</dt><dd>This field only exists if the data is compiled with 
 * 	CCD_GLOBAL_READOUT_PRIORITY = 2. This saves the current process priority before it is changed
 * 	for reading out data, if we are using BSD/SVr4 process management.</dd>
 * <dt>Global_Log_Handler</dt> <dd>Function pointer to the routine that will log messages passed to it.</dd>
 * <dt>Global_Log_Filter</dt> <dd>Function pointer to the routine that will filter log messages passed to it.
 * 		The funtion will return TRUE if the message should be logged, and FALSE if it shouldn't.</dd>
 * <dt>Global_Log_Filter_Level</dt> <dd>A globally maintained log filter level. 
 * 		This is set using CCD_Global_Set_Log_Filter_Level.
 * 		CCD_Global_Log_Filter_Level_Absolute and CCD_Global_Log_Filter_Level_Bitwise test it against
 * 		message levels to determine whether to log messages.</dd>
 * </dl>
 * @see #CCD_Global_Log
 * @see #CCD_Global_Set_Log_Filter_Level
 * @see #CCD_Global_Log_Filter_Level_Absolute
 * @see #CCD_Global_Log_Filter_Level_Bitwise
 */
struct Global_Struct
{
#if CCD_GLOBAL_READOUT_PRIORITY == 1
	struct sched_param Saved_Scheduling_Parameters;
	int Saved_Scheduling_Algorithm;
#elif CCD_GLOBAL_READOUT_PRIORITY == 2
	int Old_Priority;
#endif
	void (*Global_Log_Handler)(int level,char *string);
	int (*Global_Log_Filter)(int level,char *string);
	int Global_Log_Filter_Level;
};

/* internal data */
/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_global.c,v 1.2 2013-03-25 15:15:03 cjm Exp $";
/**
 * Variable holding error code of last operation performed by ccd_dsp.
 */
static int Global_Error_Number = 0;
/**
 * Internal variable holding description of the last error that occured.
 */
static char Global_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";
/**
 * The instance of Global_Struct that contains local data for this module.
 * This is statically initialised to the following:
 * <dl>
 * <dt>Saved_Scheduling_Parameters</dt> <dd>If compiled in, {0}</dd>
 * <dt>Saved_Scheduling_Algorithm</dt> <dd>If compiled in, 0</dd>
 * <dt>Old_Priority</dt> <dd>If compiled in, 0</dd>
 * <dt>Global_Log_Handler</dt> <dd>NULL</dd>
 * <dt>Global_Log_Filter</dt> <dd>NULL</dd>
 * <dt>Global_Log_Filter_Level</dt> <dd>0</dd>
 * </dl>
 * @see #Global_Struct
 */
static struct Global_Struct Global_Data = 
{
#if CCD_GLOBAL_READOUT_PRIORITY == 1
	{0},
	0,
#elif CCD_GLOBAL_READOUT_PRIORITY == 2
	0,
#endif
	NULL,NULL,0
};

/**
 * General buffer used for string formatting during logging.
 * @see #CCD_GLOBAL_ERROR_STRING_LENGTH
 */
static char Global_Buff[CCD_GLOBAL_ERROR_STRING_LENGTH];

/* ----------------------------------------------------------------------------
** 		external functions 
** ---------------------------------------------------------------------------- */
/**
 * This routine calls all initialisation routines for modules in libccd. These routines are generally used to
 * initialise parts of the library. This routine should be called at the start of a program.
 * <i>
 * Note some of the initialisation can produce errors. Currently these are just printed out, this should
 * perhaps return an error code when this occurs.
 * </i>
 * @see #Global_Data
 * @see ccd_interface.html#CCD_Interface_Initialise
 * @see ccd_dsp.html#CCD_DSP_Initialise
 * @see ccd_dsp_download.html#CCD_DSP_Download_Initialise
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Initialise
 * @see ccd_exposure.html#CCD_Exposure_Initialise
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Initialise
 * @see ccd_setup.html#CCD_Setup_Initialise
 */
void CCD_Global_Initialise(void)
{
	CCD_Interface_Initialise();
	if(!CCD_DSP_Initialise())
		CCD_DSP_Error();
	if(!CCD_DSP_Download_Initialise())
		CCD_DSP_Download_Error();
	CCD_Exposure_Initialise();
	CCD_Pixel_Stream_Initialise();
	CCD_Setup_Initialise();
	CCD_Filter_Wheel_Initialise();
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_Global_Initialise:%s.\n",rcsid);
#if CCD_GLOBAL_READOUT_PRIORITY == 0
	fprintf(stdout,"CCD_Global_Initialise:Process at normal priority during image readout.\n");
#elif CCD_GLOBAL_READOUT_PRIORITY == 1
	fprintf(stdout,"CCD_Global_Initialise:Process at realtime priority (POSIX.4/SCHED_FIFO)"
		" during image readout.\n");
#elif CCD_GLOBAL_READOUT_PRIORITY == 2
	fprintf(stdout,"CCD_Global_Initialise:Process at higher priority (BSD/SVr4) during image readout.\n");
#else
#error "ccd_global.c:CCD_GLOBAL_READOUT_PRIORITY has an illegal value - please define to 0/1/2."
#endif
#ifdef CCD_GLOBAL_READOUT_MLOCK
	fprintf(stdout,"CCD_Global_Initialise:Readout memory locked:cannot be swapped to disc.\n");
#else
	fprintf(stdout,"CCD_Global_Initialise:Readout memory unlocked:can be swapped to disc.\n");
#endif
	fflush(stdout);
}

/**
 * A general error routine. This checks the error numbers for all the modules that make up the library, and
 * for any non-zero numbers prints out the error message to stderr.
 * <b>Note</b> you cannot call both CCD_Global_Error and CCD_Global_Error_String to print the error string and 
 * get a string copy of it, only one of the error routines can be called after libccd has generated an error.
 * A second call to one of these routines will generate a 'Error not found' error!.
 * @see ccd_setup.html#CCD_Setup_Get_Error_Number
 * @see ccd_setup.html#CCD_Setup_Error
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Get_Error_Number
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Error
 * @see ccd_exposure.html#CCD_Exposure_Get_Error_Number
 * @see ccd_exposure.html#CCD_Exposure_Error
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Get_Error_Number
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Error
 * @see ccd_temperature.html#CCD_Temperature_Get_Error_Number
 * @see ccd_temperature.html#CCD_Temperature_Error
 * @see ccd_dsp.html#CCD_DSP_Get_Error_Number
 * @see ccd_dsp.html#CCD_DSP_Error
 * @see ccd_dsp_download.html#CCD_DSP_Download_Get_Error_Number
 * @see ccd_dsp_download.html#CCD_DSP_Download_Error
 * @see ccd_interface.html#CCD_Interface_Get_Error_Number
 * @see ccd_interface.html#CCD_Interface_Error
 * @see ccd_pci.html#CCD_PCI_Get_Error_Number
 * @see ccd_pci.html#CCD_PCI_Error
 * @see #CCD_Global_Get_Current_Time_String
 * @see #Global_Error_Number
 * @see #Global_Error_String
 */
void CCD_Global_Error(void)
{
	char time_string[32];
	int found = FALSE;

	if(CCD_Pixel_Stream_Get_Error_Number() != 0)
	{
		found = TRUE;
		CCD_Pixel_Stream_Error();
	}
	if(CCD_Setup_Get_Error_Number() != 0)
	{
		found = TRUE;
		CCD_Setup_Error();
	}
	if(CCD_Exposure_Get_Error_Number() != 0)
	{
		found = TRUE;
		CCD_Exposure_Error();
	}
	if(CCD_Filter_Wheel_Get_Error_Number() != 0)
	{
		found = TRUE;
		CCD_Filter_Wheel_Error();
	}
	if(CCD_Temperature_Get_Error_Number() != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t");
		CCD_Temperature_Error();
	}
	if(CCD_DSP_Download_Get_Error_Number() != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t");
		CCD_DSP_Download_Error();
	}
	if(CCD_DSP_Get_Error_Number() != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t");
		CCD_DSP_Error();
	}
	if(CCD_Interface_Get_Error_Number() != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t\t");
		CCD_Interface_Error();
	}
	if(CCD_PCI_Get_Error_Number() != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t\t\t");
		CCD_PCI_Error();
	}
	if(CCD_Text_Get_Error_Number() != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t\t\t");
		CCD_Text_Error();
	}
	if(Global_Error_Number != 0)
	{
		found = TRUE;
		fprintf(stderr,"\t\t\t");
		CCD_Global_Get_Current_Time_String(time_string,32);
		fprintf(stderr,"%s CCD_Global:Error(%d) : %s\n",time_string,Global_Error_Number,Global_Error_String);
	}
	if(!found)
	{
		fprintf(stderr,"Error:CCD_Global_Error:Error not found\n");
	}
}

/**
 * A general error routine. This checks the error numbers for all the modules that make up the library, and
 * for any non-zero numbers adds the error message to a passed in string. The string parameter is set to the
 * blank string initially.
 * <b>Note</b> you cannot call both CCD_Global_Error and CCD_Global_Error_String to print the error string and 
 * get a string copy of it, only one of the error routines can be called after libccd has generated an error.
 * A second call to one of these routines will generate a 'Error not found' error!.
 * @param error_string A character buffer big enough to store the longest possible error message. It is
 * recomended that it is at least 1024 bytes in size.
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Get_Error_Number
 * @see ccd_pixel_stream.html#CCD_Pixel_Stream_Error_String
 * @see ccd_setup.html#CCD_Setup_Get_Error_Number
 * @see ccd_setup.html#CCD_Setup_Error_String
 * @see ccd_exposure.html#CCD_Exposure_Get_Error_Number
 * @see ccd_exposure.html#CCD_Exposure_Error_String
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Get_Error_Number
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Error_String
 * @see ccd_temperature.html#CCD_Temperature_Get_Error_Number
 * @see ccd_temperature.html#CCD_Temperature_Error_String
 * @see ccd_dsp.html#CCD_DSP_Get_Error_Number
 * @see ccd_dsp.html#CCD_DSP_Error_String
 * @see ccd_dsp_download.html#CCD_DSP_Download_Get_Error_Number
 * @see ccd_dsp_download.html#CCD_DSP_Download_Error_String
 * @see ccd_interface.html#CCD_Interface_Get_Error_Number
 * @see ccd_interface.html#CCD_Interface_Error_String
 * @see ccd_pci.html#CCD_PCI_Get_Error_Number
 * @see ccd_pci.html#CCD_PCI_Error_String
 * @see #CCD_Global_Get_Current_Time_String
 * @see #Global_Error_Number
 * @see #Global_Error_String
 */
void CCD_Global_Error_String(char *error_string)
{
	char time_string[32];

	strcpy(error_string,"");
	if(CCD_Setup_Get_Error_Number() != 0)
	{
		CCD_Setup_Error_String(error_string);
	}
	if(CCD_Pixel_Stream_Get_Error_Number() != 0)
	{
		CCD_Pixel_Stream_Error_String(error_string);
	}
	if(CCD_Exposure_Get_Error_Number() != 0)
	{
		CCD_Exposure_Error_String(error_string);
	}
	if(CCD_Filter_Wheel_Get_Error_Number() != 0)
	{
		CCD_Filter_Wheel_Error_String(error_string);
	}
	if(CCD_Temperature_Get_Error_Number() != 0)
	{
		strcat(error_string,"\t");
		CCD_Temperature_Error_String(error_string);
	}
	if(CCD_DSP_Download_Get_Error_Number() != 0)
	{
		strcat(error_string,"\t");
		CCD_DSP_Download_Error_String(error_string);
	}
	if(CCD_DSP_Get_Error_Number() != 0)
	{
		strcat(error_string,"\t");
		CCD_DSP_Error_String(error_string);
	}
	if(CCD_Interface_Get_Error_Number() != 0)
	{
		strcat(error_string,"\t\t");
		CCD_Interface_Error_String(error_string);
	}
	if(CCD_PCI_Get_Error_Number() != 0)
	{
		strcat(error_string,"\t\t\t");
		CCD_PCI_Error_String(error_string);
	}
	if(CCD_Text_Get_Error_Number() != 0)
	{
		strcat(error_string,"\t\t\t");
		CCD_Text_Error_String(error_string);
	}
	if(Global_Error_Number != 0)
	{
		CCD_Global_Get_Current_Time_String(time_string,32);
		sprintf(error_string+strlen(error_string),"%s CCD_Global:Error(%d) : %s\n",time_string,
			Global_Error_Number,Global_Error_String);
	}
	if(strlen(error_string) == 0)
	{
		strcat(error_string,"Error:CCD_Global_Error:Error not found\n");
	}
}

/**
 * Routine to get the current time in a string. The string is returned in the format
 * '01/01/2000 13:59:59', or the string "Unknown time" if the routine failed.
 * The time is in UTC.
 * @param time_string The string to fill with the current time.
 * @param string_length The length of the buffer passed in. It is recommended the length is at least 20 characters.
 */
void CCD_Global_Get_Current_Time_String(char *time_string,int string_length)
{
	time_t current_time;
	struct tm *utc_time = NULL;

	if(time(&current_time) > -1)
	{
		utc_time = gmtime(&current_time);
		strftime(time_string,string_length,"%d/%m/%Y %H:%M:%S",utc_time);
	}
	else
		strncpy(time_string,"Unknown time",string_length);
}

/**
 * Routine to log a message to a defined logging mechanism. This routine has an arbitary number of arguments,
 * and uses vsprintf to format them i.e. like fprintf. A buffer is used to hold the created string,
 * therefore the total length of the generated string should not be longer than CCD_GLOBAL_ERROR_STRING_LENGTH.
 * CCD_Global_Log is then called to handle the log message.
 * @param level An integer, used to decide whether this particular message has been selected for
 * 	logging or not.
 * @param format A string, with formatting statements the same as fprintf would use to determine the type
 * 	of the following arguments.
 * @see #CCD_Global_Log
 * @see #CCD_GLOBAL_ERROR_STRING_LENGTH
 */
void CCD_Global_Log_Format(int level,char *format,...)
{
	char buff[1024];
	va_list ap;

/* format the arguments */
	va_start(ap,format);
	vsprintf(buff,format,ap);
	va_end(ap);
/* call the log routine to log the results */
	CCD_Global_Log(level,buff);
}

/**
 * Routine to log a message to a defined logging mechanism. If the string or Global_Data.Global_Log_Handler are NULL
 * the routine does not log the message. If the Global_Data.Global_Log_Filter function pointer is non-NULL, the
 * message is passed to it to determine whether to log the message.
 * @param level An integer, used to decide whether this particular message has been selected for
 * 	logging or not.
 * @param string The message to log.
 * @see #Global_Data
 */
void CCD_Global_Log(int level,char *string)
{
/* If the string is NULL, don't log. */
	if(string == NULL)
		return;
/* If there is no log handler, return */
	if(Global_Data.Global_Log_Handler == NULL)
		return;
/* If there's a log filter, check it returns TRUE for this message */
	if(Global_Data.Global_Log_Filter != NULL)
	{
		if(Global_Data.Global_Log_Filter(level,string) == FALSE)
			return;
	}
/* We can log the message */
	(*Global_Data.Global_Log_Handler)(level,string);
}

/**
 * Routine to set the Global_Data.Global_Log_Handler used by CCD_Global_Log.
 * @param log_fn A function pointer to a suitable handler.
 * @see #Global_Data
 * @see #CCD_Global_Log
 */
void CCD_Global_Set_Log_Handler_Function(void (*log_fn)(int level,char *string))
{
	Global_Data.Global_Log_Handler = log_fn;
}

/**
 * Routine to set the Global_Data.Global_Log_Filter used by CCD_Global_Log.
 * @param log_fn A function pointer to a suitable filter function.
 * @see #Global_Data
 * @see #CCD_Global_Log
 */
void CCD_Global_Set_Log_Filter_Function(int (*filter_fn)(int level,char *string))
{
	Global_Data.Global_Log_Filter = filter_fn;
}

/**
 * A log handler to be used for the Global_Data.Global_Log_Handler function.
 * Just prints the message to stdout, terminated by a newline.
 * @param level The log level for this message.
 * @param string The log message to be logged. 
 * @see #CCD_Global_Get_Current_Time_String
 */
void CCD_Global_Log_Handler_Stdout(int level,char *string)
{
	char time_string[32];

	if(string == NULL)
		return;
	CCD_Global_Get_Current_Time_String(time_string,32);
	fprintf(stdout,"%s : %s\n",time_string,string);
}

/**
 * Routine to set the Global_Data.Global_Log_Filter_Level.
 * @see #Global_Data
 */
void CCD_Global_Set_Log_Filter_Level(int level)
{
	Global_Data.Global_Log_Filter_Level = level;
}

/**
 * A log message filter routine, to be used for the Global_Data.Global_Log_Filter function pointer.
 * @param level The log level of the message to be tested.
 * @param string The log message to be logged, not used in this filter. 
 * @return The routine returns TRUE if the level is less than or equal to the Global_Data.Global_Log_Filter_Level,
 * 	otherwise it returns FALSE.
 * @see #Global_Data
 */
int CCD_Global_Log_Filter_Level_Absolute(int level,char *string)
{
	return (level <= Global_Data.Global_Log_Filter_Level);
}

/**
 * A log message filter routine, to be used for the Global_Data.Global_Log_Filter function pointer.
 * @param level The log level of the message to be tested.
 * @param string The log message to be logged, not used in this filter. 
 * @return The routine returns TRUE if the level has bits set that are also set in the 
 * 	Global_Data.Global_Log_Filter_Level, otherwise it returns FALSE.
 * @see #Global_Data
 */
int CCD_Global_Log_Filter_Level_Bitwise(int level,char *string)
{
	return ((level & Global_Data.Global_Log_Filter_Level) > 0);
}

/**
 * This routine increases the scheduling/priority
 * of this process. It is called whilst reading out images from the camera, and this reduces the 
 * chance of this process being interrupted during a readout, which can cause the readout to fail.
 * The process scheduling/priority is only changed if CCD_GLOBAL_READOUT_PRIORITY is defined to be 
 * non-zero.
 * @return The routine returns TRUE if it succeeds, FALSE if it fails.
 * @see #Global_Data
 * @see #CCD_Global_Decrease_Priority
 * @see #GLOBAL_PRIORITY_OFFSET
 */
int CCD_Global_Increase_Priority(void)
{
#if CCD_GLOBAL_READOUT_PRIORITY == 1
	struct sched_param scheduling_parameters;
#endif
#if CCD_GLOBAL_READOUT_PRIORITY > 0
	int scheduling_errno,retval;
#endif

#if CCD_GLOBAL_READOUT_PRIORITY == 0
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_PRIORITY is 0 (normal priority).");
#endif /* LOGGING */
#elif CCD_GLOBAL_READOUT_PRIORITY == 1
#ifdef _POSIX_PRIORITY_SCHEDULING
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_PRIORITY is 1 (POSIX.4 sched).");
#endif /* LOGGING */
/* get current priority and save */
	Global_Data.Saved_Scheduling_Algorithm = sched_getscheduler(0);
	if(Global_Data.Saved_Scheduling_Algorithm < 0)
	{
		scheduling_errno = errno;
		Global_Error_Number = 1;
		sprintf(Global_Error_String,"CCD_Global_Increase_Priority:"
			"Failed to get scheduling algorithm. (%d)",scheduling_errno);
		return FALSE;
	}
	retval = sched_getparam(0,&Global_Data.Saved_Scheduling_Parameters);
	if(retval < 0)
	{
		scheduling_errno = errno;
		Global_Error_Number = 2;
		sprintf(Global_Error_String,"CCD_Global_Increase_Priority:"
			"Failed to get scheduling parameters. (%d)",scheduling_errno);
		return FALSE;
	}
#if LOGGING > 3
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"Current scheduling:scheduler=%d,priority=%d.",
		Global_Data.Saved_Scheduling_Algorithm,Global_Data.Saved_Scheduling_Parameters.sched_priority);
#endif /* LOGGING */
/* increase priority to maximum */
	scheduling_parameters = Global_Data.Saved_Scheduling_Parameters;
	retval = sched_get_priority_max(SCHED_FIFO);
	if(retval < 0)
	{
		scheduling_errno = errno;
		Global_Error_Number = 3;
		sprintf(Global_Error_String,"CCD_Global_Increase_Priority:"
			"Failed to get scheduler max priority.(%d,SCHED_FIFO)",scheduling_errno);
		return FALSE;
	}
	scheduling_parameters.sched_priority = retval-GLOBAL_PRIORITY_OFFSET;
#if LOGGING > 3
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"Setting scheduling to:scheduler=SCHED_FIFO,priority=%d.",
		scheduling_parameters.sched_priority);
#endif /* LOGGING */
	retval = sched_setscheduler(0,SCHED_FIFO,&scheduling_parameters);
	if(retval < 0)
	{
		scheduling_errno = errno;
		Global_Error_Number = 4;
		sprintf(Global_Error_String,"CCD_Global_Increase_Priority: Failed to set scheduler.(%d,%d)",
			scheduling_errno,scheduling_parameters.sched_priority);
		return FALSE;
	}
#else
#error "ccd_global.c:CCD_Global_Increase_Priority:"
	"compiled with CCD_GLOBAL_READOUT_PRIORITY but POSIX.4 PRIORITY_SCHEDULING Support not present"
#endif /* _POSIX_PRIORITY_SCHEDULING defined */
#elif CCD_GLOBAL_READOUT_PRIORITY == 2
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_PRIORITY is 2 (SVr4/BSD priority).");
#endif /* LOGGING */
/* get current priority into old_Priority of our process (0) */
	Global_Data.Old_Priority = getpriority(PRIO_PROCESS,0);
#if LOGGING > 3
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"Current priority=%d.",Global_Data.Old_Priority);
#endif /* LOGGING */
/* set to highest priority */
	retval = setpriority(PRIO_PROCESS,0,-20);
	if(retval == -1)
	{
		scheduling_errno = errno;
		Global_Error_Number = 5;
		sprintf(Global_Error_String,"CCD_Global_Increase_Priority: Failed to set priority(-20,%d).",
			scheduling_errno);
		return FALSE;
	}
#if LOGGING > 3
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"Set priority=%d.",getpriority(PRIO_PROCESS,0));
#endif /* LOGGING */
#endif /* CCD_GLOBAL_READOUT_PRIORITY */
	return TRUE;
}

/**
 * This routine resets the scheduling/priority
 * of this process, using values saved during CCD_Global_Increase_Priority. 
 * The process scheduling/priority is only changed if CCD_GLOBAL_READOUT_PRIORITY is defined to be 
 * non-zero.
 * @return The routine returns TRUE if it succeeds, FALSE if it fails.
 * @see #Global_Data
 * @see #CCD_Global_Increase_Priority
 */
int CCD_Global_Decrease_Priority(void)
{
#if CCD_GLOBAL_READOUT_PRIORITY > 0
	int scheduling_errno,retval;
#endif

#if CCD_GLOBAL_READOUT_PRIORITY == 1
#ifdef _POSIX_PRIORITY_SCHEDULING
#if LOGGING > 3
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"Resetting scheduling to:scheduler=%d,priority=%d.",
		Global_Data.Saved_Scheduling_Algorithm,Global_Data.Saved_Scheduling_Parameters.sched_priority);
#endif /* LOGGING */
	retval = sched_setscheduler(0,Global_Data.Saved_Scheduling_Algorithm,
		&Global_Data.Saved_Scheduling_Parameters);
	if(retval < 0)
	{
		scheduling_errno = errno;
		Global_Error_Number = 6;
		sprintf(Global_Error_String,"CCD_Global_Decrease_Priority:"
			"Failed to reset scheduler.(%d,%d,%d)",
			scheduling_errno,Global_Data.Saved_Scheduling_Algorithm,
			Global_Data.Saved_Scheduling_Parameters.sched_priority);
		return FALSE;
	}
#else
#error "ccd_global.c:CCD_Global_Decrease_Priority:"
	"compiled with CCD_GLOBAL_READOUT_PRIORITY but POSIX.4 PRIORITY_SCHEDULING Support not present"
#endif /* _POSIX_PRIORITY_SCHEDULING defined. */
#elif CCD_GLOBAL_READOUT_PRIORITY == 2
/* set back to old priority */
	retval = setpriority(PRIO_PROCESS,0,Global_Data.Old_Priority);
	if(retval == -1)
	{
		scheduling_errno = errno;
		Global_Error_Number = 7;
		sprintf(Global_Error_String,"CCD_Global_Decrease_Priority: Failed to set priority(%d,%d).",
			scheduling_errno,Global_Data.Old_Priority);
		return FALSE;
	}
#if LOGGING > 3
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"Reset priority=%d.",getpriority(PRIO_PROCESS,0));
#endif /* LOGGING */
#endif /* CCD_GLOBAL_READOUT_PRIORITY */
	return TRUE;
}

/**
 * This routine locks the
 * image data memory passed in into physical memory, to stop it being paged out during readout.
 * The memory is locked only if CCD_GLOBAL_READOUT_MLOCK has been defined, and the operating system
 * supports _POSIX_MEMLOCK_RANGE. The actual address range locked is done in complete pages, for
 * portability considerations.
 * @param image_data A pointer to the image data.
 * @param image_data_size The size of the image data.
 * @return The routine returns TRUE if it suceeded and FALSE if an error occured.
 * @see #GLOBAL_ROUND_DOWN_TO_PAGE
 * @see #GLOBAL_ROUND_UP_TO_PAGE
 */
int CCD_Global_Memory_Lock(unsigned short *image_data,int image_data_size)
{
#ifdef CCD_GLOBAL_READOUT_MLOCK
	int mlock_errno,retval;
#endif

#ifdef CCD_GLOBAL_READOUT_MLOCK
#ifdef _POSIX_MEMLOCK_RANGE
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_MLOCK is defined: locking readout memory.");
#endif /* LOGGING */
	retval = mlock((unsigned short *)GLOBAL_ROUND_DOWN_TO_PAGE(image_data),
		GLOBAL_ROUND_UP_TO_PAGE(image_data_size));
	if(retval == -1)
	{
		mlock_errno = errno;
		Global_Error_Number = 8;
		sprintf(Global_Error_String,"CCD_Global_Memory_Lock:"
			"Failed to mlock image data (%p(%p),%d(%d),%d).",
			image_data,GLOBAL_ROUND_DOWN_TO_PAGE(image_data),image_data_size,
			GLOBAL_ROUND_UP_TO_PAGE(image_data_size),mlock_errno);
		return FALSE;

	}
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_MLOCK:readout memory locked.");
#endif /* LOGGING */
#else
#error "ccd_global.c:compiled with CCD_GLOBAL_READOUT_MLOCK but POSIX.4 MEMLOCK_RANGE Support not present."
#endif /* _POSIX_MEMLOCK_RANGE */
#endif /* CCD_GLOBAL_READOUT_MLOCK */
	return TRUE;
}

/**
 * This routine un-locks the image data memory passed in, which was locked in CCD_Global_Memory_Lock.
 * The memory is un-locked only if CCD_GLOBAL_READOUT_MLOCK has been defined, and the operating system
 * supports _POSIX_MEMLOCK_RANGE.
 * @param image_data A pointer to the image data.
 * @param image_data_size The size of the image data.
 * @return The routine returns TRUE if it suceeded and FALSE if an error occured.
 * @see #GLOBAL_ROUND_DOWN_TO_PAGE
 * @see #GLOBAL_ROUND_UP_TO_PAGE
 */
int CCD_Global_Memory_UnLock(unsigned short *image_data,int image_data_size)
{
#ifdef CCD_GLOBAL_READOUT_MLOCK
	int munlock_errno,retval;
#endif

#ifdef CCD_GLOBAL_READOUT_MLOCK
#ifdef _POSIX_MEMLOCK_RANGE
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_MLOCK is defined:unlocking readout memory.");
#endif /* LOGGING */
	retval = munlock((unsigned short *)GLOBAL_ROUND_DOWN_TO_PAGE(image_data),
		GLOBAL_ROUND_UP_TO_PAGE(image_data_size));
	if(retval == -1)
	{
		munlock_errno = errno;
		Global_Error_Number = 9;
		sprintf(Global_Error_String,"CCD_Global_Memory_UnLock:"
			"Failed to munlock image data (%p(%p),%d(%d),%d).",
			image_data,GLOBAL_ROUND_DOWN_TO_PAGE(image_data),
			image_data_size,GLOBAL_ROUND_UP_TO_PAGE(image_data_size),munlock_errno);
		return FALSE;
	}
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_EXPOSURE_READOUT_MLOCK:readout memory is unlocked.");
#endif /* LOGGING */
#else
#error "ccd_global.c:compiled with CCD_GLOBAL_READOUT_MLOCK but POSIX.4 MEMLOCK_RANGE Support not present."
#endif /* _POSIX_MEMLOCK_RANGE */
#endif /* CCD_GLOBAL_READOUT_MLOCK */
	return TRUE;
}

/**
 * This routine locks all the processes memory, to stop it being paged out during readout.
 * The memory is locked only if CCD_GLOBAL_READOUT_MLOCK has been defined, and the operating system
 * supports _POSIX_MEMLOCK. 
 * @return The routine returns TRUE if it suceeded and FALSE if an error occured.
 * @see #CCD_Global_Memory_UnLock_All
 */
int CCD_Global_Memory_Lock_All(void)
{
#ifdef CCD_GLOBAL_READOUT_MLOCK
	int mlock_errno,retval;
#endif

#ifdef CCD_GLOBAL_READOUT_MLOCK
#ifdef _POSIX_MEMLOCK
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_MLOCK is defined: locking all memory.");
#endif /* LOGGING */
	retval = mlockall(MCL_CURRENT|MCL_FUTURE);
	if(retval == -1)
	{
		mlock_errno = errno;
		Global_Error_Number = 10;
		sprintf(Global_Error_String,"CCD_Global_Memory_Lock_All:Failed to mlockall(%d).",
			mlock_errno);
		return FALSE;

	}
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_MLOCK:all memory locked.");
#endif /* LOGGING */
#else
#error "ccd_global.c:compiled with CCD_GLOBAL_READOUT_MLOCK but POSIX.4 MEMLOCK Support not present."
#endif /* _POSIX_MEMLOCK */
#endif /* CCD_GLOBAL_READOUT_MLOCK */
	return TRUE;
}

/**
 * This routine un-locks the all the processes memory, which was locked in CCD_Global_Memory_Lock_All.
 * The memory is un-locked only if CCD_GLOBAL_READOUT_MLOCK has been defined, and the operating system
 * supports _POSIX_MEMLOCK.
 * @return The routine returns TRUE if it suceeded and FALSE if an error occured.
 * @see #CCD_Global_Memory_Lock_All
 */
int CCD_Global_Memory_UnLock_All(void)
{
#ifdef CCD_GLOBAL_READOUT_MLOCK
	int munlock_errno,retval;
#endif

#ifdef CCD_GLOBAL_READOUT_MLOCK
#ifdef _POSIX_MEMLOCK
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_READOUT_MLOCK is defined:unlocking all memory.");
#endif /* LOGGING */
	retval = munlockall();
	if(retval == -1)
	{
		munlock_errno = errno;
		Global_Error_Number = 11;
		sprintf(Global_Error_String,"CCD_Global_Memory_UnLock_All:Failed to munlockall(%d).",
			munlock_errno);
		return FALSE;
	}
#if LOGGING > 3
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_GLOBAL_EXPOSURE_READOUT_MLOCK:all memory is unlocked.");
#endif /* LOGGING */
#else
#error "ccd_global.c:compiled with CCD_GLOBAL_READOUT_MLOCK but POSIX.4 MEMLOCK Support not present."
#endif /* _POSIX_MEMLOCK */
#endif /* CCD_GLOBAL_READOUT_MLOCK */
	return TRUE;
}

/**
 * Routine to add a number of milliseconds to the specified timespec.
 * @param time The address of a timespec to add the milliseconds to.
 * @param ms The number of milliseconds to add.
 * @see #CCD_GLOBAL_ONE_SECOND_NS
 * @see #CCD_GLOBAL_ONE_MILLISECOND_NS
 */
void CCD_Global_Add_Time_Ms(struct timespec *time,int ms)
{
	time->tv_nsec += ms*CCD_GLOBAL_ONE_MILLISECOND_NS;
	/* This shouldn't be necesary, unless someone is mad enough to call CD_Global_Add_Time_Ms with negative ms. */
	while(time->tv_nsec < 0)
	{
		time->tv_sec -= 1;
		time->tv_nsec += CCD_GLOBAL_ONE_SECOND_NS;
	}
	while(time->tv_nsec > CCD_GLOBAL_ONE_SECOND_NS)
	{
		time->tv_sec += 1;
		time->tv_nsec -= CCD_GLOBAL_ONE_SECOND_NS;
	}
}

/**
 * Routine to subtract a number of milliseconds to the specified timespec.
 * @param time The address of a timespec to subtract the milliseconds from.
 * @param ms The number of milliseconds to subtract.
 * @see #CCD_GLOBAL_ONE_SECOND_NS
 * @see #CCD_GLOBAL_ONE_MILLISECOND_NS
 */
void CCD_Global_Subtract_Time_Ms(struct timespec *time,int ms)
{
	time->tv_nsec -= ms*CCD_GLOBAL_ONE_MILLISECOND_NS;
	while(time->tv_nsec < 0)
	{
		time->tv_sec -= 1;
		time->tv_nsec += CCD_GLOBAL_ONE_SECOND_NS;
	}
	/* This shouldn't be necesary, unless someone is mad enough to call CCD_Global_Subtract_Time_Ms 
	** with negative ms. */
	while(time->tv_nsec > CCD_GLOBAL_ONE_SECOND_NS)
	{
		time->tv_sec += 1;
		time->tv_nsec -= CCD_GLOBAL_ONE_SECOND_NS;
	}
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.1  2011/11/23 10:59:52  cjm
** Initial revision
**
** Revision 0.14  2009/02/05 11:40:27  cjm
** Swapped Bitwise for Absolute logging levels.
**
** Revision 0.13  2008/12/11 16:42:50  cjm
** Changed CCD_Global_Log_Format to not use Global_Buff.
**
** Revision 0.12  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 0.11  2006/05/16 14:14:04  cjm
** gnuify: Added GNU General Public License.
**
** Revision 0.10  2002/12/16 16:49:36  cjm
** Removed Error routines resetting error number to zero.
**
** Revision 0.9  2002/11/07 19:13:39  cjm
** Changes to make library work with SDSU version 1.7 DSP code.
**
** Revision 0.8  2001/06/04 14:41:05  cjm
** Added conditionally compiled process priority change routines, and (readout)
** memory locking routines.
**
** Revision 0.7  2001/04/17 09:37:55  cjm
** Added logging code.
**
** Revision 0.6  2000/12/19 16:23:56  cjm
** Added filter wheel module calls.
**
** Revision 0.5  2000/06/13 17:14:13  cjm
** Changes to make Ccs agree with voodoo.
**
** Revision 0.4  2000/04/13 12:57:55  cjm
** Added CCD_Global_Get_Current_Time_String.
**
** Revision 0.3  2000/03/08 18:27:11  cjm
** CCD_DSP_Initialise now returns success.
**
** Revision 0.2  2000/03/01 15:44:41  cjm
** Backup.
**
** Revision 0.1  2000/01/25 14:57:27  cjm
** initial revision (PCI version).
**
*/
