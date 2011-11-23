/* ccd_temperature.c
** low level ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_temperature.c,v 1.1 2011-11-23 10:59:52 cjm Exp $
*/

/**
 * ccd_temperature holds the routines for calulating the current CCD temperature and setting the CCDs
 * temperature.
 * These routines are setup with use with a:
 * STMicroelectronics STTH5L06FP Turbo2 ultra fast high voltage rectifier 5A 600V (part RS 486-6163).
 * The CCD_Temperature_Get function calculates the temperature of the the ccd using the equation:
 * <code>V = (-0.0019 x Tc) + 0.4358</code>. The inverse equation is therefore:
 * <code>Tc = (V - 0.4358) / -0.0019</code>. These equations derived by IAS at temperatures between 7C and 72C.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 * @see #TEMPERATURE_DEFAULT_ZERO_VOLTAGE_OFFSET
 * @see #TEMPERATURE_DEFAULT_V_PER_DEG_C 
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
#include "log_udp.h"
#include "ccd_global.h"
#include "ccd_dsp.h"
#include "ccd_temperature.h"

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_temperature.c,v 1.1 2011-11-23 10:59:52 cjm Exp $";

/**
 * The default adu per volt used to calculate CCD temperature. From SDSU documentation.
 */
#define TEMPERATURE_DEFAULT_AIN5_ADU_PER_VOLT    (1366.98)
/**
 * The default adu offset used to calculate CCD temperature. From SDSU documentation.
 */
#define TEMPERATURE_DEFAULT_AIN5_ADU_OFFSET	 (2045)
/**
 * The voltage measured across the diode when the diode is at zero degrees Centigrade.
 * Calibrated by IAS.
 */
#define TEMPERATURE_DEFAULT_ZERO_VOLTAGE_OFFSET  (0.4358)
/**
 * Change in voltage measured in the diode for 1 degrees C of temperature change.
 * Voltage decreases as the temperature increases, as it is easier to excite electrons at warmer temperatures,
 * hence the negative sign. Calibrated by IAS.
 */
#define TEMPERATURE_DEFAULT_V_PER_DEG_C          (-0.0019)
/**
 * The default adu per volt used to calculate heater power.
 * Calibrated CJM 22/9/11.
 */
#define TEMPERATURE_DEFAULT_DAC0_ADU_PER_VOLT    (-409.076)
/**
 * The default adu offset used to calculate heater power.
 * Calibrated CJM 22/9/11.
 */
#define TEMPERATURE_DEFAULT_DAC0_ADU_OFFSET	 (0)
/**
 * The resistance of the heater resistor in the dewar, in Ohms.
 */
#define TEMPERATURE_HEATER_RESISTANCE            (25.0)

/**
 * The number of times to read the SDSU CCD Controller to determine the temperature.
 * @see #CCD_Temperature_Get
 */
#define TEMPERATURE_MAX_CHECKS  		 (30)

/**
 * Definition of the memory address where the Temperature regulation ADU counts are held,
 * in utility board Y memory space. This is the same value as is in the DSP code (DAC0).
 * @see #CCD_Temperature_Get_Heater_ADU
 */
#define TEMPERATURE_HEATER_ADDRESS		 (0x2)
/**
 * This is the address on the SDSU utility board, Y memory space, to read the current ADU count of the thermistor
 * mounted on the utility board. 
 * This ADU count is read from the temperature monitoring device.
 */
#define TEMPERATURE_UTILITY_BOARD_ADU_ADDRESS	 (0x7)
/**
 * This is the address on the SDSU utility board, Y memory space, to read the current ADU count of the thermistor
 * mounted in the dewar. This is connected by AIN5 via user interface connector C32.
 * This ADU count is read from the temperature monitoring device.
 */
#define TEMPERATURE_CURRENT_ADU_ADDRESS		(0xc)
/**
 * This is the address on the SDSU utility board to write the required ADU count. This ADU count
 * is derived from a temperature, and allows the utility board to control the CCD temperature.
 */
