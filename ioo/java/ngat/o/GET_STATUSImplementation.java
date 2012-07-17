// GET_STATUSImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/GET_STATUSImplementation.java,v 1.2 2012-07-17 17:17:45 cjm Exp $
package ngat.o;

import java.lang.*;
import java.util.Hashtable;

import ngat.o.ccd.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.ISS_TO_INST;
import ngat.message.ISS_INST.GET_STATUS;
import ngat.message.ISS_INST.GET_STATUS_DONE;
import ngat.util.ExecuteCommand;

/**
 * This class provides the implementation for the GET_STATUS command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.2 $
 */
public class GET_STATUSImplementation extends INTERRUPTImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: GET_STATUSImplementation.java,v 1.2 2012-07-17 17:17:45 cjm Exp $");
	/**
	 * Local copy of the O status object.
	 * @see O#getStatus
	 * @see OStatus
	 */
	private OStatus status = null;
	/**
	 * This hashtable is created in processCommand, and filled with status data,
	 * and is returned in the GET_STATUS_DONE object.
	 */
	private Hashtable hashTable = null;

	/**
	 * Standard status string passed back in the hashTable, describing the detector temperature status health,
	 * using the standard keyword KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS. 
	 * Initialised to VALUE_STATUS_UNKNOWN.
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_UNKNOWN
	 */
	private String detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_UNKNOWN;

	/**
	 * Constructor. 
	 */
	public GET_STATUSImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.GET_STATUS&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.GET_STATUS";
	}

	/**
	 * This method gets the GET_STATUS command's acknowledge time. 
	 * This takes the default acknowledge time to implement.
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set.
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OTCPServerConnectionThread#getDefaultAcknowledgeTime
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method implements the GET_STATUS command. 
	 * The local hashTable is setup (returned in the done object) and a local copy of status setup.
	 * The current mode of the camera is returned by calling getCurrentMode.
	 * The following data is put into the hashTable:
	 * <ul>
	 * <li><b>currentCommand</b> The current command from the status object, or blank if no current command.
	 * <li><b>Instrument</b> The name of this instrument, retrieved from the property 
	 * 	<i>o.get_status.instrument_name</i>.
	 * <li><b>NRows, NCols</b> Number of rows and columns setup on the CCD(from libo_ccd, not the camera hardware).
	 * <li><b>NSBin, NPBin</b> Binning factor for rows and columns setup on the CCD 
	 * 	(from libo_ccd, not the camera hardware).
	 * <li><b>DeInterlace Type</b> The de-interlace type, which tells us how many readouts we are using 
	 * 	(from libo_ccd, not the camera hardware).
	 * <li><b>Window Flags</b> The window flags, which tell us which windows are in effect
	 * 	(from libo_ccd, not the camera hardware).
	 * <li><b>Setup Status</b> Whether the camera has been setup sufficiently for exposures to be taken
	 * 	(from libo_ccd, not the camera hardware).
	 * <li><b>Exposure Start Time, Exposure Length</b> The exposure start time, and the length of the current
	 * 	(or last) exposure.
	 * <li><b>Exposure Count, Exposure Number</b> How many exposures the current command has taken and how many
	 * 	it will do in total (from the status object).
	 * </ul>
	 * The currently selected filter status is added to the hashTable (using getFilterWheelStatus).
	 * If the command requests a <b>INTERMEDIATE</b> level status, getIntermediateStatus is called.
	 * If the command requests a <b>FULL</b> level status, getFullStatus is called.
	 * An object of class GET_STATUS_DONE is returned, with the information retrieved.
	 * @see #status
	 * @see #hashTable
	 * @see #detectorTemperatureInstrumentStatus
	 * @see #getCurrentMode
	 * @see #getFilterWheelStatus
	 * @see #getIntermediateStatus
	 * @see #getFullStatus
	 * @see OStatus#getCurrentCommand
	 * @see CCDLibrary#getExposureStatus
	 * @see CCDLibrary#getExposureLength
	 * @see CCDLibrary#getExposureStartTime
	 * @see CCDLibrary#getBinnedNCols
	 * @see CCDLibrary#getBinnedNRows
	 * @see CCDLibrary#getXBin
	 * @see CCDLibrary#getYBin
	 * @see CCDLibrary#getDeInterlaceType
	 * @see CCDLibrary#getSetupWindowFlags
	 * @see CCDLibrary#getSetupComplete
	 * @see OStatus#getExposureCount
	 * @see OStatus#getExposureNumber
	 * @see OStatus#getProperty
	 * @see OStatus#getPropertyInteger
	 * @see OStatus#getPropertyBoolean
	 * @see ngat.message.ISS_INST.GET_STATUS#getLevel
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#KEYWORD_INSTRUMENT_STATUS
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		GET_STATUS getStatusCommand = (GET_STATUS)command;
		GET_STATUS_DONE getStatusDone = new GET_STATUS_DONE(command.getId());
		ISS_TO_INST currentCommand = null;
		int currentMode;

	 // Create new hashtable to be returned
		hashTable = new Hashtable();
	// get local reference to OStatus object.
		status = o.getStatus();
	// current mode
		currentMode = getCurrentMode();
		getStatusDone.setCurrentMode(currentMode);
	// What instrument is this?
		hashTable.put("Instrument",status.getProperty("o.get_status.instrument_name"));
	// Initialise Standard status to UNKNOWN
		detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_UNKNOWN;
		hashTable.put(GET_STATUS_DONE.KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS,
			      detectorTemperatureInstrumentStatus);
		hashTable.put(GET_STATUS_DONE.KEYWORD_INSTRUMENT_STATUS,GET_STATUS_DONE.VALUE_STATUS_UNKNOWN);
	// current command
		currentCommand = status.getCurrentCommand();
		if(currentCommand == null)
			hashTable.put("currentCommand","");
		else
			hashTable.put("currentCommand",currentCommand.getClass().getName());
	// Currently, we query ccd setup stored settings, not hardware.
		hashTable.put("NCols",new Integer(ccd.getBinnedNCols()));
		hashTable.put("NRows",new Integer(ccd.getBinnedNRows()));
		hashTable.put("NSBin",new Integer(ccd.getXBin()));
		hashTable.put("NPBin",new Integer(ccd.getYBin()));
		hashTable.put("DeInterlace Type",new Integer(ccd.getDeInterlaceType()));
		hashTable.put("Window Flags",new Integer(ccd.getSetupWindowFlags()));
		hashTable.put("Setup Status",new Boolean(ccd.getSetupComplete()));
		hashTable.put("Exposure Length",new Integer(ccd.getExposureLength()));
		hashTable.put("Exposure Start Time",new Long(ccd.getExposureStartTime()));
	// filter wheel settings
		getFilterWheelStatus();
		hashTable.put("Exposure Count",new Integer(status.getExposureCount()));
		hashTable.put("Exposure Number",new Integer(status.getExposureNumber()));
	// intermediate level information - basic plus controller calls.
		if(getStatusCommand.getLevel() >= GET_STATUS.LEVEL_INTERMEDIATE)
		{
			getIntermediateStatus();
		}// end if intermediate level status
	// Get full status information.
		if(getStatusCommand.getLevel() >= GET_STATUS.LEVEL_FULL)
		{
			getFullStatus();
		}
	// set hashtable and return values.
		getStatusDone.setDisplayInfo(hashTable);
		getStatusDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		getStatusDone.setErrorString("");
		getStatusDone.setSuccessful(true);
	// return done object.
		return getStatusDone;
	}

	/**
	 * Internal method to get the current mode, the GET_STATUS command will return.
	 * @see #ccd
	 * @see ngat.o.ccd.CCDLibrary#getExposureStatus
	 */
	private int getCurrentMode()
	{
		int currentMode;

		currentMode = GET_STATUS_DONE.MODE_IDLE;
		switch(ccd.getExposureStatus())
		{
			case CCDLibrary.EXPOSURE_STATUS_NONE:
				if(ccd.getSetupInProgress())
					currentMode = GET_STATUS_DONE.MODE_CONFIGURING;
				//if(ccd.getFilterWheelStatus() != CCDLibrary.CCD_FILTER_WHEEL_STATUS_NONE)
				//	currentMode = GET_STATUS_DONE.MODE_CONFIGURING;
				break;
			case CCDLibrary.EXPOSURE_STATUS_CLEAR:
				currentMode =  GET_STATUS_DONE.MODE_CLEARING;
				break;
			case CCDLibrary.EXPOSURE_STATUS_WAIT_START:
				currentMode =  GET_STATUS_DONE.MODE_WAITING_TO_START;
				break;
			case CCDLibrary.EXPOSURE_STATUS_EXPOSE:
				currentMode = GET_STATUS_DONE.MODE_EXPOSING;
				break;
			case CCDLibrary.EXPOSURE_STATUS_PRE_READOUT:
				currentMode = GET_STATUS_DONE.MODE_PRE_READOUT;
				break;
			case CCDLibrary.EXPOSURE_STATUS_READOUT:
				currentMode = GET_STATUS_DONE.MODE_READING_OUT;
				break;
			case CCDLibrary.EXPOSURE_STATUS_POST_READOUT:
				currentMode = GET_STATUS_DONE.MODE_POST_READOUT;
				break;
			default:
				currentMode = GET_STATUS_DONE.MODE_ERROR;
				break;
		}
		return currentMode;
	}

	/**
	 * Internal method to get some filter wheel status. The status is put into the hashTable.
	 * The following data is put into the hashTable:
	 * <ul>
	 * <li><b>Filter Wheel:&lt;wheel number&gt;</b>The name of the filter in this wheel currently in use.
	 * <li><b>Filter Wheel Id:&lt;wheel number&gt;</b>The ID of the filter in this wheel currently in use.
	 * <li><b>Filter Wheel Status</b> The status of the filter wheel mechanism, as returned by 
	 * 	filterWheelGetStatus.
	 * </ul>
	 * @see #ccd
	 * @see #status
	 * @see #hashTable
	 * @see CCDLibrary#filterWheelGetStatus
	 * @see CCDLibrary#filterWheelGetPosition
	 * @see OStatus#getFilterTypeName
	 */
	private void getFilterWheelStatus()
	{
		int filterWheelCount,filterWheelPosition,filterWheelStatus;
		String s = null;
		String s1 = null;

		filterWheelPosition = -1;
		try
		{
			filterWheelPosition = ccd.filterWheelGetPosition();
			if(filterWheelPosition == -1) // indeterminate position
			{
				s = "None";
				s1 = "Unknown";
			}
			else
			{
				s = status.getFilterTypeName(filterWheelPosition);
				s1 = status.getFilterIdName(s);
			}
		}
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":processCommand:get Filter Wheel Position:.",e);
			s = null;
		}
		catch(IllegalArgumentException e)
		{
			o.error(this.getClass().getName()+":processCommand:get Filter Wheel Position::"+
				filterWheelPosition+".",e);
			s = null;
			s1 = null;
		}
		hashTable.put("Filter Wheel:0",s);
		hashTable.put("Filter Wheel Id:0",s1);
		filterWheelStatus = ccd.filterWheelGetStatus();
		hashTable.put("Filter Wheel Status",new Integer(filterWheelStatus));
		hashTable.put("Filter Wheel Status String",
			      new String(ccd.filterWheelStatusToString(filterWheelStatus)));
	}

	/**
	 * Routine to get status, when level INTERMEDIATE has been selected.
	 * Intermediate level status is usually useful data which can only be retrieved by querying the
	 * SDSU controller directly. 
	 * The following data is put into the hashTable:
	 * <ul>
	 * <li><b>Elapsed Exposure Time</b> The Elapsed Exposure Time, this is read from the controller.
	 * </ul>
	 * If the <i>o.get_status.temperature</i> boolean property is TRUE, 
	 * the following data is put into the hashTable:
	 * <ul>
	 * <li><b>Temperature</b> The current CCD (dewar) temperature, this is read from the controller.
	 *       <i>setDetectorTemperatureInstrumentStatus</i> is then called to set the hashtable entry 
	 *       KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS and detectorTemperatureInstrumentStatus.
	 * <li><b>Heater ADU</b> The current Heater ADU count, this is read from the controller.
	 * <li><b>Heater Power</b> This is the power in Watts put into the heater, converted from 
	 *        the Heater ADU just read.
	 * <li><b>Utility Board Temperature ADU</b> The Utility Board ADU count, 
	 * 	this is read from the utility board temperature sensor.
	 * </ul>
	 * If the <i>o.get_status.supply_voltages</i> boolean property is TRUE, 
	 * the following data is put into the hashTable:
	 * <ul>
	 * <li><b>High Voltage Supply ADU</b> The SDSU High Voltage supply ADU count, 
	 * 	this is read from the utility board.
	 * <li><b>Low Voltage Supply ADU</b> The SDSU Low Voltage supply ADU count, 
	 * 	this is read from the utility board.
	 * <li><b>Minus Low Voltage Supply ADU</b> The SDSU Negative Voltage supply ADU count, 
	 * 	this is read from the utility board.
	 * </ul>
	 * Finally, <i>setInstrumentStatus</i> is called to set the hashTable's overall instrument status,
	 * in the KEYWORD_INSTRUMENT_STATUS.
	 * @see #ccd
	 * @see #status
	 * @see #hashTable
	 * @see #setDetectorTemperatureInstrumentStatus
	 * @see #setInstrumentStatus
	 * @see #CENTIGRADE_TO_KELVIN
	 * @see CCDLibrary#getElapsedExposureTime
	 * @see CCDLibrary#getHighVoltageAnalogueADU
	 * @see CCDLibrary#getLowVoltageAnalogueADU
	 * @see CCDLibrary#getMinusLowVoltageAnalogueADU
	 * @see CCDLibrary#getTemperature
	 * @see CCDLibrary#getTemperatureHeaterADU
	 * @see CCDLibrary#getTemperatureHeaterPower
	 * @see CCDLibrary#getTemperatureUtilityBoardADU
	 * @see OStatus#getPropertyBoolean
	 */
	private void getIntermediateStatus()
	{
		int elapsedExposureTime,adu;
		double ccdTemperature,dvalue;

		// These setting are queried directly from the controller.
		// libo_ccd routines that do DSP code may cause problems here as the instrument
		// may be in the process of reading out or similar.
		// With mutex support in libo_ccd around controller commands this should work.
		// elapsed exposure time - this seems to work when an exposure is in progress.
		try
		{
			elapsedExposureTime = ccd.getElapsedExposureTime();
		}
		catch(CCDLibraryNativeException e)
		{
			// Don't report the error, if it's just we are reading out at the moment
			if(e.getDSPErrorNumber() != 17)
			{
				o.error(this.getClass().getName()+
					  ":processCommand:Get Elapsed Exposure Time failed.",e);
			}
			elapsedExposureTime = 0;
		}
		// Always add the exposure time, if we are reading out it has been set to 0
		hashTable.put("Elapsed Exposure Time",new Integer(elapsedExposureTime));
		if(status.getPropertyBoolean("o.get_status.temperature"))
		{
			// CCD temperature
			// This involves a read of the utility board, which will fail when exposing...
			// Therefore only put the temperature in the hashtable on success.
			// Return temperature in degrees kelvin.
			try
			{
				ccdTemperature = ccd.getTemperature();
				hashTable.put("Temperature",new Double(ccdTemperature+CENTIGRADE_TO_KELVIN));
				// set standard status value based on current temperature
				setDetectorTemperatureInstrumentStatus(ccdTemperature);
			}
			catch(CCDLibraryNativeException e)
			{
				// Don't report the error, if it's just we are reading out at the moment
				if(e.getDSPErrorNumber() != 64)
				{
					o.error(this.getClass().getName()+
						  ":processCommand:Get Temperature failed.",e);
				}
			}// catch
			// Dewar heater ADU counts - how much we are heating the dewar to control the temperature.
		        // Utility Board ADU counts - how hot the temperature sensor is on the utility board.
			try
			{
				adu = ccd.getTemperatureHeaterADU();
				hashTable.put("Heater ADU",new Integer(adu));
				dvalue = ccd.getTemperatureHeaterPower(adu);
				hashTable.put("Heater Power",new Double(dvalue));
				adu = ccd.getTemperatureUtilityBoardADU();
				hashTable.put("Utility Board Temperature ADU",new Integer(adu));
			}
			catch(CCDLibraryNativeException e)
			{
				// Don't report the error, if it's just we are reading out at the moment
				if(e.getDSPErrorNumber() != 64)
				{
					o.error(this.getClass().getName()+
						  ":processCommand:Get ADU(s) failed.",e);
				}
			}// end catch
		}// end if get temperature status
		// SDSU supply voltages
		if(status.getPropertyBoolean("o.get_status.supply_voltages"))
		{
			try
			{
				adu = ccd.getHighVoltageAnalogueADU();
				hashTable.put("High Voltage Supply ADU",new Integer(adu));
				adu = ccd.getLowVoltageAnalogueADU();
				hashTable.put("Low Voltage Supply ADU",new Integer(adu));
				adu = ccd.getMinusLowVoltageAnalogueADU();
				hashTable.put("Minus Low Voltage Supply ADU",new Integer(adu));
			}
			catch(CCDLibraryNativeException e)
			{
				// Don't report the error, if it's just we are reading out at the moment
				if(e.getDSPErrorNumber() != 64)
				{
					o.error(this.getClass().getName()+
						  ":processCommand:Get supply voltage ADU failed.",e);
				}
			}// end catch
		}// end if get supply voltage status
	// Standard status
		setInstrumentStatus();
	}

	/**
	 * Set the standard entry for detector temperature in the hashtable based upon the current temperature.
	 * Reads the folowing config:
	 * <ul>
	 * <li>o.get_status.detector.temperature.warm.warn
	 * <li>o.get_status.detector.temperature.warm.fail
	 * <li>o.get_status.detector.temperature.cold.warn
	 * <li>o.get_status.detector.temperature.cold.fail
	 * </ul>
	 * The config values should be in degrees C.
	 * @param currentTemperature The current temperature in degrees C.
	 * @exception NumberFormatException Thrown if the config is not a valid double.
	 * @see #hashTable
	 * @see #status
	 * @see #detectorTemperatureInstrumentStatus
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_OK
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_WARN
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_FAIL
	 */
	protected void setDetectorTemperatureInstrumentStatus(double currentTemperature) throws NumberFormatException
	{
		double warmWarnTemperature,warmFailTemperature,coldWarnTemperature,coldFailTemperature;

		// get config for warn and fail temperatures
		warmWarnTemperature = status.getPropertyDouble("o.get_status.detector.temperature.warm.warn");
		warmFailTemperature = status.getPropertyDouble("o.get_status.detector.temperature.warm.fail");
		coldWarnTemperature = status.getPropertyDouble("o.get_status.detector.temperature.cold.warn");
		coldFailTemperature = status.getPropertyDouble("o.get_status.detector.temperature.cold.fail");
		// set status
		if(currentTemperature > warmFailTemperature)
			detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_FAIL;
		else if(currentTemperature > warmWarnTemperature)
			detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_WARN;
		else if(currentTemperature < coldFailTemperature)
			detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_FAIL;
		else if(currentTemperature < coldWarnTemperature)
			detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_WARN;
		else
			detectorTemperatureInstrumentStatus = GET_STATUS_DONE.VALUE_STATUS_OK;
		// set hashtable entry
		hashTable.put(GET_STATUS_DONE.KEYWORD_DETECTOR_TEMPERATURE_INSTRUMENT_STATUS,
			      detectorTemperatureInstrumentStatus);
	}

	/**
	 * Set the overall instrument status keyword in the hashtable. This is derived from sub-system keyword values,
	 * currently only the detector temperature. HashTable entry KEYWORD_INSTRUMENT_STATUS)
	 * should be set to the worst of OK/WARN/FAIL. If sub-systems are UNKNOWN, OK is returned.
	 * @see #hashTable
	 * @see #status
	 * @see #detectorTemperatureInstrumentStatus
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#KEYWORD_INSTRUMENT_STATUS
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_OK
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_WARN
	 * @see ngat.message.ISS_INST.GET_STATUS_DONE#VALUE_STATUS_FAIL
	 */
	protected void setInstrumentStatus()
	{
		String instrumentStatus;

		// default to OK
		instrumentStatus = GET_STATUS_DONE.VALUE_STATUS_OK;
		// if a sub-status is in warning, overall status is in warning
		if(detectorTemperatureInstrumentStatus.equals(GET_STATUS_DONE.VALUE_STATUS_WARN))
			instrumentStatus = GET_STATUS_DONE.VALUE_STATUS_WARN;
		// if a sub-status is in fail, overall status is in fail. This overrides a previous warn
	        if(detectorTemperatureInstrumentStatus.equals(GET_STATUS_DONE.VALUE_STATUS_FAIL))
			instrumentStatus = GET_STATUS_DONE.VALUE_STATUS_FAIL;
		// set standard status in hashtable
		hashTable.put(GET_STATUS_DONE.KEYWORD_INSTRUMENT_STATUS,instrumentStatus);
	}

	/**
	 * Method to get misc status, when level FULL has been selected.
	 * The following data is put into the hashTable:
	 * <ul>
	 * <li><b>Log Level</b> The current log level used by O.
	 * <li><b>Disk Usage</b> The results of running a &quot;df -k&quot;, to get the disk usage.
	 * <li><b>Process List</b> The results of running a &quot;ps -e -o pid,pcpu,vsz,ruser,stime,time,args&quot;, 
	 * 	to get the processes running on this machine.
	 * <li><b>Uptime</b> The results of running a &quot;uptime&quot;, 
	 * 	to get system load and time since last reboot.
	 * <li><b>Total Memory, Free Memory</b> The total and free memory in the Java virtual machine.
	 * <li><b>java.version, java.vendor, java.home, java.vm.version, java.vm.vendor, java.class.path</b> 
	 * 	Java virtual machine version, classpath and type.
	 * <li><b>os.name, os.arch, os.version</b> The operating system type/version.
	 * <li><b>user.name, user.home, user.dir</b> Data about the user the process is running as.
	 * <li><b>thread.list</b> A list of threads the O process is running.
	 * </ul>
	 * @see #serverConnectionThread
	 * @see #hashTable
	 * @see ExecuteCommand#run
	 * @see OStatus#getLogLevel
	 */
	private void getFullStatus()
	{
		ExecuteCommand executeCommand = null;
		Runtime runtime = null;
		StringBuffer sb = null;
		Thread threadList[] = null;
		int threadCount;

		// log level
		hashTable.put("Log Level",new Integer(status.getLogLevel()));
		// execute 'df -k' on instrument computer
		executeCommand = new ExecuteCommand("df -k");
		executeCommand.run();
		if(executeCommand.getException() == null)
			hashTable.put("Disk Usage",new String(executeCommand.getOutputString()));
		else
			hashTable.put("Disk Usage",new String(executeCommand.getException().toString()));
		// execute "ps -e -o pid,pcpu,vsz,ruser,stime,time,args" on instrument computer
		executeCommand = new ExecuteCommand("ps -e -o pid,pcpu,vsz,ruser,stime,time,args");
		executeCommand.run();
		if(executeCommand.getException() == null)
			hashTable.put("Process List",new String(executeCommand.getOutputString()));
		else
			hashTable.put("Process List",new String(executeCommand.getException().toString()));
		// execute "uptime" on instrument computer
		executeCommand = new ExecuteCommand("uptime");
		executeCommand.run();
		if(executeCommand.getException() == null)
			hashTable.put("Uptime",new String(executeCommand.getOutputString()));
		else
			hashTable.put("Uptime",new String(executeCommand.getException().toString()));
		// get vm memory situation
		runtime = Runtime.getRuntime();
		hashTable.put("Free Memory",new Long(runtime.freeMemory()));
		hashTable.put("Total Memory",new Long(runtime.totalMemory()));
		// get some java vm information
		hashTable.put("java.version",new String(System.getProperty("java.version")));
		hashTable.put("java.vendor",new String(System.getProperty("java.vendor")));
		hashTable.put("java.home",new String(System.getProperty("java.home")));
		hashTable.put("java.vm.version",new String(System.getProperty("java.vm.version")));
		hashTable.put("java.vm.vendor",new String(System.getProperty("java.vm.vendor")));
		hashTable.put("java.class.path",new String(System.getProperty("java.class.path")));
		hashTable.put("os.name",new String(System.getProperty("os.name")));
		hashTable.put("os.arch",new String(System.getProperty("os.arch")));
		hashTable.put("os.version",new String(System.getProperty("os.version")));
		hashTable.put("user.name",new String(System.getProperty("user.name")));
		hashTable.put("user.home",new String(System.getProperty("user.home")));
		hashTable.put("user.dir",new String(System.getProperty("user.dir")));
		// get a list of threads running in the vm
		threadCount = serverConnectionThread.activeCount();
		threadList = new Thread[threadCount];
		serverConnectionThread.enumerate(threadList);
		sb = new StringBuffer();
		for(int i = 0;i< threadCount;i++)
		{
			if(threadList[i] != null)
			{
				sb.append(threadList[i].getName());
				sb.append("\n");
			}
		}
		hashTable.put("thread.list",sb.toString());
	}
}

//
// $Log: not supported by cvs2svn $
// Revision 1.1  2011/11/23 10:55:24  cjm
// Initial revision
//
//
