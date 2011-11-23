// DAY_CALIBRATEImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/DAY_CALIBRATEImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.io.*;
import java.lang.*;
import java.util.*;

import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.DAY_CALIBRATE;
import ngat.message.ISS_INST.DAY_CALIBRATE_ACK;
import ngat.message.ISS_INST.DAY_CALIBRATE_DP_ACK;
import ngat.message.ISS_INST.DAY_CALIBRATE_DONE;
import ngat.util.*;
import ngat.util.logging.*;

/**
 * This class provides the implementation of a DAY_CALIBRATE command sent to a server using the
 * Java Message System. It performs a series of BIAS and DARK frames from a configurable list,
 * taking into account frames done in previous invocations of this command (it saves it's state).
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class DAY_CALIBRATEImplementation extends CALIBRATEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: DAY_CALIBRATEImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");
	/**
	 * Initial part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_STRING = "o.day_calibrate.";
	/**
	 * Final part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_TYPE_STRING = ".type";
	/**
	 * Final part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_BIN_STRING = ".config.bin";
	/**
	 * Final part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_AMPLIFIER_STRING = ".config.window_amplifier";
	/**
	 * Final part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_FREQUENCY_STRING = ".frequency";
	/**
	 * Final part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_COUNT_STRING = ".count";
	/**
	 * Final part of a key string, used to create a list of potential day calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_EXPOSURE_TIME_STRING = ".exposure_time";
	/**
	 * Middle part of a key string, used for saving and restoring the stored calibration state.
	 */
	protected final static String LIST_KEY_LAST_TIME_STRING = "last_time.";
	/**
	 * The time, in milliseconds since the epoch, that the implementation of this command was started.
	 */
	private long implementationStartTime = 0L;
	/**
	 * The saved state of calibrations done over time by invocations of this command.
	 * @see DAY_CALIBRATESavedState
	 */
	private DAY_CALIBRATESavedState dayCalibrateState = null;
	/**
	 * The filename holding the saved state data.
	 */
	private String stateFilename = null;
	/**
	 * The list of calibrations to select from.
	 * Each item in the list is an instance of DAY_CALIBRATECalibration.
	 * @see DAY_CALIBRATECalibration
	 */
	protected List calibrationList = null;
	/**
	 * The readout overhead for a full frame, in milliseconds.
	 */
	private int readoutOverhead = 0;

	/**
	 * Constructor.
	 */
	public DAY_CALIBRATEImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.DAY_CALIBRATE&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.DAY_CALIBRATE";
	}

	/**
	 * This method is the first to be called in this class. 
	 * <ul>
	 * <li>It calls the superclass's init method.
	 * </ul>
	 * @param command The command to be implemented.
	 */
	public void init(COMMAND command)
	{
		super.init(command);
	}

	/**
	 * This method gets the unknown command's acknowledge time. This returns the server connection threads 
	 * default acknowledge time.
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set.
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OTCPServerConnectionThread#getMinAcknowledgeTime
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method implements the DAY_CALIBRATE command.
	 * <ul>
	 * <li>The implementation start time is saved.
	 * <li>loadCalibrationList is called to load a calibration list from the property file.
	 * <li>initialiseState is called to load the saved calibration database.
	 * <li>The readoutOverhead is retrieved from the configuration.
	 * <li>addSavedStateToCalibration is called, which finds the correct last time for each
	 * 	calibration in the list and sets the relevant field.
	 * <li>The FITS headers are cleared, and a the MULTRUN number is incremented.
	 * <li>For each calibration, we do the following:
	 *      <ul>
	 *      <li>testCalibration is called, to see whether the calibration should be done.
	 * 	<li>If it should, doCalibration is called to get the relevant frames.
	 *      </ul>
	 * <li>sendBasicAck is called, to stop the client timing out whilst creating the master bias.
	 * <li>The makeMasterBias method is called, to create master bias fields from the data just taken.
	 * </ul>
	 * Note this method assumes the loading and initialisation before the main loop takes less than the
	 * default acknowledge time, as no ACK's are sent to the client until we are ready to do the first
	 * sequence of calibration frames.
	 * @param command The command to be implemented.
	 * @return An instance of DAY_CALIBRATE_DONE is returned, with it's fields indicating
	 * 	the result of the command implementation.
	 * @see #implementationStartTime
	 * @see #loadCalibrationList
	 * @see #initialiseState
	 * @see #addSavedStateToCalibration
	 * @see FITSImplementation#clearFitsHeaders
	 * @see FITSImplementation#oFilename
	 * @see #testCalibration
	 * @see #doCalibration
	 * @see #readoutOverhead
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		DAY_CALIBRATE dayCalibrateCommand = (DAY_CALIBRATE)command;
		DAY_CALIBRATE_DONE dayCalibrateDone = new DAY_CALIBRATE_DONE(command.getId());
		DAY_CALIBRATECalibration calibration = null;
		String directoryString = null;
		int makeBiasAckTime;

		dayCalibrateDone.setMeanCounts(0.0f);
		dayCalibrateDone.setPeakCounts(0.0f);
	// initialise
		implementationStartTime = System.currentTimeMillis();
		if(loadCalibrationList(dayCalibrateCommand,dayCalibrateDone) == false)
			return dayCalibrateDone;
		if(initialiseState(dayCalibrateCommand,dayCalibrateDone) == false)
			return dayCalibrateDone;
	// Get the amount of time to readout and save a full frame
		try
		{
			readoutOverhead = status.getPropertyInteger("o.day_calibrate.readout_overhead");
		}
		catch (Exception e)
		{
			String errorString = new String(command.getId()+
				":processCommand:Failed to get readout overhead.");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2200);
			dayCalibrateDone.setErrorString(errorString);
			dayCalibrateDone.setSuccessful(false);
			return dayCalibrateDone;
		}
	// match saved state to calibration list (put last time into calibration list)
		if(addSavedStateToCalibration(dayCalibrateCommand,dayCalibrateDone) == false)
			return dayCalibrateDone;
	// initialise status/fits header info, in case any frames are produced.
	// get fits headers
		clearFitsHeaders();
	// get a filename to store frame in
		oFilename.nextMultRunNumber();
	// main loop, do calibrations until we run out of time.
		for(int i = 0; i < calibrationList.size(); i++)
		{
			calibration = (DAY_CALIBRATECalibration)(calibrationList.get(i));
		// see if we are going to do this calibration.
		// Note if we have run out of time (timeToComplete) then this method
		// should always return false.
			if(testCalibration(dayCalibrateCommand,dayCalibrateDone,calibration))
			{
				if(doCalibration(dayCalibrateCommand,dayCalibrateDone,calibration) == false)
					return dayCalibrateDone;
			}
		}// end for on calibration list
	// send an ack before make master processing, so the client doesn't time out.
		makeBiasAckTime = status.getPropertyInteger("o.day_calibrate.acknowledge_time.make_bias");
		if(sendBasicAck(dayCalibrateCommand,dayCalibrateDone,makeBiasAckTime) == false)
			return dayCalibrateDone;
	// get directory FITS files are in.
		directoryString = status.getProperty("o.file.fits.path");
		if(directoryString.endsWith(System.getProperty("file.separator")) == false)
			directoryString = directoryString.concat(System.getProperty("file.separator"));
	// Call pipeline to create master bias.
		// diddly TODO
		//if(makeMasterBias(dayCalibrateCommand,dayCalibrateDone,directoryString) == false)
		//	return dayCalibrateDone;
	// return done
		dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		dayCalibrateDone.setErrorString("");
		dayCalibrateDone.setSuccessful(true);
		return dayCalibrateDone;
	}

	/**
	 * Method to load a list of calibrations to do.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. If false is returned the error
	 * 	data in dayCalibrateDone is filled in.
	 * @see #calibrationList
	 * @see #LIST_KEY_STRING
	 * @see #LIST_KEY_TYPE_STRING
	 * @see #LIST_KEY_BIN_STRING
	 * @see #LIST_KEY_AMPLIFIER_STRING
	 * @see #LIST_KEY_FREQUENCY_STRING
	 * @see #LIST_KEY_COUNT_STRING
	 * @see #LIST_KEY_EXPOSURE_TIME_STRING
	 */
	protected boolean loadCalibrationList(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone)
	{
		DAY_CALIBRATECalibration calibration = null;
		String typeString = null;
		int index,bin,count,exposureTime;
		long frequency;
		boolean done,useWindowAmplifier;

		index = 0;
		done = false;
		calibrationList = new Vector();
		while(done == false)
		{
			typeString = status.getProperty(LIST_KEY_STRING+index+LIST_KEY_TYPE_STRING);
			if(typeString != null)
			{
			// create calibration instance, and set it's type
				calibration = new DAY_CALIBRATECalibration();
				try
				{
					calibration.setType(typeString);
				}
				catch(Exception e)
				{
					String errorString = new String(dayCalibrateCommand.getId()+
						":loadCalibrationList:Failed to set type "+typeString+
						" at index "+index+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2203);
					dayCalibrateDone.setErrorString(errorString);
					dayCalibrateDone.setSuccessful(false);
					return false;
				}
			// get common parameters
				try
				{
					bin = status.getPropertyInteger(LIST_KEY_STRING+index+LIST_KEY_BIN_STRING);
					useWindowAmplifier  = status.getPropertyBoolean(LIST_KEY_STRING+index+
											LIST_KEY_AMPLIFIER_STRING);
					frequency = status.getPropertyLong(LIST_KEY_STRING+index+
										LIST_KEY_FREQUENCY_STRING);
					count = status.getPropertyInteger(LIST_KEY_STRING+index+LIST_KEY_COUNT_STRING);
					if(calibration.isBias())
					{
						exposureTime = 0;
					}
					else if(calibration.isDark())
					{
						exposureTime = status.getPropertyInteger(LIST_KEY_STRING+index+
							LIST_KEY_EXPOSURE_TIME_STRING);
					}
					else // we should never get here
						exposureTime = 0;
				}
				catch(Exception e)
				{
					String errorString = new String(dayCalibrateCommand.getId()+
						":loadCalibrationList:Failed at index "+index+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2204);
					dayCalibrateDone.setErrorString(errorString);
					dayCalibrateDone.setSuccessful(false);
					return false;
				}
			// set calibration data
				try
				{
					calibration.setBin(bin);
					calibration.setUseWindowAmplifier(useWindowAmplifier);
					calibration.setFrequency(frequency);
					calibration.setCount(count);
					calibration.setExposureTime(exposureTime);
				}
				catch(Exception e)
				{
					String errorString = new String(dayCalibrateCommand.getId()+
						":loadCalibrationList:Failed to set calibration data at index "+index+
						":bin:"+bin+":use window amplifier:"+useWindowAmplifier+
						":frequency:"+frequency+":count:"+count+
						":exposure time:"+exposureTime+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2205);
					dayCalibrateDone.setErrorString(errorString);
					dayCalibrateDone.setSuccessful(false);
					return false;
				}
			// add calibration instance to list
				calibrationList.add(calibration);
			// log
				o.log(Logging.VERBOSITY_VERBOSE,
				      "Command:"+dayCalibrateCommand.getClass().getName()+
				      ":Loaded calibration "+index+
				      "\n\ttype:"+calibration.getType()+
				      ":bin:"+calibration.getBin()+
				      ":use window amplifier:"+calibration.useWindowAmplifier()+
				      ":count:"+calibration.getCount()+
				      ":texposure time:"+calibration.getExposureTime()+
				      ":frequency:"+calibration.getFrequency()+".");
			}
			else
				done = true;
			index++;
		}
		return true;
	}

	/**
	 * Method to initialse dayCalibrateState and stateFilename.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. If false is returned the error
	 * 	data in dayCalibrateDone is filled in.
	 * @see #dayCalibrateState
	 * @see #stateFilename
	 */
	protected boolean initialiseState(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone)
	{
	// get stateFilename from properties
		try
		{
			stateFilename = status.getProperty("o.day_calibrate.state_filename");
		}
		catch (Exception e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":initialiseState:Failed to get state filename.");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2201);
			dayCalibrateDone.setErrorString(errorString);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
	// initialise and load dayCalibrateState instance
		dayCalibrateState = new DAY_CALIBRATESavedState();
		try
		{
			dayCalibrateState.load(stateFilename);
		}
		catch (Exception e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":initialiseState:Failed to load state filename:"+stateFilename);
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2202);
			dayCalibrateDone.setErrorString(errorString);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * This method matches the saved state to the calibration list to set the last time
	 * each calibration was completed.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. It currently always returns true.
	 * @see #calibrationList
	 * @see #dayCalibrateState
	 */
	protected boolean addSavedStateToCalibration(DAY_CALIBRATE dayCalibrateCommand,
		DAY_CALIBRATE_DONE dayCalibrateDone)
	{

		DAY_CALIBRATECalibration calibration = null;
		int type,count,bin,exposureTime;
		long lastTime;
		boolean useWindowAmplifier;

		for(int i = 0; i< calibrationList.size(); i++)
		{
			calibration = (DAY_CALIBRATECalibration)(calibrationList.get(i));
			type = calibration.getType();
			bin = calibration.getBin();
			useWindowAmplifier = calibration.useWindowAmplifier();
			exposureTime = calibration.getExposureTime();
			count = calibration.getCount();
			lastTime = dayCalibrateState.getLastTime(type,bin,useWindowAmplifier,exposureTime,count);
			calibration.setLastTime(lastTime);
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+dayCalibrateCommand.getClass().getName()+":Calibration:"+
			      "\n\ttype:"+calibration.getType()+
			      ":bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":count:"+calibration.getCount()+
			      ":exposure time:"+calibration.getExposureTime()+
			      ":frequency:"+calibration.getFrequency()+
			      "\n\t\tnow has last time set to:"+lastTime+".");
		}
		return true;
	}

	/**
	 * This method try's to determine whether we should perform the passed in calibration.
	 * The following  cases are tested:
	 * <ul>
	 * <li>If the current time is after the implementationStartTime plus the dayCalibrateCommand's timeToComplete
	 * 	return false, because the DAY_CALIBRATE command should be stopping (it's run out of time).
	 * <li>If the difference between the current time and the last time the calibration was done is
	 * 	less than the frequency return false, it's too soon to do this calibration again.
	 * <li>We work out how long it will take us to do the calibration, using the <b>count</b>, 
	 * 	<b>exposureTime</b>, and the <b>readoutOverhead</b> property.
	 * <li>If it's going to take us longer to do the calibration than the remaining time available, return
	 * 	false.
	 * <li>Otherwise, return true.
	 * </ul>
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @param calibration The calibration we wish to determine whether to do or not.
	 * @return The method returns true if we should do the calibration, false if we should not.
	 */
	protected boolean testCalibration(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,
						DAY_CALIBRATECalibration calibration)
	{
		long now;
		long calibrationCompletionTime;

	// get current time
		now = System.currentTimeMillis();
	// if current time is after the implementation start time plus time to complete, it's time to finish
	// We don't want to do any more calibrations, return false.
		if(now > (implementationStartTime+dayCalibrateCommand.getTimeToComplete()))
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+dayCalibrateCommand.getClass().getName()+":Testing Calibration:"+
			      "\n\ttype:"+calibration.getType()+
			      ":bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":count:"+calibration.getCount()+
			      ":exposure time:"+calibration.getExposureTime()+
			      ":frequency:"+calibration.getFrequency()+
			      "\n\tlast time:"+calibration.getLastTime()+
			      "\n\t\twill not be done,time to complete exceeded ("+now+" > ("+
			      implementationStartTime+" + "+dayCalibrateCommand.getTimeToComplete()+")).");
			return false;
		}
	// If the last time we did this calibration was less than frequency milliseconds ago, then it's
	// too soon to do the calibration again.
		if((now-calibration.getLastTime()) < calibration.getFrequency())
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+dayCalibrateCommand.getClass().getName()+":Testing Calibration:"+
			      "\n\ttype:"+calibration.getType()+
			      ":bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":count:"+calibration.getCount()+
			      ":exposure time:"+calibration.getExposureTime()+
			      ":frequency:"+calibration.getFrequency()+
			      "\n\tlast time:"+calibration.getLastTime()+
			      "\n\t\twill not be done,too soon after last calibration.");
			return false;
		}
	// How long will it take us to do this calibration?
		if(calibration.isBias())
		{
			calibrationCompletionTime = calibration.getCount()*readoutOverhead;
		}
		else if(calibration.isDark())
		{
			calibrationCompletionTime = calibration.getCount()*
					(calibration.getExposureTime()+readoutOverhead);
		}
		else // we should never get here, but if we do make method return false.
			calibrationCompletionTime = Long.MAX_VALUE;
	// if it's going to take us longer than the remaining time to do this, return false
		if((now+calibrationCompletionTime) > (implementationStartTime+dayCalibrateCommand.getTimeToComplete()))
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+dayCalibrateCommand.getClass().getName()+":Testing Calibration:"+
			      "\n\ttype:"+calibration.getType()+
			      ":bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":count:"+calibration.getCount()+
			      ":exposure time:"+calibration.getExposureTime()+
			      ":frequency:"+calibration.getFrequency()+
			      "\n\tlast time:"+calibration.getLastTime()+
			      "\n\t\twill not be done,will take too long to complete:"+
			      calibrationCompletionTime+".");
			return false;
		}
		return true;
	}

	/**
	 * This method does the specified calibration.
	 * <ul>
	 * <li>The relevant data is retrieved from the calibration parameter.
	 * <li><b>doConfig</b> is called for the relevant binning factor to be setup.
	 * <li><b>sendBasicAck</b> is called to stop the client timing out before the first frame is completed.
	 * <li><b>doFrames</b> is called to exposure count frames with the correct exposure length (DARKs only).
	 * <li>If the calibration suceeded, the saved state's last time is updated to now, and the state saved.
	 * </ul>
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @param calibration The calibration to do.
	 * @return The method returns true if the calibration was done successfully, false if an error occured.
	 * @see #doConfig
	 * @see #doFrames
	 * @see #sendBasicAck
	 * @see #stateFilename
	 */
	protected boolean doCalibration(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,
						DAY_CALIBRATECalibration calibration)
	{
		int type,count,bin,exposureTime;
		long lastTime;
		boolean useWindowAmplifier;

		o.log(Logging.VERBOSITY_VERBOSE,
			"Command:"+dayCalibrateCommand.getClass().getName()+
			":doCalibrate:type:"+calibration.getType()+":bin:"+calibration.getBin()+
			":use window amplifier:"+calibration.useWindowAmplifier()+
			":count:"+calibration.getCount()+":exposure time:"+calibration.getExposureTime()+
			":frequency:"+calibration.getFrequency()+".");
	// get copy of calibration data
		type = calibration.getType();
		bin = calibration.getBin();
		useWindowAmplifier = calibration.useWindowAmplifier();
		count = calibration.getCount();
		exposureTime = calibration.getExposureTime();
	// configure CCD camera
	// don't send a basic ack, as setting the binning takes less than 1 second
		if(doConfig(dayCalibrateCommand,dayCalibrateDone,bin,useWindowAmplifier) == false)
			return false;
	// send an ack before the frame, so the client doesn't time out during the first exposure
		if(sendBasicAck(dayCalibrateCommand,dayCalibrateDone,exposureTime+readoutOverhead) == false)
			return false;
	// do the frames with this configuration
		if(doFrames(dayCalibrateCommand,dayCalibrateDone,type,exposureTime,count) == false)
			return false;
	// update state
		dayCalibrateState.setLastTime(type,bin,useWindowAmplifier,exposureTime,count);
		try
		{
			dayCalibrateState.save(stateFilename);
		}
		catch(IOException e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":doCalibration:Failed to save state filename:"+stateFilename);
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2206);
			dayCalibrateDone.setErrorString(errorString);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
		lastTime = dayCalibrateState.getLastTime(type,bin,useWindowAmplifier,exposureTime,count);
		calibration.setLastTime(lastTime);
		return true;
	}

	/**
	 * Method to setup the CCD configuration with the specified binning factor.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @param bin The binning factor to use.
	 * @param useWindowAmplifier Boolean specifying whether to use the default amplifier (false), or the
	 *                           amplifier used for windowing readouts (true).
	 * @return The method returns true if the calibration was done successfully, false if an error occured.
	 * @see OStatus#getNumberColumns
	 * @see OStatus#getNumberRows
	 */
	protected boolean doConfig(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,int bin,
				   boolean useWindowAmplifier)
	{
		CCDLibrarySetupWindow windowList[] = new CCDLibrarySetupWindow[CCDLibrary.SETUP_WINDOW_COUNT];
		int numberColumns,numberRows,amplifier,deInterlaceSetting;

	// load other required config for dimension configuration from O properties file.
		try
		{
			numberColumns = status.getNumberColumns(bin);
			numberRows = status.getNumberRows(bin);
			amplifier = getAmplifier(useWindowAmplifier);
			deInterlaceSetting = getDeInterlaceSetting(useWindowAmplifier);
		}
	// CCDLibraryFormatException,IllegalArgumentException,NumberFormatException.
		catch(Exception e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":doConfig:Failed to get config:");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2207);
			dayCalibrateDone.setErrorString(errorString);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
	// test abort
		if(testAbort(dayCalibrateCommand,dayCalibrateDone) == true)
			return false;
		for(int i = 0; i < CCDLibrary.SETUP_WINDOW_COUNT; i++)
		{
			windowList[i] = new CCDLibrarySetupWindow(-1,-1,-1,-1);
		}
		if(testAbort(dayCalibrateCommand,dayCalibrateDone) == true)
			return false;
	// send dimension configuration to the SDSU controller
		try
		{
			ccd.setupDimensions(numberColumns,numberRows,bin,bin,
					    amplifier,deInterlaceSetting,0,windowList);
		}
		catch(Exception e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":doConfig:Failed to setup dimensions:");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2208);
			dayCalibrateDone.setErrorString(errorString);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
	// test abort
		if(testAbort(dayCalibrateCommand,dayCalibrateDone) == true)
			return false;
	// Increment unique config ID.
	// This is queried when saving FITS headers to get the CONFIGID value.
		try
		{
			status.incConfigId();
		}
		catch(Exception e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":doConfig:Incrementing configuration ID failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2209);
			dayCalibrateDone.setErrorString(errorString+e);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
	// Store name of configuration used in status object.
	// This is queried when saving FITS headers to get the CONFNAME value.
		status.setConfigName("DAY_CALIBRATION:"+dayCalibrateCommand.getId()+":"+bin+":"+useWindowAmplifier);
		return true;
	}

	/**
	 * The method that does a series of calibration frames with the current configuration, based
	 * on the passed in parameter set. The following occurs:
	 * <ul>
	 * <li>A loop is entered, from zero to <b>count</b>.
	 * <li>The pause and resume times are cleared, and the FITS headers setup from the current configuration.
	 * <li>Some FITS headers are got from the ISS.
	 * <li>testAbort is called to see if this command implementation has been aborted.
	 * <li>The correct filename code is set in oFilename. The run number is incremented and
	 * 	a new unique filename generated.
	 * <li>The FITS headers are saved using saveFitsHeaders.
	 * <li>The frame is taken, using libccd. If the <b>type</b> is BIAS, CCDLibrary's bias
	 * 	method is called, otherwise expose is used.
	 * <li>testAbort is called to see if this command implementation has been aborted.
	 * <li>reduceCalibrate is called to pass the frame to the Real Time Data Pipeline for processing.
	 * </ul>
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @param type The type of calibration, one of DAY_CALIBRATECalibration.TYPE_BIAS
	 * 	or DAY_CALIBRATECalibration.TYPE_DARK.
	 * @param exposureTime The length of exposure for a DARK, always zero for a BIAS.
	 * @param count The number of frames to do of this type.
	 * @return The method returns true if the calibration was done successfully, false if an error occured.
	 * @see DAY_CALIBRATEImplementation.DAY_CALIBRATECalibration#TYPE_BIAS
	 * @see DAY_CALIBRATEImplementation.DAY_CALIBRATECalibration#TYPE_DARK
	 * @see FITSImplementation#testAbort
	 * @see FITSImplementation#setFitsHeaders
	 * @see FITSImplementation#getFitsHeadersFromISS
	 * @see FITSImplementation#getFitsHeadersFromBSS
	 * @see FITSImplementation#saveFitsHeaders
	 * @see FITSImplementation#unLockFile
	 * @see FITSImplementation#oFilename
	 * @see FITSImplementation#ccd
	 * @see ngat.o.ccd.CCDLibrary#bias
	 * @see ngat.o.ccd.CCDLibrary#expose
	 * @see CALIBRATEImplementation#reduceCalibrate
	 * @see #readoutOverhead
	 */
	protected boolean doFrames(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,
				int type, int exposureTime, int count)
	{
		String filename = null;

		for(int i = 0;i < count; i++)
		{
		// Clear the pause and resume times.
			status.clearPauseResumeTimes();
			if(type == DAY_CALIBRATECalibration.TYPE_BIAS)
			{
				if(setFitsHeaders(dayCalibrateCommand,dayCalibrateDone,
					FitsHeaderDefaults.OBSTYPE_VALUE_BIAS,0) == false)
					return false;
			}
			else if (type == DAY_CALIBRATECalibration.TYPE_DARK)
			{
				if(setFitsHeaders(dayCalibrateCommand,dayCalibrateDone,
					FitsHeaderDefaults.OBSTYPE_VALUE_DARK,exposureTime) == false)
					return false;
			}
			if(getFitsHeadersFromISS(dayCalibrateCommand,dayCalibrateDone) == false)
				return false;
			if(getFitsHeadersFromBSS(dayCalibrateCommand,dayCalibrateDone) == false)
				return false;
			if(testAbort(dayCalibrateCommand,dayCalibrateDone) == true)
				return false;
		// get a filename to store frame in
			try
			{
				if(type == DAY_CALIBRATECalibration.TYPE_BIAS)
				{
					oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_BIAS);
				}
				else if (type == DAY_CALIBRATECalibration.TYPE_DARK)
				{
					oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_DARK);
				}
			}
			catch(Exception e)
			{
				String errorString = new String(dayCalibrateCommand.getId()+
					":doFrames:Setting exposure code "+i+" failed:");
				o.error(this.getClass().getName()+":"+errorString,e);
				dayCalibrateDone.setFilename(filename);
				dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2214);
				dayCalibrateDone.setErrorString(errorString+e);
				dayCalibrateDone.setSuccessful(false);
				return false;
			}
			oFilename.nextRunNumber();
			filename = oFilename.getFilename();
			if(saveFitsHeaders(dayCalibrateCommand,dayCalibrateDone,filename) == false)
			{
				unLockFile(dayCalibrateCommand,dayCalibrateDone,filename);
				return false;
			}
			status.setExposureFilename(filename);
		// do exposure
			try
			{
				if(type == DAY_CALIBRATECalibration.TYPE_BIAS)
				{
					ccd.bias(filename);
				}
				else if (type == DAY_CALIBRATECalibration.TYPE_DARK)
				{
					ccd.expose(false,-1,exposureTime,filename);
				}
			}
			catch(CCDLibraryNativeException e)
			{
				String errorString = new String(dayCalibrateCommand.getId()+
					":doFrames:Doing frame "+i+" failed:");
				o.error(this.getClass().getName()+":"+errorString,e);
				dayCalibrateDone.setFilename(filename);
				dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2210);
				dayCalibrateDone.setErrorString(errorString+e);
				dayCalibrateDone.setSuccessful(false);
				unLockFile(dayCalibrateCommand,dayCalibrateDone,filename);
				return false;
			}
		// unlock FITS lock file created by saveFitsHeaders
			if(unLockFile(dayCalibrateCommand,dayCalibrateDone,filename) == false)
				return false;
		// send with filename back to client
		// time to complete is reduction time, we will send another ACK after reduceCalibrate
			if(sendDayCalibrateAck(dayCalibrateCommand,dayCalibrateDone,readoutOverhead,filename) == false)
				return false; 
		// Test abort status.
			if(testAbort(dayCalibrateCommand,dayCalibrateDone) == true)
				return false;
		// Call pipeline to reduce data.
			if(reduceCalibrate(dayCalibrateCommand,dayCalibrateDone,filename) == false)
				return false; 
		// send dp_ack, filename/mean counts/peak counts are all retrieved from dayCalibrateDone,
		// which had these parameters filled in by reduceCalibrate
		// time to complete is readout overhead + exposure Time for next frame
			if(sendDayCalibrateDpAck(dayCalibrateCommand,dayCalibrateDone,
				exposureTime+readoutOverhead) == false)
				return false;
		}// end for on count
		return true;
	}

	/**
	 * Method to send an instance of ACK back to the client. This stops the client timing out, whilst we
	 * work out what calibration to attempt next.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @param timeToComplete The time it will take to complete the next set of operations
	 *	before the next ACK or DONE is sent to the client. The time is in milliseconds. 
	 * 	The server connection thread's default acknowledge time is added to the value before it
	 * 	is sent to the client, to allow for network delay etc.
	 * @return The method returns true if the ACK was sent successfully, false if an error occured.
	 */
	protected boolean sendBasicAck(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,
		int timeToComplete)
	{
		ACK acknowledge = null;

		acknowledge = new ACK(dayCalibrateCommand.getId());
		acknowledge.setTimeToComplete(timeToComplete+serverConnectionThread.getDefaultAcknowledgeTime());
		try
		{
			serverConnectionThread.sendAcknowledge(acknowledge,true);
		}
		catch(IOException e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":sendBasicAck:Sending ACK failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2213);
			dayCalibrateDone.setErrorString(errorString+e);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to send an instance of DAY_CALIBRATE_ACK back to the client. This tells the client about
	 * a FITS frame that has been produced, and also stops the client timing out.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * @param timeToComplete The time it will take to complete the next set of operations
	 *	before the next ACK or DONE is sent to the client. The time is in milliseconds. 
	 * 	The server connection thread's default acknowledge time is added to the value before it
	 * 	is sent to the client, to allow for network delay etc.
	 * @param filename The FITS filename to be sent back to the client, that has just completed
	 * 	processing.
	 * @return The method returns true if the ACK was sent successfully, false if an error occured.
	 */
	protected boolean sendDayCalibrateAck(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,
		int timeToComplete,String filename)
	{
		DAY_CALIBRATE_ACK dayCalibrateAck = null;

	// send acknowledge to say frame is completed.
		dayCalibrateAck = new DAY_CALIBRATE_ACK(dayCalibrateCommand.getId());
		dayCalibrateAck.setTimeToComplete(timeToComplete+
			serverConnectionThread.getDefaultAcknowledgeTime());
		dayCalibrateAck.setFilename(filename);
		try
		{
			serverConnectionThread.sendAcknowledge(dayCalibrateAck,true);
		}
		catch(IOException e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":sendDayCalibrateAck:Sending DAY_CALIBRATE_ACK failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2211);
			dayCalibrateDone.setErrorString(errorString+e);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to send an instance of DAY_CALIBRATE_DP_ACK back to the client. This tells the client about
	 * a FITS frame that has been produced, and the mean and peak counts in the frame.
	 * The time to complete parameter stops the client timing out.
	 * @param dayCalibrateCommand The instance of DAY_CALIBRATE we are currently running.
	 * @param dayCalibrateDone The instance of DAY_CALIBRATE_DONE to fill in with errors we receive.
	 * 	It also contains the filename and mean and peak counts returned from the last reduction calibration.
	 * @param timeToComplete The time it will take to complete the next set of operations
	 *	before the next ACK or DONE is sent to the client. The time is in milliseconds. 
	 * 	The server connection thread's default acknowledge time is added to the value before it
	 * 	is sent to the client, to allow for network delay etc.
	 * @return The method returns true if the ACK was sent successfully, false if an error occured.
	 */
	protected boolean sendDayCalibrateDpAck(DAY_CALIBRATE dayCalibrateCommand,DAY_CALIBRATE_DONE dayCalibrateDone,
		int timeToComplete)
	{
		DAY_CALIBRATE_DP_ACK dayCalibrateDpAck = null;

	// send acknowledge to say frame is completed.
		dayCalibrateDpAck = new DAY_CALIBRATE_DP_ACK(dayCalibrateCommand.getId());
		dayCalibrateDpAck.setTimeToComplete(timeToComplete+
			serverConnectionThread.getDefaultAcknowledgeTime());
		dayCalibrateDpAck.setFilename(dayCalibrateDone.getFilename());
		dayCalibrateDpAck.setMeanCounts(dayCalibrateDone.getMeanCounts());
		dayCalibrateDpAck.setPeakCounts(dayCalibrateDone.getPeakCounts());
		try
		{
			serverConnectionThread.sendAcknowledge(dayCalibrateDpAck,true);
		}
		catch(IOException e)
		{
			String errorString = new String(dayCalibrateCommand.getId()+
				":sendDayCalibrateDpAck:Sending DAY_CALIBRATE_DP_ACK failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			dayCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2212);
			dayCalibrateDone.setErrorString(errorString+e);
			dayCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Private inner class that deals with loading and interpreting the saved state of calibrations
	 * (the DAY_CALIBRATE calibration database).
	 */
	private class DAY_CALIBRATESavedState
	{
		private NGATProperties properties = null;

		/**
		 * Constructor.
		 */
		public DAY_CALIBRATESavedState()
		{
			super();
			properties = new NGATProperties();
		}

		/**
	 	 * Load method, that retrieves the saved state from file.
		 * Calls the <b>properties</b> load method.
		 * @param filename The filename to load the saved state from.
		 * @exception FileNotFoundException Thrown if the file described by filename does not exist.
		 * @exception IOException Thrown if an IO error occurs whilst reading the file.
		 * @see #properties
	 	 */
		public void load(String filename) throws FileNotFoundException, IOException
		{
			properties.load(filename);
		}

		/**
	 	 * Save method, that stores the saved state into a file.
		 * Calls the <b>properties</b> load method.
		 * @param filename The filename to save the saved state to.
		 * @exception FileNotFoundException Thrown if the file described by filename does not exist.
		 * @exception IOException Thrown if an IO error occurs whilst writing the file.
		 * @see #properties
	 	 */
		public void save(String filename) throws IOException
		{
			Date now = null;

			now = new Date();
			properties.save(filename,"DAY_CALIBRATE saved state saved on:"+now);
		}

		/**
		 * Method to get the last time a calibration with these attributes was done.
		 * @param type The type of calibration, one of DAY_CALIBRATECalibration.TYPE_BIAS
		 * 	or DAY_CALIBRATECalibration.TYPE_DARK.
		 * @param bin The binning factor used for this calibration.
		 * @param useWindowAmplifier Whether we are using the default amplifier (false) or the amplifier
		 *        used for windowing (true).
		 * @param exposureTime The length of exposure for a DARK, always zero for a BIAS.
		 * @param count The number of frames.
		 * @return The number of milliseconds since the EPOCH, the last time a calibration with these
		 * 	parameters was completed. If this calibraion has not been performed before, zero
		 * 	is returned.
		 * @see DAY_CALIBRATEImplementation.DAY_CALIBRATECalibration#TYPE_BIAS
		 * @see DAY_CALIBRATEImplementation.DAY_CALIBRATECalibration#TYPE_DARK
		 * @see #LIST_KEY_STRING
		 * @see #LIST_KEY_LAST_TIME_STRING
		 */
		public long getLastTime(int type,int bin,boolean useWindowAmplifier,int exposureTime,int count)
		{
			long time;

			try
			{
				time = properties.getLong(LIST_KEY_STRING+LIST_KEY_LAST_TIME_STRING+
					       type+"."+bin+"."+useWindowAmplifier+"."+exposureTime+"."+count);
			}
			catch(NGATPropertyException e)/* assume failure due to key not existing */
			{
				time = 0;
			}
			return time;
		}

		/**
		 * Method to set the last time a calibration with these attributes was done.
		 * The time is set to now. The property file should be saved after a call to this method is made.
		 * @param type The type of calibration, one of DAY_CALIBRATECalibration.TYPE_BIAS
		 * 	or DAY_CALIBRATECalibration.TYPE_DARK.
		 * @param bin The binning factor used for this calibration.
		 * @param useWindowAmplifier Whether we are using the default amplifier (false) or the amplifier
		 *        used for windowing (true).
		 * @param exposureTime The length of exposure for a DARK, always zero for a BIAS.
		 * @param count The number of frames.
		 * @see DAY_CALIBRATEImplementation.DAY_CALIBRATECalibration#TYPE_BIAS
		 * @see DAY_CALIBRATEImplementation.DAY_CALIBRATECalibration#TYPE_DARK
		 * @see #LIST_KEY_STRING
		 * @see #LIST_KEY_LAST_TIME_STRING
		 */
		public void setLastTime(int type,int bin,boolean useWindowAmplifier,int exposureTime,int count)
		{
			long now;

			now = System.currentTimeMillis();
			properties.setProperty(LIST_KEY_STRING+LIST_KEY_LAST_TIME_STRING+type+"."+bin+"."+
					       useWindowAmplifier+"."+exposureTime+"."+count,new String(""+now));
		}
	}

	/**
	 * Private inner class that stores data pertaining to one possible calibration run that can take place during
	 * a DAY_CALIBRATE command invocation.
	 */
	private class DAY_CALIBRATECalibration
	{
		/**
		 * Constant used in the type field to specify this calibration is a BIAS.
		 * @see #type
		 */
		public final static int TYPE_BIAS = 1;
		/**
		 * Constant used in the type field to specify this calibration is a DARK.
		 * @see #type
		 */
		public final static int TYPE_DARK = 2;
		/**
		 * What type of calibration is it? A DARK or a BIAS?
		 * @see #TYPE_BIAS
		 * @see #TYPE_DARK
		 */
		protected int type = 0;
		/**
		 * What binning to configure the ccd to for this calibration.
		 */
		protected int bin;
		/**
		 * Which amplifier should we use to readout.
		 * If false, we use the default amplifier.
		 * If true, we use the window amplifier.
		 */
		protected boolean useWindowAmplifier;
		/**
		 * How many times to perform this calibration.
		 */
		protected int count;
		/**
		 * How often we should perform the calibration in milliseconds.
		 */
		protected long frequency;
		/**
		 * How long an exposure time this calibration has. This is zero for BIAS frames.
		 */
		protected int exposureTime;
		/**
		 * The last time this calibration was performed. This is retrieved from the saved state,
		 * not from the calibration list.
		 */
		protected long lastTime;
		
		/**
		 * Constructor.
		 */
		public DAY_CALIBRATECalibration()
		{
			super();
		}

		/**
		 * Method to set the type of the calibration i.e. is it a DARK or BIAS.
		 * @param typeString A string describing the type. This should be &quot;dark&quot; or
		 * 	&quot;bias&quot;.
		 * @exception IllegalArgumentException Thrown if typeString is an illegal type.
		 * @see #type
		 */
		public void setType(String typeString) throws IllegalArgumentException
		{
			if(typeString.equals("dark"))
				type = TYPE_DARK;
			else if(typeString.equals("bias"))
				type = TYPE_BIAS;
			else
				throw new IllegalArgumentException(this.getClass().getName()+":setType failed:"+
					typeString+" not a legal type of calibration.");
		}

		/**
		 * Method to get the calibration type of this calibration.
		 * @return The type of calibration.
		 * @see #type
		 * @see #TYPE_DARK
		 * @see #TYPE_BIAS
		 */
		public int getType()
		{
			return type;
		}

		/**
		 * Method to return whether this calibration is a DARK.
		 * @return This method returns true if <b>type</b> is <b>TYPE_DARK</b>, otherwqise it returns false.
		 * @see #type
		 * @see #TYPE_DARK
		 */
		public boolean isDark()
		{
			return (type == TYPE_DARK);
		}

		/**
		 * Method to return whether this calibration is a BIAS.
		 * @return This method returns true if <b>type</b> is <b>TYPE_BIAS</b>, otherwise it returns false.
		 * @see #type
		 * @see #TYPE_BIAS
		 */
		public boolean isBias()
		{
			return (type == TYPE_BIAS);
		}

		/**
		 * Method to set the binning configuration for this calibration.
		 * @param b The binning to use. This should be greater than 0 and less than 5.
		 * @exception IllegalArgumentException Thrown if parameter b is out of range.
		 * @see #bin
		 */
		public void setBin(int b) throws IllegalArgumentException
		{
			if((b < 1)||(b > 4))
			{
				throw new IllegalArgumentException(this.getClass().getName()+":setBin failed:"+
					b+" not a legal binning value.");
			}
			bin = b;
		}

		/**
		 * Method to get the binning configuration for this calibration.
		 * @return The binning.
		 * @see #bin
		 */
		public int getBin()
		{
			return bin;
		}

		/**
		 * Method to set whether we are using the default amplifier or the one used for windowing.
		 * @param b Set to true to use the windowing amplifier, false to use the default amplifier.
		 * @see #useWindowAmplifier
		 */
		public void setUseWindowAmplifier(boolean b)
		{
			useWindowAmplifier = b;
		}

		/**
		 * Method to get whether we are using the default amplifier or the one used for windowing.
		 * @return Returns true if we are using the windowing amplifier, 
		 *         false if we are using the default amplifier.
		 * @see #useWindowAmplifier
		 */
		public boolean useWindowAmplifier()
		{
			return useWindowAmplifier;
		}

		/**
		 * Method to set the frequency this calibration should be performed.
		 * @param f The frequency in milliseconds. This should be greater than zero.
		 * @exception IllegalArgumentException Thrown if parameter f is out of range.
		 * @see #frequency
		 */
		public void setFrequency(long f) throws IllegalArgumentException
		{
			if(f <= 0)
			{
				throw new IllegalArgumentException(this.getClass().getName()+":setFrequency failed:"+
					f+" not a legal frequency.");
			}
			frequency = f;
		}

		/**
		 * Method to get the frequency configuration for this calibration.
		 * @return The frequency this calibration should be performed, in milliseconds.
		 * @see #frequency
		 */
		public long getFrequency()
		{
			return frequency;
		}

		/**
		 * Method to set the number of times this calibration should be performed in one invocation
		 * of DAY_CALIBRATE.
		 * @param c The number of times this calibration should be performed in one invocation
		 * of DAY_CALIBRATE This should be greater than zero.
		 * @exception IllegalArgumentException Thrown if parameter c is out of range.
		 * @see #count
		 */
		public void setCount(int c) throws IllegalArgumentException
		{
			if(c <= 0)
			{
				throw new IllegalArgumentException(this.getClass().getName()+":setCount failed:"+
					c+" not a legal count.");
			}
			count = c;
		}

		/**
		 * Method to get the number of times this calibration is performed per invocation of DAY_CALIBRATE.
		 * @return The number of times this calibration is performed per invocation of DAY_CALIBRATE.
		 * @see #count
		 */
		public int getCount()
		{
			return count;
		}

		/**
		 * Method to set the exposure length of this calibration.
		 * @param t The exposure length in milliseconds. This should be greater than or equal to zero.
		 * @exception IllegalArgumentException Thrown if parameter t is out of range.
		 * @see #exposureTime
		 */
		public void setExposureTime(int t) throws IllegalArgumentException
		{
			if(t < 0)
			{
				throw new IllegalArgumentException(this.getClass().getName()+
					":setExposureTime failed:"+t+" not a legal exposure length.");
			}
			exposureTime = t;
		}

		/**
		 * Method to get the exposure length of this calibration.
		 * @return The exposure length of this calibration.
		 * @see #exposureTime
		 */
		public int getExposureTime()
		{
			return exposureTime;
		}

		/**
		 * Method to set the last time this calibration was performed.
		 * @param t A long representing the last time the calibration was done, as a 
		 * 	number of milliseconds since the EPOCH.
		 * @exception IllegalArgumentException Thrown if parameter f is out of range.
		 * @see #frequency
		 */
		public void setLastTime(long t) throws IllegalArgumentException
		{
			if(t < 0)
			{
				throw new IllegalArgumentException(this.getClass().getName()+":setLastTime failed:"+
					t+" not a legal last time.");
			}
			lastTime = t;
		}

		/**
		 * Method to get the last time this calibration was performed.
		 * @return The number of milliseconds since the epoch that this calibration was last performed.
		 * @see #frequency
		 */
		public long getLastTime()
		{
			return lastTime;
		}
	}
}

//
// $Log: not supported by cvs2svn $
//