#define TEMPERATURE_REQUIRED_ADU_ADDRESS	(0x1c)
/**
 * How long we sleep for, in milliseconds, between successive calls to read
 * the current temperature ADU count from the utility board. The ADU count is
 * only calculated every 3ms.
 */
#define TEMPERATURE_GET_SLEEP_MS	         (2)

/* data types */
/**
 * Data type holding local data to ccd_temperature. This data is all the numerical constants neccessary to
 * calculate temperatures for a particular temperature sensor connected to the system.
 * <dl>
 * <dt>AIN5_Adu_Per_Volt</dt> <dd>ADU difference representing a difference of 1V for AIN5 
 *     (dewar tmperature diode).</dd>
 * <dt>AIN5_Adu_Offset</dt> <dd>ADUs representing 0V for AIN5 (dewar tmperature diode).</dd>
 * <dt>Zero_Voltage_Offset</dt> <dd>Voltage returned by diode at 0 degrees centigrade.</dd>
 * <dt>V_Per_Deg_C</dt> <dd>Change in voltage representing a change in temperature of 1 degrees C.</dd>
 * <dt>DAC0_Adu_Per_Volt</dt> <dd>ADU difference representing a difference of 1V for DAC0 (dewar heater).</dd>
 * <dt>DAC0_Adu_Offset</dt> <dd>ADUs representing 0V for DAC0 (dewar heater).</dd>
 * </dl>
 */
typedef struct Temperature_Struct
{
	float	AIN5_Adu_Per_Volt;
	int	AIN5_Adu_Offset;
	double  Zero_Voltage_Offset;
	double  V_Per_Deg_C;
	float	DAC0_Adu_Per_Volt;
	int	DAC0_Adu_Offset;
} Temperature_Struct_T;


/* external variables */

/* internal variables */
/**
 * Variable holding error code of last operation performed by ccd_temperature.
 */
static int Temperature_Error_Number = 0;
/**
 * Local variable holding description of the last error that occured.
 */
static char Temperature_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";
/**
 * Data holding the current diode configuration values for a particular temperature sensor.
 * @see #Temperature_Struct_T
 * @see #TEMPERATURE_DEFAULT_AIN5_ADU_PER_VOLT
 * @see #TEMPERATURE_DEFAULT_AIN5_ADU_OFFSET,
 * @see #TEMPERATURE_DEFAULT_ZERO_VOLTAGE_OFFSET
 * @see #TEMPERATURE_DEFAULT_V_PER_DEG_C
 * @see #TEMPERATURE_DEFAULT_DAC0_ADU_PER_VOLT
 * @see #TEMPERATURE_DEFAULT_DAC0_ADU_OFFSET,
 */
static Temperature_Struct_T Temperature_Data = 
{
	TEMPERATURE_DEFAULT_AIN5_ADU_PER_VOLT,TEMPERATURE_DEFAULT_AIN5_ADU_OFFSET,
	TEMPERATURE_DEFAULT_ZERO_VOLTAGE_OFFSET,TEMPERATURE_DEFAULT_V_PER_DEG_C,
	TEMPERATURE_DEFAULT_DAC0_ADU_PER_VOLT,TEMPERATURE_DEFAULT_DAC0_ADU_OFFSET
};

/* internal function definitions */
static int Temperature_ADU_To_Centigrade(float adu,double *temperature,double *voltage);
static int Temperature_Centigrade_To_ADU(double target_temperature,int *adu);

/* external functions */
/**
 * This routine gets the current temperature of the CCD in the dewar using the SDSU CCD Controller utility board.
 * It reads the utility board using CCD_DSP_Command_RDM to read memory which has the digital counts 
 * of the voltage from the temperature sensor in it. This is done TEMPERATURE_MAX_CHECKS times.
 * The temperature is calculated from the adu by calling Temperature_ADU_To_Centigrade. 
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param temperature The address of a variable to hold the calculated temperature to be returned.
 * 	The returned temperature is in degrees centigrade.
 * @return TRUE if the operation was successfull and the temperature returned was sensible, FALSE
 * 	if a failure occured or the temperature returned was not sensible.
 * @see #TEMPERATURE_MAX_CHECKS
 * @see #TEMPERATURE_CURRENT_ADU_ADDRESS
 * @see #TEMPERATURE_GET_SLEEP_MS
 * @see #Temperature_ADU_To_Centigrade
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Temperature_Get(CCD_Interface_Handle_T* handle,double *temperature)
{
	struct timespec sleep_time;
	int adu,retval;
	int i;
	double voltage;

	Temperature_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get() started.");
#endif
	if(temperature == NULL)
	{
		Temperature_Error_Number = 7;
		sprintf(Temperature_Error_String,"CCD_Temperature_Get:temperature pointer was NULL.");
		return FALSE;
	}
	adu = 0;
	for (i = 0; i < TEMPERATURE_MAX_CHECKS; i++)
	{
		retval = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
					     TEMPERATURE_CURRENT_ADU_ADDRESS);
		if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
		{
			Temperature_Error_Number = 3;
			sprintf(Temperature_Error_String,"CCD_Temperature_Get:Read temperature (%d) failed",i);
			return FALSE;
		}
#if LOGGING > 9
		CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get():RDM returned %d.",retval);
#endif
	/* ensure returned adu in range */
		retval = retval & 0xFFF;
		adu += retval;
	/* Sleep for 2 milliseconds. The controller only updates the temperature adu value every 3 ms. */
		sleep_time.tv_sec = 0;
		sleep_time.tv_nsec = TEMPERATURE_GET_SLEEP_MS*CCD_GLOBAL_ONE_MILLISECOND_NS;
		nanosleep(&sleep_time,NULL);
	}

#if LOGGING > 9
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get():%d RDMs returned %d.",
			TEMPERATURE_MAX_CHECKS,adu);
#endif
	/* Average the adu counts */
	adu = adu / TEMPERATURE_MAX_CHECKS;
#if LOGGING > 9
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get():Average adu:%d.",adu);
#endif
	/* Calculate the temperature */
	retval = Temperature_ADU_To_Centigrade((float)adu,temperature,&voltage);
#if LOGGING > 5
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get():Temperature:%.2f.",(*temperature));
#endif
#if LOGGING > 5
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get():Voltage:%.6f v.",voltage);
#endif
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get() finished.");
#endif
	return TRUE;
}

/**
 * This routine gets the current ADU of the utility board temperature sensor.
 * It reads the utility board using CCD_DSP_Command_RDM to read memory which has the digital counts 
 * of the voltage from the utility board temperature sensor in it.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param adu The address of a variable to hold the Analogue Digital Units to be returned.
 * @return TRUE if the operation was successfull, FALSE if a failure occured.
 * @see #TEMPERATURE_UTILITY_BOARD_ADU_ADDRESS
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Temperature_Get_Utility_Board_ADU(CCD_Interface_Handle_T* handle,int *adu)
{
	int retval;

	Temperature_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get_Utility_Board() started.");
#endif
	if(adu == NULL)
	{
		Temperature_Error_Number = 8;
		sprintf(Temperature_Error_String,"CCD_Temperature_Get:adu pointer was NULL.");
		return FALSE;
	}
	retval = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
				     TEMPERATURE_UTILITY_BOARD_ADU_ADDRESS);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		Temperature_Error_Number = 6;
		sprintf(Temperature_Error_String,"CCD_Temperature_Get_Utility_Board:"
			"Read temperature failed");
		return FALSE;
	}
	(*adu) = retval;
#if LOGGING > 9
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get_Utility_Board():returned %d.",
			      (*adu));
#endif
	return TRUE;
}

/**
 * Routine to set the target temperature the SDSU CCD Controller will try to keep the CCD at during
 * operation of the camera. First <a href="#Temperature_Calc_Temp_ADU">Temperature_Calc_Temp_ADU</a> 
 * is called to get an ADU value for the
 * target_temperature and this is then written to the utility board using a 
 * write memory command using CCD_DSP_Command_WRM.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param target_temperature The temperature we want the CCD cooled to, in degrees centigrade.
 * @return TRUE if the target temperature was set, FALSE if an error occured.
 * @see #Temperature_Centigrade_To_ADU
 * @see #TEMPERATURE_REQUIRED_ADU_ADDRESS
 * @see ccd_dsp.html#CCD_DSP_Command_WRM
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Temperature_Set(CCD_Interface_Handle_T* handle,double target_temperature)
{
	int adu = 0;

	Temperature_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Set(temperature=%.2f) started.",
		target_temperature);
#endif
	/* get the target adu count from target_temperature using the setup data */
	if(!Temperature_Centigrade_To_ADU(target_temperature,&adu))
		return FALSE;
	/* write the target to memory */
	if(CCD_DSP_Command_WRM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,
			       TEMPERATURE_REQUIRED_ADU_ADDRESS,adu) != CCD_DSP_DON)
	{
		Temperature_Error_Number = 2;
		sprintf(Temperature_Error_String,"Setting CCD Temperature failed.");
		return FALSE;
	}
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Set() returned TRUE.");
#endif
	return TRUE;
}

/**
 * This routine gets the current ADU count of the voltage fed into the heater that regulates
 * the CCD temperature.
 * CCD_DSP_Command_RDM is used to read the relevant DSP memory location.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param heater_adu The address of an integer to hold the current heater ADU.
 * @return TRUE if the operation was successfull and the temperature returned was sensible, FALSE
 * 	if a failure occured or the temperature returned was not sensible.
 * @see ccd_dsp.html#CCD_DSP_Command_RDM
 * @see #TEMPERATURE_HEATER_ADDRESS
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_Temperature_Get_Heater_ADU(CCD_Interface_Handle_T* handle,int *heater_adu)
{
	int retval;

	Temperature_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get_Heater_ADU() started.");
#endif
	if(heater_adu == NULL)
	{
		Temperature_Error_Number = 4;
		sprintf(Temperature_Error_String,"CCD_Temperature_Get_Heater_ADU:adu was NULL.");
		return FALSE;
	}
	retval = CCD_DSP_Command_RDM(handle,CCD_DSP_UTIL_BOARD_ID,CCD_DSP_MEM_SPACE_Y,TEMPERATURE_HEATER_ADDRESS);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number() != 0))
	{
		Temperature_Error_Number = 5;
		sprintf(Temperature_Error_String,"CCD_Temperature_Get_Heater_ADU:Read memory failed.");
		return FALSE;
	}
	(*heater_adu) = retval;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Get_Heater_ADU() returned %#x.",
		(*heater_adu));
#endif
	return TRUE;
}

/**
 * Convert the heater ADU into the power being put into the dewar.
 * This is done using the equation: <code>P = V^2 x R</code>, where
 * <code>V = heater_adu - Temperature_Data.DAC0_Adu_Offset) / Temperature_Data.DAC0_Adu_Per_Volt</code>
 * @param heater_adu The heater ADU, retrieved from a previous call to CCD_Temperature_Get_Heater_ADU.
 * @param power The address of a double to store the returned power 
 * @see #CCD_Temperature_Get_Heater_ADU
 * @see #TEMPERATURE_HEATER_RESISTANCE
 */
int CCD_Temperature_Heater_ADU_To_Power(int heater_adu,double *power)
{
	double voltage;

	Temperature_Error_Number = 0;
#if LOGGING > 0
	CCD_Global_Log(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Heater_ADU_To_Power() started.");
#endif
	if(power == NULL)
	{
		Temperature_Error_Number = 11;
		sprintf(Temperature_Error_String,"CCD_Temperature_Heater_ADU_To_Power:power was NULL.");
		return FALSE;
	}
	voltage = (heater_adu - (float) Temperature_Data.DAC0_Adu_Offset) / Temperature_Data.DAC0_Adu_Per_Volt;
#if LOGGING > 5
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_Temperature_Heater_ADU_To_Power(): "
			      "heater_adu %d means voltage %.6f v.",heater_adu,(voltage));
#endif
	(*power) = (voltage*voltage)/TEMPERATURE_HEATER_RESISTANCE;
#if LOGGING > 0
	CCD_Global_Log_Format(LOG_VERBOSITY_VERBOSE,"CCD_Temperature_Heater_ADU_To_Power() returned %.6f Watts.",
		(*power));
#endif
	return TRUE;
}

/**
 * Get the current value of the error number.
 * @return The current value of the error number.
 */
int CCD_Temperature_Get_Error_Number(void)
{
	return Temperature_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_temperature in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Temperature_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Temperature_Error_Number == 0)
		sprintf(Temperature_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_Temperature:Error(%d) : %s\n",time_string,
		Temperature_Error_Number,Temperature_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_temperature in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_Temperature_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(Temperature_Error_Number == 0)
		sprintf(Temperature_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_Temperature:Error(%d) : %s\n",time_string,
		Temperature_Error_Number,Temperature_Error_String);
}

/* -----------------------------------------------------------------------------
** 	internal functions 
** ----------------------------------------------------------------------------- */
/**
 * Calculates the temperature from the adu. Converts the ADU into a voltage using adu_per_volt and adu_offset,
 * and then converts voltage to temperature in degrees C.
 * @param adu The adu we want the temperature for.
 * @param temperature The address of a double to store the temperature in degrees centigrade.
 * @param voltage The address of a double to store voltage represented by the adu. Debug only.
 * @return TRUE if the conversion succeeds, FALSE if it fails.
 * @see #Temperature_Data
 */
static int Temperature_ADU_To_Centigrade(float adu,double *temperature,double *voltage)
{
	if(temperature == NULL)
	{
		Temperature_Error_Number = 1;
		sprintf(Temperature_Error_String,"Temperature_ADU_To_Centigrade:temperature was NULL.");
		return FALSE;
	}
	if(voltage == NULL)
	{
		Temperature_Error_Number = 9;
		sprintf(Temperature_Error_String,"Temperature_ADU_To_Centigrade:voltage was NULL.");
		return FALSE;
	}
	(*voltage) = (adu - (float) Temperature_Data.AIN5_Adu_Offset) / Temperature_Data.AIN5_Adu_Per_Volt;
	/* using calibration provided by IAS */
	(*temperature) = ((*voltage)-Temperature_Data.Zero_Voltage_Offset) / Temperature_Data.V_Per_Deg_C;
	return TRUE;
}

/**
 * Determine an adu value for a given temperature. 
 * @param target_temperature The temperature we want the adu for, in degrees centigrade.
 * @param adu The address of an integer to returns the adu for the temperature.
 * @return Returns TRUE if sucessful and FALSE if there was an error.
 * @see #Temperature_Data
 */
static int Temperature_Centigrade_To_ADU(double target_temperature,int *adu)
{
	double voltage,dadu;
	int iadu;

	if(adu == NULL)
	{
		Temperature_Error_Number = 10;
		sprintf(Temperature_Error_String,"Temperature_Centigrade_To_ADU:adu was NULL.");
		return FALSE;
	}
#if LOGGING > 5
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"Temperature_Centigrade_To_ADU() started: "
			      "Finding ADU counts representing temperature %.2f C.",target_temperature);
#endif
	voltage = (target_temperature*Temperature_Data.V_Per_Deg_C)+Temperature_Data.Zero_Voltage_Offset;
#if LOGGING > 9
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"Temperature_Centigrade_To_ADU(): "
			      "Voltage representing temperature %.2f C is %.6f v.",target_temperature,voltage);
#endif
	dadu = (voltage*Temperature_Data.AIN5_Adu_Per_Volt)+((float)Temperature_Data.AIN5_Adu_Offset);
#if LOGGING > 5
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"Temperature_Centigrade_To_ADU(): "
			      "ADU counts representing temperature %.2f C is %f ADU.",target_temperature,dadu);
#endif
/* ensure adu's are in range. */
	iadu = (int)dadu;
	iadu = iadu & 0xfff;
/* return adu */
#if LOGGING > 9
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"Temperature_Centigrade_To_ADU(): "
			      "ADU counts representing temperature %.2f C is %d ADU.",target_temperature,iadu);
#endif
	(*adu) = iadu;
	return TRUE;
}

/*
** $Log: not supported by cvs2svn $
*/
