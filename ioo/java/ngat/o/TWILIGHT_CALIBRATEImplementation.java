// TWILIGHT_CALIBRATEImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/TWILIGHT_CALIBRATEImplementation.java,v 1.11 2013-03-25 15:01:38 cjm Exp $
package ngat.o;

import java.io.*;
import java.lang.*;
import java.util.*;

import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.OFFSET_FOCUS;
import ngat.message.ISS_INST.OFFSET_FOCUS_DONE;
import ngat.message.ISS_INST.OFFSET_RA_DEC;
import ngat.message.ISS_INST.OFFSET_RA_DEC_DONE;
import ngat.message.ISS_INST.INST_TO_ISS_DONE;
import ngat.message.ISS_INST.TWILIGHT_CALIBRATE;
import ngat.message.ISS_INST.TWILIGHT_CALIBRATE_ACK;
import ngat.message.ISS_INST.TWILIGHT_CALIBRATE_DP_ACK;
import ngat.message.ISS_INST.TWILIGHT_CALIBRATE_DONE;
import ngat.phase2.*;
import ngat.util.*;
import ngat.util.logging.*;

/**
 * This class provides the implementation of a TWILIGHT_CALIBRATE command sent to a server using the
 * Java Message System. It performs a series of SKYFLAT frames from a configurable list,
 * taking into account frames done in previous invocations of this command (it saves it's state).
 * The exposure length is dynamically adjusted as the sky gets darker or brighter. TWILIGHT_CALIBRATE commands
 * should be sent to O just after sunset and just before sunrise.
 * @author Chris Mottram
 * @version $Revision: 1.11 $
 */
public class TWILIGHT_CALIBRATEImplementation extends CALIBRATEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: TWILIGHT_CALIBRATEImplementation.java,v 1.11 2013-03-25 15:01:38 cjm Exp $");
	/**
	 * The number of different binning factors we should min/best/max count data for.
	 * Actually 1 more than the maximum used binning, as we go from 1 not 0.
	 * @see #minMeanCounts
	 * @see #maxMeanCounts
	 * @see #bestMeanCounts
	 */
	protected final static int BIN_COUNT 	     = 5;
	/**
	 * Initial part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_STRING = "o.twilight_calibrate.";
	/**
	 * Middle part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_CALIBRATION_STRING = "calibration.";
	/**
	 * Middle part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_SUNSET_STRING = "sunset.";
	/**
	 * Middle part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_SUNRISE_STRING = "sunrise.";
	/**
	 * Final part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_BIN_STRING = ".bin";
	/**
	 * Final part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_AMPLIFIER_STRING = ".window_amplifier";
	/**
	 * Middle part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_SLIDE_STRING = ".slide";
	/**
	 * Final part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_LOWER_STRING = ".lower";
	/**
	 * Final part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_UPPER_STRING = ".upper";
	/**
	 * Final part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_FILTER_STRING = ".filter";
	/**
	 * Final part of a key string, used to create a list of potential twilight calibrations to
	 * perform from a Java property file.
	 */
	protected final static String LIST_KEY_FREQUENCY_STRING = ".frequency";
	/**
	 * Middle part of a key string, used for saving and restoring the stored calibration state.
	 */
	protected final static String LIST_KEY_LAST_TIME_STRING = "last_time.";
	/**
	 * Middle part of a key string, used to load a list of telescope RA/DEC offsets from a Java property file.
	 */
	protected final static String LIST_KEY_OFFSET_STRING = "offset.";
	/**
	 * Last part of a key string, used to load a list of telescope RA/DEC offsets from a Java property file.
	 */
	protected final static String LIST_KEY_RA_STRING = ".ra";
	/**
	 * Last part of a key string, used to load a list of telescope RA/DEC offsets from a Java property file.
	 */
	protected final static String LIST_KEY_DEC_STRING = ".dec";
	/**
	 * Middle part of a key string, used to get comparative filter sensitivity.
	 */
	protected final static String LIST_KEY_FILTER_SENSITIVITY_STRING = "filter_sensitivity.";
	/**
	 * Constant used for time of night determination.
	 * @see #timeOfNight
	 */
	protected final static int TIME_OF_NIGHT_UNKNOWN = 0;
	/**
	 * Constant used for time of night determination.
	 * @see #timeOfNight
	 */
	protected final static int TIME_OF_NIGHT_SUNSET	= 1;
	/**
	 * Constant used for time of night determination.
	 * @see #timeOfNight
	 */
	protected final static int TIME_OF_NIGHT_SUNRISE = 2;
	/**
	 * A possible state of a frame taken by this command. 
	 * The frame did not have enough counts to be useful, i.e. the mean counts were less than minMeanCounts[bin].
	 * @see #minMeanCounts
	 */
	protected final static int FRAME_STATE_UNDEREXPOSED 	= 0;
	/**
	 * A possible state of a frame taken by this command. 
	 * The mean counts for the frame were sensible, i.e. the mean counts were more than minMeanCounts[bin] and less
	 * than maxMeanCounts[bin].
	 * @see #minMeanCounts
	 * @see #maxMeanCounts
	 */
	protected final static int FRAME_STATE_OK 		= 1;
	/**
	 * A possible state of a frame taken by this command. 
	 * The frame had too many counts to be useful, i.e. the mean counts were higher than maxMeanCounts[bin].
	 * @see #maxMeanCounts
	 */
	protected final static int FRAME_STATE_OVEREXPOSED 	= 2;
	/**
	 * The number of possible frame states.
	 * @see #FRAME_STATE_UNDEREXPOSED
	 * @see #FRAME_STATE_OK
	 * @see #FRAME_STATE_OVEREXPOSED
	 */
	protected final static int FRAME_STATE_COUNT 	= 3;
	/**
	 * Description strings for the frame states, indexed by the frame state enumeration numbers.
	 * @see #FRAME_STATE_UNDEREXPOSED
	 * @see #FRAME_STATE_OK
	 * @see #FRAME_STATE_OVEREXPOSED
	 * @see #FRAME_STATE_COUNT
	 */
	protected final static String FRAME_STATE_NAME_LIST[] = {"underexposed","ok","overexposed"};
	/**
	 * The time, in milliseconds since the epoch, that the implementation of this command was started.
	 */
	private long implementationStartTime = 0L;
	/**
	 * The saved state of calibrations done over time by invocations of this command.
	 * @see TWILIGHT_CALIBRATESavedState
	 */
	private TWILIGHT_CALIBRATESavedState twilightCalibrateState = null;
	/**
	 * The filename holding the saved state data.
	 */
	private String stateFilename = null;
	/**
	 * The list of calibrations to select from.
	 * Each item in the list is an instance of TWILIGHT_CALIBRATECalibration.
	 * @see TWILIGHT_CALIBRATECalibration
	 */
	protected List calibrationList = null;
	/**
	 * The list of telescope offsets to do each calibration for.
	 * Each item in the list is an instance of TWILIGHT_CALIBRATEOffset.
	 * @see TWILIGHT_CALIBRATEOffset
	 */
	protected List offsetList = null;
	/**
	 * The frame overhead for a full frame, in milliseconds. This takes into account readout time,
	 * real time data reduction, communication overheads and the like.
	 */
	private int frameOverhead = 0;
	/**
	 * The minimum allowable exposure time for a frame, in milliseconds.
	 */
	private int minExposureLength = 0;
	/**
	 * The maximum allowable exposure time for a frame, in milliseconds.
	 */
	private int maxExposureLength = 0;
	/**
	 * The exposure time for the current frame, in milliseconds.
	 */
	private int exposureLength = 0;
	/**
	 * The exposure time for the last frame exposed, in milliseconds.
	 */
	private int lastExposureLength = 0;
	/**
	 * What time of night we are doing the calibration, is it sunset or sunrise.
	 * @see #TIME_OF_NIGHT_UNKNOWN
	 * @see #TIME_OF_NIGHT_SUNSET
	 * @see #TIME_OF_NIGHT_SUNRISE
	 */
	private int timeOfNight = TIME_OF_NIGHT_UNKNOWN;
	/**
	 * Filename used to save FITS frames to, until they are determined to contain valid data
	 * (the counts in them are within limits).
	 */
	private String temporaryFITSFilename = null;
	/**
	 * The minimum mean counts. A &quot;good&quot; frame will have a mean counts greater than this number.
	 * A list, indexed by binning factor, as the chip has different saturation values at different binnings.
	 * The array must be BIN_COUNT size long.
	 */
	private int minMeanCounts[] = {0,0,0,0,0};
	/**
	 * The best mean counts. The &quot;ideal&quot; frame will have a mean counts of this number.
	 * A list, indexed by binning factor, as the chip has different saturation values at different binnings.
	 * The array must be BIN_COUNT size long.
	 */
	private int bestMeanCounts[] = {0,0,0,0,0};
	/**
	 * The maximum mean counts. A &quot;good&quot; frame will have a mean counts less than this number.
	 * A list, indexed by binning factor, as the chip has different saturation values at different binnings.
	 * The array must be BIN_COUNT size long.
	 */
	private int maxMeanCounts[] = {0,0,0,0,0};
	/**
	 * The last relative filter sensitivity used for calculating exposure lengths.
	 */
	private double lastFilterSensitivity = 1.0;
	/**
	 * The last bin factor used for calculating exposure lengths.
	 */
	private int lastBin = 1;
	/**
	 * Loop terminator for the calibration list loop.
	 */
	private boolean doneCalibration = false;
	/**
	 * Loop terminator for the offset list loop.
	 */
	private boolean doneOffset = false;
	/**
	 * The number of calibrations completed that have a frame state of good,
	 * for the currently executing calibration.
	 * A calibration is successfully completed if the calibrationFrameCount is
	 * equal to the offsetList size at the end of the offset list loop.
	 */
	private int calibrationFrameCount = 0;

	/**
	 * Constructor.
	 */
	public TWILIGHT_CALIBRATEImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.TWILIGHT_CALIBRATE&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.TWILIGHT_CALIBRATE";
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
	 * This method gets the TWILIGHT_CALIBRATE command's acknowledge time. 
	 * This returns the server connection threads default acknowledge time.
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
	 * This method implements the TWILIGHT_CALIBRATE command.
	 * <ul>
	 * <li>The implementation start time is saved.
	 * <li><b>setTimeOfNight</b> is called to set the time of night flag.
	 * <li><b>loadProperties</b> is called to get configuration data from the properties.
	 * <li><b>addSavedStateToCalibration</b> is called, which finds the correct last time for each
	 * 	calibration in the list and sets the relevant field.
	 * <li>The FITS headers are cleared, and a the MULTRUN number is incremented.
	 * <li>The fold mirror is moved to the correct location using <b>moveFold</b>.
	 * <li>For each calibration, we do the following:
	 *      <ul>
	 *      <li><b>doCalibration</b> is called.
	 *      </ul>
	 * <li>sendBasicAck is called, to stop the client timing out whilst creating the master flat.
	 * <li>The makeMasterFlat method is called, to create master flat fields from the data just taken.
	 * </ul>
	 * Note this method assumes the loading and initialisation before the main loop takes less than the
	 * default acknowledge time, as no ACK's are sent to the client until we are ready to do the first
	 * sequence of calibration frames.
	 * @param command The command to be implemented.
	 * @return An instance of TWILIGHT_CALIBRATE_DONE is returned, with it's fields indicating
	 * 	the result of the command implementation.
	 * @see #implementationStartTime
	 * @see #exposureLength
	 * @see #lastFilterSensitivity
	 * @see #setTimeOfNight
	 * @see #addSavedStateToCalibration
	 * @see FITSImplementation#moveFold
	 * @see FITSImplementation#clearFitsHeaders
	 * @see FITSImplementation#oFilename
	 * @see #doCalibration
	 * @see #frameOverhead
	 * @see CALIBRATEImplementation#makeMasterFlat
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		TWILIGHT_CALIBRATE twilightCalibrateCommand = (TWILIGHT_CALIBRATE)command;
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone = new TWILIGHT_CALIBRATE_DONE(command.getId());
		TWILIGHT_CALIBRATECalibration calibration = null;
		int calibrationListIndex = 0;
		String directoryString = null;
		int makeFlatAckTime;

		twilightCalibrateDone.setMeanCounts(0.0f);
		twilightCalibrateDone.setPeakCounts(0.0f);
	// initialise
		implementationStartTime = System.currentTimeMillis();
		setTimeOfNight();
		if(loadProperties(twilightCalibrateCommand,twilightCalibrateDone) == false)
			return twilightCalibrateDone;
	// initialise status/fits header info, in case any frames are produced.
		status.setExposureCount(-1);
		status.setExposureNumber(0);
	// increment multrun number if filename generation object
		oFilename.nextMultRunNumber();
		try
		{
			oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_SKY_FLAT);
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":"+e.toString());
			twilightCalibrateDone.setFilename(null);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2320);
			twilightCalibrateDone.setErrorString(e.toString());
			twilightCalibrateDone.setSuccessful(false);
			return twilightCalibrateDone;
		}
	// match saved state to calibration list (put last time into calibration list)
		if(addSavedStateToCalibration(twilightCalibrateCommand,twilightCalibrateDone) == false)
			return twilightCalibrateDone;
	// move the fold mirror to the correct location
		if(moveFold(twilightCalibrateCommand,twilightCalibrateDone) == false)
			return twilightCalibrateDone;
		if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
			return twilightCalibrateDone;
	// initialise exposureLength
		if(timeOfNight == TIME_OF_NIGHT_SUNRISE)
			exposureLength = maxExposureLength;
		else if(timeOfNight == TIME_OF_NIGHT_SUNSET)
			exposureLength = minExposureLength;
		else // this should never happen
			exposureLength = minExposureLength;
		// set lastFilterSensitivity/lastBin to contents of first calibration so initial exposure length
		// remains the same as the calculated above.
		lastFilterSensitivity = 1.0;
		lastBin = 1;
		if(calibrationList.size() > 0)
		{
			calibration = (TWILIGHT_CALIBRATECalibration)(calibrationList.get(0));
			lastFilterSensitivity = calibration.getFilterSensitivity();
			lastBin = calibration.getBin();
		}
		// initialise loop variables
		calibrationListIndex = 0;
		doneCalibration = false;
	// main loop, do calibrations until we run out of time.
		while((doneCalibration == false) && (calibrationListIndex < calibrationList.size()))
		{
		// get calibration
			calibration = (TWILIGHT_CALIBRATECalibration)(calibrationList.get(calibrationListIndex));
		// do calibration
			if(doCalibration(twilightCalibrateCommand,twilightCalibrateDone,calibration) == false)
				return twilightCalibrateDone;
			calibrationListIndex++;
		}// end for on calibration list
	// send an ack before make master processing, so the client doesn't time out.
		makeFlatAckTime = status.getPropertyInteger("o.twilight_calibrate.acknowledge_time.make_flat");
		if(sendBasicAck(twilightCalibrateCommand,twilightCalibrateDone,makeFlatAckTime) == false)
			return twilightCalibrateDone;
	// Call pipeline to create master flat.
		directoryString = status.getProperty("o.file.fits.path");
		if(directoryString.endsWith(System.getProperty("file.separator")) == false)
			directoryString = directoryString.concat(System.getProperty("file.separator"));
		if(makeMasterFlat(twilightCalibrateCommand,twilightCalibrateDone,directoryString) == false)
			return twilightCalibrateDone;
	// return done
		twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		twilightCalibrateDone.setErrorString("");
		twilightCalibrateDone.setSuccessful(true);
		return twilightCalibrateDone;
	}

	/**
	 * Method to set time of night flag.
	 * @see #TIME_OF_NIGHT_UNKNOWN
	 * @see #TIME_OF_NIGHT_SUNRISE
	 * @see #TIME_OF_NIGHT_SUNSET
	 * @see #timeOfNight
	 */
	protected void setTimeOfNight()
	{
		Calendar calendar = null;
		int hour;

		timeOfNight = TIME_OF_NIGHT_UNKNOWN;
	// get Instance initialises the calendar to the current time.
		calendar = Calendar.getInstance();
	// the hour returned using HOUR of DAY is between 0 and 23
		hour = calendar.get(Calendar.HOUR_OF_DAY);
		if(hour < 12)
			timeOfNight = TIME_OF_NIGHT_SUNRISE;
		else
			timeOfNight = TIME_OF_NIGHT_SUNSET;
	}

	/**
	 * Method to load twilight calibration configuration data from the O Properties file.
	 * The following configuration properties are retrieved:
	 * <ul>
	 * <li>frame overhead
	 * <li>minimum exposure length
	 * <li>maximum exposure length
	 * <li>temporary FITS filename
	 * <li>saved state filename
	 * <li>minimum mean counts (for each binning factor)
	 * <li>best mean counts (for each binning factor)
	 * <li>maximum mean counts (for each binning factor)
	 * </ul>
	 * The following methods are then called to load more calibration data:
	 * <ul>
	 * <li><b>loadCalibrationList</b>
	 * <li><b>loadOffsetList</b>
	 * <li><b>loadState</b>
	 * </ul>
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if the method succeeds, false if an error occurs.
	 * 	If false is returned the error data in twilightCalibrateDone is filled in.
	 * @see #loadCalibrationList
	 * @see #loadState
	 * @see #loadOffsetList
	 * @see #frameOverhead
	 * @see #minExposureLength
	 * @see #maxExposureLength
	 * @see #temporaryFITSFilename
	 * @see #stateFilename
	 * @see #minMeanCounts
	 * @see #bestMeanCounts
	 * @see #maxMeanCounts
	 * @see #timeOfNight
	 * @see #LIST_KEY_SUNSET_STRING
	 * @see #LIST_KEY_SUNRISE_STRING
	 * @see #LIST_KEY_STRING
	 */
	protected boolean loadProperties(TWILIGHT_CALIBRATE twilightCalibrateCommand,
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone)
	{
		String timeOfNightString = null;
		String propertyName = null;

		if(timeOfNight == TIME_OF_NIGHT_SUNSET)
			timeOfNightString = LIST_KEY_SUNSET_STRING;
		else
			timeOfNightString = LIST_KEY_SUNRISE_STRING;
		try
		{
		// frame overhead
			propertyName = LIST_KEY_STRING+"frame_overhead";
			frameOverhead = status.getPropertyInteger(propertyName);
		// minimum exposure length
			propertyName = LIST_KEY_STRING+"min_exposure_time";
			minExposureLength = status.getPropertyInteger(propertyName);
		// maximum exposure length
			propertyName = LIST_KEY_STRING+"max_exposure_time";
			maxExposureLength = status.getPropertyInteger(propertyName);
		// temporary FITS filename
			propertyName = LIST_KEY_STRING+"file.tmp";
			temporaryFITSFilename = status.getProperty(propertyName);
		// saved state filename
			propertyName = LIST_KEY_STRING+"state_filename";
			stateFilename = status.getProperty(propertyName);
			// binning goes from 1..BIN_COUNT-1
			for(int binIndex = 1; binIndex < BIN_COUNT; binIndex ++)
			{
				// minimum mean counts
				propertyName = LIST_KEY_STRING+"mean_counts.min."+binIndex;
				minMeanCounts[binIndex] = status.getPropertyInteger(propertyName);
				// best mean counts
				propertyName = LIST_KEY_STRING+"mean_counts.best."+binIndex;
				bestMeanCounts[binIndex] = status.getPropertyInteger(propertyName);
				// maximum mean counts
				propertyName = LIST_KEY_STRING+"mean_counts.max."+binIndex;
				maxMeanCounts[binIndex] = status.getPropertyInteger(propertyName);
			}// end for on binIndex
		}
		catch (Exception e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":loadProperties:Failed to get property:"+propertyName);
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2300);
			twilightCalibrateDone.setErrorString(errorString);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
		if(loadCalibrationList(twilightCalibrateCommand,twilightCalibrateDone) == false)
			return false;
		if(loadOffsetList(twilightCalibrateCommand,twilightCalibrateDone) == false)
			return false;
		if(loadState(twilightCalibrateCommand,twilightCalibrateDone) == false)
			return false;
		return true;
	}

	/**
	 * Method to load a list of calibrations to do. The list used depends on whether timeOfNight is set to
	 * sunrise or sunset.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. If false is returned the error
	 * 	data in twilightCalibrateDone is filled in.
	 * @see #calibrationList
	 * @see #getFilterSensitivity
	 * @see #LIST_KEY_STRING
	 * @see #LIST_KEY_CALIBRATION_STRING
	 * @see #LIST_KEY_SLIDE_STRING
	 * @see #LIST_KEY_LOWER_STRING
	 * @see #LIST_KEY_UPPER_STRING
	 * @see #LIST_KEY_FILTER_STRING
	 * @see #LIST_KEY_BIN_STRING
	 * @see #LIST_KEY_AMPLIFIER_STRING
	 * @see #LIST_KEY_FREQUENCY_STRING
	 * @see #LIST_KEY_SUNSET_STRING
	 * @see #LIST_KEY_SUNRISE_STRING
	 * @see #timeOfNight
	 */
	protected boolean loadCalibrationList(TWILIGHT_CALIBRATE twilightCalibrateCommand,
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone)
	{
		TWILIGHT_CALIBRATECalibration calibration = null;
		String timeOfNightString = null;
		String filter = null;
		String lowerSlide,upperSlide;
		int index,bin;
		long frequency;
		double filterSensitivity;
		boolean done,useWindowAmplifier;

		index = 0;
		done = false;
		calibrationList = new Vector();
		if(timeOfNight == TIME_OF_NIGHT_SUNSET)
			timeOfNightString = LIST_KEY_SUNSET_STRING;
		else
			timeOfNightString = LIST_KEY_SUNRISE_STRING;
		while(done == false)
		{
			filter = status.getProperty(LIST_KEY_STRING+LIST_KEY_CALIBRATION_STRING+
							timeOfNightString+index+LIST_KEY_FILTER_STRING);
			if(filter != null)
			{
			// create calibration instance
				calibration = new TWILIGHT_CALIBRATECalibration();
			// get parameters from properties
				try
				{
					frequency = status.getPropertyLong(LIST_KEY_STRING+LIST_KEY_CALIBRATION_STRING+
							timeOfNightString+index+LIST_KEY_FREQUENCY_STRING);
					bin = status.getPropertyInteger(LIST_KEY_STRING+LIST_KEY_CALIBRATION_STRING+
							timeOfNightString+index+LIST_KEY_BIN_STRING);
					useWindowAmplifier  = status.getPropertyBoolean(LIST_KEY_STRING+
											LIST_KEY_CALIBRATION_STRING+
											timeOfNightString+index+
											LIST_KEY_AMPLIFIER_STRING);
					lowerSlide = status.getProperty(LIST_KEY_STRING+
									LIST_KEY_CALIBRATION_STRING+
									timeOfNightString+index+
									LIST_KEY_SLIDE_STRING+
									LIST_KEY_LOWER_STRING);
					upperSlide = status.getProperty(LIST_KEY_STRING+
									LIST_KEY_CALIBRATION_STRING+
									timeOfNightString+index+
									LIST_KEY_SLIDE_STRING+
									LIST_KEY_UPPER_STRING);
				}
				catch(Exception e)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
						":loadCalibrationList:Failed at index "+index+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2301);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// set calibration data
				try
				{
					calibration.setBin(bin);
					calibration.setUseWindowAmplifier(useWindowAmplifier);
					calibration.setLowerSlide(lowerSlide);
					calibration.setUpperSlide(upperSlide);
					calibration.setFilter(filter);
					calibration.setFrequency(frequency);
				}
				catch(Exception e)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
						":loadCalibrationList:Failed to set calibration data at index "+index+
						":bin:"+bin+":use window amplifier:"+useWindowAmplifier+
						":filter:"+filter+":frequency:"+frequency+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2302);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// get filter sensitivity, and set calibration sensitivities
				try
				{
					filterSensitivity = getFilterSensitivity(filter);
					calibration.setFilterSensitivity(filterSensitivity);
				}
				catch(Exception e)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
						":loadCalibrationList:Failed to set filter sensitivities at index "+
						index+":bin:"+bin+
						":filter:"+filter+":frequency:"+frequency+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2303);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// add calibration instance to list
				calibrationList.add(calibration);
			// log
				o.log(Logging.VERBOSITY_VERBOSE,
					"Command:"+twilightCalibrateCommand.getClass().getName()+
					":Loaded calibration:"+index+
					":bin:"+calibration.getBin()+
					":use window amplifier:"+calibration.useWindowAmplifier()+
					":upper slide:"+calibration.getUpperSlide()+
					":lower slide:"+calibration.getLowerSlide()+
					":filter:"+calibration.getFilter()+
					":frequency:"+calibration.getFrequency()+".");
			}
			else
				done = true;
			index++;
		}
		return true;
	}

	/**
	 * Method to get the relative filter sensitivity of a filter.
	 * @param filterType The type name of the filter to find the sensitivity for.
	 * @return A double is returned, which is the filter sensitivity relative to no filter,
	 * 	and should be in the range 0 to 1.
	 * @exception IllegalArgumentException Thrown if the filter sensitivity returned from the
	 * 	property is out of the range 0..1.
	 * @exception NumberFormatException  Thrown if the filter sensitivity property is not a valid double.
	 */
	protected double getFilterSensitivity(String filterType) throws NumberFormatException, IllegalArgumentException
	{
		double filterSensitivity;

		filterSensitivity = status.getPropertyDouble(LIST_KEY_STRING+LIST_KEY_FILTER_SENSITIVITY_STRING+
								filterType);
		if((filterSensitivity < 0.0)||(filterSensitivity > 1.0))
		{
			throw new IllegalArgumentException(this.getClass().getName()+
				":getFilterSensitivity failed:filter type "+filterType+
				" has filter sensitivity "+filterSensitivity+", which is out of range.");
		}
		return filterSensitivity;
	}

	/**
	 * Method to initialse twilightCalibrateState.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. If false is returned the error
	 * 	data in twilightCalibrateDone is filled in.
	 * @see #twilightCalibrateState
	 * @see #stateFilename
	 */
	protected boolean loadState(TWILIGHT_CALIBRATE twilightCalibrateCommand,
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone)
	{
	// initialise and load twilightCalibrateState instance
		twilightCalibrateState = new TWILIGHT_CALIBRATESavedState();
		try
		{
			twilightCalibrateState.load(stateFilename);
		}
		catch (Exception e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":loadState:Failed to load state filename:"+stateFilename);
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2304);
			twilightCalibrateDone.setErrorString(errorString);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to load a list of telescope RA/DEC offsets. These are used to offset the telescope
	 * between frames of the same calibration.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. If false is returned the error
	 * 	data in twilightCalibrateDone is filled in.
	 * @see #offsetList
	 * @see #LIST_KEY_OFFSET_STRING
	 * @see #LIST_KEY_RA_STRING
	 * @see #LIST_KEY_DEC_STRING
	 */
	protected boolean loadOffsetList(TWILIGHT_CALIBRATE twilightCalibrateCommand,
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone)
	{

		TWILIGHT_CALIBRATEOffset offset = null;
		String testString = null;
		int index;
		double raOffset,decOffset;
		boolean done;

		index = 0;
		done = false;
		offsetList = new Vector();
		while(done == false)
		{
			testString = status.getProperty(LIST_KEY_STRING+LIST_KEY_OFFSET_STRING+
							index+LIST_KEY_RA_STRING);
			if((testString != null))
			{
			// create offset
				offset = new TWILIGHT_CALIBRATEOffset();
			// get parameters from properties
				try
				{
					raOffset = status.getPropertyDouble(LIST_KEY_STRING+LIST_KEY_OFFSET_STRING+
							index+LIST_KEY_RA_STRING);
					decOffset = status.getPropertyDouble(LIST_KEY_STRING+LIST_KEY_OFFSET_STRING+
							index+LIST_KEY_DEC_STRING);
				}
				catch(Exception e)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
						":loadOffsetList:Failed at index "+index+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2305);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// set offset data
				try
				{
					offset.setRAOffset((float)raOffset);
					offset.setDECOffset((float)decOffset);
				}
				catch(Exception e)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
						":loadOffsetList:Failed to set data at index "+index+
						":RA offset:"+raOffset+":DEC offset:"+decOffset+".");
					o.error(this.getClass().getName()+":"+errorString,e);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2306);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// add offset instance to list
				offsetList.add(offset);
			// log
				o.log(Logging.VERBOSITY_VERBOSE,
					"Command:"+twilightCalibrateCommand.getClass().getName()+
					":Loaded offset "+index+
					":RA Offset:"+offset.getRAOffset()+
					":DEC Offset:"+offset.getDECOffset()+".");
			}
			else
				done = true;
			index++;
		}
		return true;
	}

	/**
	 * This method matches the saved state to the calibration list to set the last time
	 * each calibration was completed.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @return The method returns true if it succeeds, false if it fails. Currently always returns true.
	 * @see #calibrationList
	 * @see #twilightCalibrateState
	 */
	protected boolean addSavedStateToCalibration(TWILIGHT_CALIBRATE twilightCalibrateCommand,
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone)
	{
		TWILIGHT_CALIBRATECalibration calibration = null;
		String filter = null;
		String lowerSlide,upperSlide;
		int bin;
		long lastTime;
		boolean useWindowAmplifier;

		for(int i = 0; i< calibrationList.size(); i++)
		{
			calibration = (TWILIGHT_CALIBRATECalibration)(calibrationList.get(i));
			bin = calibration.getBin();
			useWindowAmplifier = calibration.useWindowAmplifier();
			lowerSlide = calibration.getLowerSlide();
			upperSlide = calibration.getUpperSlide();
			filter = calibration.getFilter();
			lastTime = twilightCalibrateState.getLastTime(bin,useWindowAmplifier,
								      lowerSlide,upperSlide,filter);
			calibration.setLastTime(lastTime);
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+":Calibration:"+
			      "bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":upper slide:"+calibration.getUpperSlide()+
			      ":lower slide:"+calibration.getLowerSlide()+
			      ":filter:"+calibration.getFilter()+
			      ":frequency:"+calibration.getFrequency()+
			      " now has last time set to:"+lastTime+".");
		}
		return true;
	}

	/**
	 * This method does the specified calibration.
	 * <ul>
	 * <li>The relevant data is retrieved from the calibration parameter.
	 * <li>If we did this calibration more recently than frequency, log and return.
	 * <li>The start exposure length is recalculated, by dividing by the last relative sensitivity used
	 * 	(to get the exposure length as if though a clear filter), and then dividing by the 
	 * 	new relative filter sensitivity (to increase the exposure length).
	 * <li>The start exposure length is recalculated to take account of differences from the last binning
	 *     to the new binning.
	 * <li>If the new exposure length is too short, it is reset to the minimum exposure length.
	 * <li>If the new exposure length is too long, the fact is logged and the method returns.
	 * <li><b>sendBasicAck</b> is called to stop the client timing out before the config is completed.
	 * <li><b>doConfig</b> is called for the relevant binning factor/slides/filter set to be setup.
	 * <li><b>sendBasicAck</b> is called to stop the client timing out before the first frame is completed.
	 * <li><b>doOffsetList</b> is called to go through the telescope RA/DEC offsets and take frames at
	 * 	each offset.
	 * <li>If the calibration suceeded, the saved state's last time is updated to now, and the state saved.
	 * </ul>
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @param calibration The calibration to do.
	 * @return The method returns true if the calibration was done successfully, false if an error occured.
	 * @see #doConfig
	 * @see #doOffsetList
	 * @see #sendBasicAck
	 * @see #stateFilename
	 * @see #lastFilterSensitivity
	 * @see #lastBin
	 * @see #calibrationFrameCount
	 */
	protected boolean doCalibration(TWILIGHT_CALIBRATE twilightCalibrateCommand,
			TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,TWILIGHT_CALIBRATECalibration calibration)
	{
		String filter = null;
		String lowerSlide,upperSlide;
		int bin;
		long lastTime,frequency;
		long now;
		double filterSensitivity;
		boolean useWindowAmplifier;

		o.log(Logging.VERBOSITY_VERBOSE,
		      "Command:"+twilightCalibrateCommand.getClass().getName()+
		      ":doCalibrate:"+"bin:"+calibration.getBin()+
		      ":use window amplifier:"+calibration.useWindowAmplifier()+
		      ":upper slide:"+calibration.getUpperSlide()+
		      ":lower slide:"+calibration.getLowerSlide()+
		      ":filter:"+calibration.getFilter()+
		      ":frequency:"+calibration.getFrequency()+
		      ":filter sensitivity:"+calibration.getFilterSensitivity()+
		      ":last time:"+calibration.getLastTime()+" Started.");
	// get copy of calibration data
		bin = calibration.getBin();
		useWindowAmplifier = calibration.useWindowAmplifier();
		upperSlide = calibration.getUpperSlide();
		lowerSlide = calibration.getLowerSlide();
		filter = calibration.getFilter();
		frequency = calibration.getFrequency();
		lastTime = calibration.getLastTime();
		filterSensitivity = calibration.getFilterSensitivity();
	// get current time
		now = System.currentTimeMillis();
	// if we did the calibration more recently than frequency, log and return
		if(now-lastTime < frequency)
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+
			      ":doCalibrate:"+"bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":upper slide:"+calibration.getUpperSlide()+
			      ":lower slide:"+calibration.getLowerSlide()+
			      ":filter:"+calibration.getFilter()+
			      ":frequency:"+calibration.getFrequency()+
			      ":last time:"+lastTime+
			      "NOT DONE: too soon since last completed:"+now+" - "+lastTime+" < "+frequency+".");
			return true;
		}
	// recalculate the exposure length
		o.log(Logging.VERBOSITY_VERBOSE,
		      "Command:"+twilightCalibrateCommand.getClass().getName()+
		      ":doCalibrate:exposureLength currently:"+exposureLength);
		exposureLength = (int)((((double)exposureLength)*lastFilterSensitivity)/filterSensitivity);
		o.log(Logging.VERBOSITY_VERBOSE,
		      "Command:"+twilightCalibrateCommand.getClass().getName()+
		      ":doCalibrate:exposureLength after multiplication through by last filter sensitivity:"+
		      lastFilterSensitivity+"/ filter senisitivity:"+filterSensitivity+" =:"+exposureLength);
		exposureLength = (exposureLength*(lastBin*lastBin))/(bin*bin);
		o.log(Logging.VERBOSITY_VERBOSE,
		      "Command:"+twilightCalibrateCommand.getClass().getName()+
		      ":doCalibrate:exposureLength after multiplication through by last bin:"+
		      lastBin+" (squared) / bin:"+bin+" (squared) =:"+exposureLength);
	// if we are going to do this calibration, reset the last filter sensitivity for next time
	// We need to think about when to do this when the new exposure length means we DON'T do the calibration
		lastFilterSensitivity = filterSensitivity;
		lastBin = bin;
	// check exposure time
		if(exposureLength < minExposureLength)
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+
			      ":doCalibrate:"+"bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":upper slide:"+calibration.getUpperSlide()+
			      ":lower slide:"+calibration.getLowerSlide()+
			      ":filter:"+calibration.getFilter()+
			      ":frequency:"+calibration.getFrequency()+
			      ":last time:"+lastTime+
			      " calculated exposure length:"+exposureLength+
			      " too short, using minimum:"+minExposureLength+".");
			exposureLength = minExposureLength;
		}
		if(exposureLength > maxExposureLength)
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+
			      ":doCalibrate:"+"bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":upper slide:"+calibration.getUpperSlide()+
			      ":lower slide:"+calibration.getLowerSlide()+
			      ":filter:"+calibration.getFilter()+
			      ":frequency:"+calibration.getFrequency()+
			      ":last time:"+lastTime+
			      " calculated exposure length:"+exposureLength+
			      " too long, using maximum:"+maxExposureLength+".");
			exposureLength = maxExposureLength;
		}
		if((now+exposureLength+frameOverhead) > 
			(implementationStartTime+twilightCalibrateCommand.getTimeToComplete()))
		{
			o.log(Logging.VERBOSITY_VERBOSE,
				"Command:"+twilightCalibrateCommand.getClass().getName()+
				":doCalibrate:Ran out of time to complete:"+
				"((now:"+now+
				")+(exposureLength:"+exposureLength+
				")+(frameOverhead:"+frameOverhead+")) > "+
				"((implementationStartTime:"+implementationStartTime+
				")+(timeToComplete:"+twilightCalibrateCommand.getTimeToComplete()+")).");
			return true;
		}
	// send an ack before the frame, so the client doesn't time out during configuration
		if(sendBasicAck(twilightCalibrateCommand,twilightCalibrateDone,frameOverhead) == false)
			return false;
	// configure slide/filter/CCD camera
		if(doConfig(twilightCalibrateCommand,twilightCalibrateDone,bin,useWindowAmplifier,
			    lowerSlide,upperSlide,filter) == false)
			return false;
	// send an ack before the frame, so the client doesn't time out during the first exposure
		if(sendBasicAck(twilightCalibrateCommand,twilightCalibrateDone,exposureLength+frameOverhead) == false)
			return false;
	// do the frames with this configuration
		calibrationFrameCount = 0;
		if(doOffsetList(twilightCalibrateCommand,twilightCalibrateDone,bin,
				lowerSlide,upperSlide,filter) == false)
			return false;
	// update state, if we completed the whole calibration.
		if(calibrationFrameCount == offsetList.size())
		{
			twilightCalibrateState.setLastTime(bin,useWindowAmplifier,lowerSlide,upperSlide,filter);
			try
			{
				twilightCalibrateState.save(stateFilename);
			}
			catch(IOException e)
			{
				String errorString = new String(twilightCalibrateCommand.getId()+
					":doCalibration:Failed to save state filename:"+stateFilename);
				o.error(this.getClass().getName()+":"+errorString,e);
				twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2307);
				twilightCalibrateDone.setErrorString(errorString);
				twilightCalibrateDone.setSuccessful(false);
				return false;
			}
			lastTime = twilightCalibrateState.getLastTime(bin,useWindowAmplifier,
								      lowerSlide,upperSlide,filter);
			calibration.setLastTime(lastTime);
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+
			      ":doCalibrate:Calibration successfully completed:"+
			      "bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":upper slide:"+calibration.getUpperSlide()+
			      ":lower slide:"+calibration.getLowerSlide()+
			      ":filter:"+calibration.getFilter()+".");
		}// end if done calibration
		else
		{
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+
			      ":doCalibrate:Calibration NOT completed:"+
			      "bin:"+calibration.getBin()+
			      ":use window amplifier:"+calibration.useWindowAmplifier()+
			      ":upper slide:"+calibration.getUpperSlide()+
			      ":lower slide:"+calibration.getLowerSlide()+
			      ":filter:"+calibration.getFilter()+".");
		}
		return true;
	}

	/**
	 * Method to setup the CCD configuration with the specified binning factor.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @param bin The binning factor to use.
	 * @param useWindowAmplifier Boolean specifying whether to use the default amplifier (false), or the
	 *                           amplifier used for windowing readouts (true).
	 * @param lowerSlide The position the lower filter slide should be in.
	 * @param upperSlide The position the upper filter slide should be in.
	 * @param filter The type of filter to use.
	 * @return The method returns true if the calibration was done successfully, false if an error occured.
	 * @see FITSImplementation#setFocusOffset
	 * @see FITSImplementation#beamSteer
	 * @see OStatus#getNumberColumns
	 * @see OStatus#getNumberRows
	 */
	protected boolean doConfig(TWILIGHT_CALIBRATE twilightCalibrateCommand,
				   TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,int bin,boolean useWindowAmplifier,
				   String lowerSlide,String upperSlide,String filter)
	{
		CCDLibrarySetupWindow windowList[] = new CCDLibrarySetupWindow[CCDLibrary.SETUP_WINDOW_COUNT];
		int numberColumns,numberRows,amplifier;
		int filterWheelPosition = -1;
		boolean filterWheelEnable;

	// load other required config for dimension configuration from the O properties file.
		try
		{
			numberColumns = status.getNumberColumns(bin);
			numberRows = status.getNumberRows(bin);
			amplifier = getAmplifier(useWindowAmplifier);
			filterWheelEnable = status.getPropertyBoolean("o.config.filter_wheel.enable");
			filterWheelPosition = status.getFilterWheelPosition(filter);
		}
	// CCDLibraryFormatException,IllegalArgumentException,NumberFormatException.
		catch(Exception e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":doConfig:Failed to get config properties:");
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2308);
			twilightCalibrateDone.setErrorString(errorString);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
	// test abort
		if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
			return false;
	// set up blank windows
		for(int i = 0; i < CCDLibrary.SETUP_WINDOW_COUNT; i++)
		{
			windowList[i] = new CCDLibrarySetupWindow(-1,-1,-1,-1);
		}
	// test abort
		if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
			return false;
	// send configuration to the SDSU controller
		try
		{
			ccd.setupDimensions(numberColumns,numberRows,bin,bin,amplifier,0,windowList);
			if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
				return false;
			if(filterWheelEnable)
			{
				ccd.filterWheelMove(filterWheelPosition);
			}
			else
			{
				o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
					":doConfig:Filter wheels not enabled:Filter wheels NOT moved.");
			}
			// send BEAM_STEER command to BSS to position dichroics.
		        beamSteer(twilightCalibrateCommand.getId(),lowerSlide,upperSlide);
		}
		catch(Exception e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":doConfig:Failed to configure CCD/filter wheel/BSS:");
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2309);
			twilightCalibrateDone.setErrorString(errorString);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
	// test abort
		if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
			return false;
	// Issue ISS OFFSET_FOCUS_CONTROL commmand based on the optical thickness of the filter(s)
		if(filterWheelEnable)
		{
			try
			{
				setFocusOffset(twilightCalibrateCommand.getId(),filter);
			}
			catch(Exception e)
			{
				String errorString = new String(twilightCalibrateCommand.getId()+
								"doConfig:setFocusOffset failed:");
				o.error(this.getClass().getName()+":"+errorString,e);
				twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2322);
				twilightCalibrateDone.setErrorString(errorString+e);
				twilightCalibrateDone.setSuccessful(false);
				return false;
			}
		}
		else
		{
			o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
			    ":doConfig:Filter wheels not enabled:Focus offset NOT set.");
		}
	// Increment unique config ID.
	// This is queried when saving FITS headers to get the CONFIGID value.
		try
		{
			status.incConfigId();
		}
		catch(Exception e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":doConfig:Incrementing configuration ID failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2310);
			twilightCalibrateDone.setErrorString(errorString+e);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
	// Store name of configuration used in status object.
	// This is queried when saving FITS headers to get the CONFNAME value.
		status.setConfigName("TWILIGHT_CALIBRATION:"+twilightCalibrateCommand.getId()+
				     ":"+bin+":"+useWindowAmplifier+":"+upperSlide+":"+lowerSlide+":"+filter);
		return true;
	}

	/**
	 * This method goes through the offset list for the configured calibration. It trys to
	 * get a frame for each offset.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @param bin The binning factor we are doing the exposure at, used to select the correct min/best/max counts.
	 * @param lowerSlide The position the lower filter slide should be in. Passed through for logging purposes.
	 * @param upperSlide The position the upper filter slide should be in. Passed through for logging purposes.
	 * @param filter The type of filter to use. Passed through for logging purposes.
	 * @return The method returns true when the offset list is terminated, false if an error occured.
	 * @see #offsetList
	 * @see #doFrame
	 * @see O#sendISSCommand
	 * @see ngat.message.ISS_INST.OFFSET_RA_DEC
	 */
	protected boolean doOffsetList(TWILIGHT_CALIBRATE twilightCalibrateCommand,
				       TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,int bin,
				       String lowerSlide,String upperSlide,String filter)
	{
		TWILIGHT_CALIBRATEOffset offset = null;
		OFFSET_RA_DEC offsetRaDecCommand = null;
		INST_TO_ISS_DONE instToISSDone = null;
		int offsetListIndex;

		doneOffset = false;
		offsetListIndex = 0;
		while((doneOffset == false) && (offsetListIndex < offsetList.size()))
		{
		// get offset
			offset = (TWILIGHT_CALIBRATEOffset)(offsetList.get(offsetListIndex));
		// log telescope offset
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getClass().getName()+
			      ":Attempting telescope position offset index:"+offsetListIndex+
			      ":RA:"+offset.getRAOffset()+
			      ":DEC:"+offset.getDECOffset()+".");
		// tell telescope of offset RA and DEC
			offsetRaDecCommand = new OFFSET_RA_DEC(twilightCalibrateCommand.getId());
			offsetRaDecCommand.setRaOffset(offset.getRAOffset());
			offsetRaDecCommand.setDecOffset(offset.getDECOffset());
			instToISSDone = o.sendISSCommand(offsetRaDecCommand,serverConnectionThread);
			if(instToISSDone.getSuccessful() == false)
			{
				String errorString = null;

				errorString = new String("Offset Ra Dec failed:ra = "+offset.getRAOffset()+
					", dec = "+offset.getDECOffset()+":"+instToISSDone.getErrorString());
				o.error(errorString);
				twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2312);
				twilightCalibrateDone.setErrorString(this.getClass().getName()+
									":doOffsetList:"+errorString);
				twilightCalibrateDone.setSuccessful(false);
				return false;
			}
		// do exposure at this offset
			if(doFrame(twilightCalibrateCommand,twilightCalibrateDone,bin,lowerSlide,upperSlide,
				   filter) == false)
				return false;
			offsetListIndex++;
		}// end for on offset list
		return true;
	}

	/**
	 * The method that does a calibration frame with the current configuration. The following is performed 
	 * in a while loop, that is terminated when a good frame has been taken.
	 * <ul>
	 * <li>The pause and resume times are cleared, and the FITS headers setup from the current configuration.
	 * <li>Some FITS headers are got from the ISS and BSS.
	 * <li><b>testAbort</b> is called to see if this command implementation has been aborted.
	 * <li>The FITS headers are saved using <b>saveFitsHeaders</b>, to the temporary FITS filename.
	 * <li>The frame is taken, using libccd's <b>expose</b> method.
	 * <li>The FITS file lock created in <b>saveFitsHeaders</b> is removed with a call to <b>unLockFile</b>.
	 * <li>The last exposure length variable is updated.
	 * <li>An instance of TWILIGHT_CALIBRATE_ACK is sent back to the client using <b>sendTwilightCalibrateAck</b>.
	 * <li><b>testAbort</b> is called to see if this command implementation has been aborted.
	 * <li><b>reduceCalibrate</b> is called to pass the frame to the Real Time Data Pipeline for processing.
	 * <li>The frame state is derived from the returned mean counts.
	 * <li>If the frame state was good, the raw frame and DpRt reduced (if different) are renamed into
	 * 	the standard FITS filename using oFilename, by incrementing the run number.
	 * <li><b>testAbort</b> is called to see if this command implementation has been aborted.
	 * <li>The exposure Length is modified by multiplying by the ratio of best mean counts over mean counts.
	 * <li>If the calculated exposure length is out of the acceptable range,
	 *     but the last attempt was not at the range limit or the last attempt was at the limit but acceptable, 
	 *     reset the exposure length to the limit.
	 * <li>An instance of TWILIGHT_CALIBRATE_DP_ACK is sent back to the client using
	 * 	<b>sendTwilightCalibrateDpAck</b>.
	 * <li>We check to see if the loop should be terminated:
	 * 	<ul>
	 * 	<li>If the frame state is OK, the loop is exited and the method stopped.
	 * 	<li>If the next exposure will take longer than the time remaining, we stop the frame loop,
	 * 		offset loop and calibration loop (i.e. the TWILIGHT_CALIBRATE command is terminated).
	 * 	<li>If the next exposure will take longer than the maximum exposure length, we stop the frame loop and
	 * 		offset loop. (i.e. we try the next calibration).
	 * 	<li>If the next exposure will be shorter than the minimum exposure length, we stop the frame loop and
	 * 		offset loop. (i.e. we try the next calibration).
	 * 	</ul>
	 * </ul>
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @param bin The binning factor we are doing the exposure at, used to select the correct min/best/max counts.
	 * @param lowerSlide The position the lower filter slide should be in. Passed through for logging purposes.
	 * @param upperSlide The position the upper filter slide should be in. Passed through for logging purposes.
	 * @param filter The type of filter to use. Passed through for logging purposes.
	 * @return The method returns true if no errors occured, false if an error occured.
	 * @see FITSImplementation#testAbort
	 * @see FITSImplementation#setFitsHeaders
	 * @see FITSImplementation#getFitsHeadersFromISS
	 * @see FITSImplementation#getFitsHeadersFromBSS
	 * @see FITSImplementation#saveFitsHeaders
	 * @see FITSImplementation#unLockFile
	 * @see FITSImplementation#oFilename
	 * @see FITSImplementation#ccd
	 * @see ngat.o.ccd.CCDLibrary#expose
	 * @see #sendTwilightCalibrateAck
	 * @see #sendTwilightCalibrateDpAck
	 * @see CALIBRATEImplementation#reduceCalibrate
	 * @see #exposureLength
	 * @see #lastExposureLength
	 * @see #minExposureLength
	 * @see #maxExposureLength
	 * @see #frameOverhead
	 * @see #temporaryFITSFilename
	 * @see #implementationStartTime
	 * @see #FRAME_STATE_OVEREXPOSED
	 * @see #FRAME_STATE_UNDEREXPOSED
	 * @see #FRAME_STATE_OK
	 * @see #FRAME_STATE_NAME_LIST
	 */
	protected boolean doFrame(TWILIGHT_CALIBRATE twilightCalibrateCommand,
				  TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,int bin,
				  String lowerSlide,String upperSlide,String filter)
	{
		File temporaryFile = null;
		File newFile = null;
		String filename = null;
		String reducedFilename = null;
		long now;
		int frameState;
		float meanCounts,countsDifference;
		boolean doneFrame;

		doneFrame = false;
		while(doneFrame == false)
		{
		// Clear the pause and resume times.
			status.clearPauseResumeTimes();
		// delete old temporary file.
			temporaryFile = new File(temporaryFITSFilename);
			if(temporaryFile.exists())
				temporaryFile.delete();
		// setup fits headers
			clearFitsHeaders();
			if(setFitsHeaders(twilightCalibrateCommand,twilightCalibrateDone,
				FitsHeaderDefaults.OBSTYPE_VALUE_SKY_FLAT,exposureLength) == false)
				return false;
			if(getFitsHeadersFromISS(twilightCalibrateCommand,twilightCalibrateDone) == false)
				return false;
			if(getFitsHeadersFromBSS(twilightCalibrateCommand,twilightCalibrateDone) == false)
				return false;
			if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
				return false;
			if(saveFitsHeaders(twilightCalibrateCommand,twilightCalibrateDone,
					   temporaryFITSFilename) == false)
			{
				unLockFile(twilightCalibrateCommand,twilightCalibrateDone,temporaryFITSFilename);
				return false;
			}
			status.setExposureFilename(temporaryFITSFilename);
		// log exposure attempt
			o.log(Logging.VERBOSITY_VERBOSE,"Command:"+twilightCalibrateCommand.getId()+
			      ":doFrame:"+"bin:"+bin+
			      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
			      ":Attempting exposure: length:"+exposureLength+".");
		// do exposure
			try
			{
				ccd.expose(true,-1,exposureLength,temporaryFITSFilename);
			}
			catch(CCDLibraryNativeException e)
			{
				String errorString = new String(twilightCalibrateCommand.getId()+
					":doFrame:Doing frame of length "+exposureLength+" failed:");
				o.error(this.getClass().getName()+":"+errorString,e);
				twilightCalibrateDone.setFilename(temporaryFITSFilename);
				twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2313);
				twilightCalibrateDone.setErrorString(errorString+e);
				twilightCalibrateDone.setSuccessful(false);
				return false;
			}
			finally
			{
				if(unLockFile(twilightCalibrateCommand,twilightCalibrateDone,
					      temporaryFITSFilename) == false)
					return false;
			}
		// set last exposure length
			lastExposureLength = exposureLength;
		// send with filename back to client
		// time to complete is reduction time, we will send another ACK after reduceCalibrate
			if(sendTwilightCalibrateAck(twilightCalibrateCommand,twilightCalibrateDone,frameOverhead,
				temporaryFITSFilename) == false)
				return false; 
		// Test abort status.
			if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
				return false;
		// Call pipeline to reduce data.
			if(reduceCalibrate(twilightCalibrateCommand,twilightCalibrateDone,
				temporaryFITSFilename) == false)
				return false;
		// Test abort status.
			if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
				return false;
		// log reduction
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getId()+
			      ":doFrame:"+"bin:"+bin+
			      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
			      ":Exposure reduction:length "+exposureLength+
			      ":filename:"+twilightCalibrateDone.getFilename()+
			      ":mean counts:"+twilightCalibrateDone.getMeanCounts()+
			      ":peak counts:"+twilightCalibrateDone.getPeakCounts()+".");
		// get reduced filename from done
			reducedFilename = twilightCalibrateDone.getFilename();
		// get mean counts and set frame state.
			meanCounts = twilightCalibrateDone.getMeanCounts();
			// range check mean counts, if they are negative we are saturated due to the way
			// the CCD deals with saturation.
			if(meanCounts < 0)
			{
				o.log(Logging.VERBOSITY_VERBOSE,
				      "Command:"+twilightCalibrateCommand.getId()+
				      ":doFrame:"+"bin:"+bin+
				      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
				      ":Exposure reduction:length "+exposureLength+
				      ":filename:"+twilightCalibrateDone.getFilename()+
				      ":mean counts:"+twilightCalibrateDone.getMeanCounts()+
				      ":Mean counts are negative, exposure is probably saturated, "+
				      "faking mean counts to 65000.");
				meanCounts = 65000;
			}
			if(meanCounts > maxMeanCounts[bin])
				frameState = FRAME_STATE_OVEREXPOSED;
			else if(meanCounts < minMeanCounts[bin])
				frameState = FRAME_STATE_UNDEREXPOSED;
			else
				frameState = FRAME_STATE_OK;
		// log frame state
			o.log(Logging.VERBOSITY_VERBOSE,
			      "Command:"+twilightCalibrateCommand.getId()+
			      ":doFrame:"+"bin:"+bin+
			      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
			      ":Exposure frame state:length:"+exposureLength+
			      ":mean counts:"+meanCounts+
			      ":peak counts:"+twilightCalibrateDone.getPeakCounts()+
			      ":frame state:"+FRAME_STATE_NAME_LIST[frameState]+".");
		// if the frame was good, rename it
			if(frameState == FRAME_STATE_OK)
			{
			// raw frame
				temporaryFile = new File(temporaryFITSFilename);
			// does the temprary file exist?
				if(temporaryFile.exists() == false)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
								":File does not exist:"+temporaryFITSFilename);

					o.error(this.getClass().getName()+
						":doFrame:"+errorString);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2314);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// get a filename to store frame in
				oFilename.nextRunNumber();
				filename = oFilename.getFilename();
				newFile = new File(filename);
			// rename temporary filename to filename
				if(temporaryFile.renameTo(newFile) == false)
				{
					String errorString = new String(twilightCalibrateCommand.getId()+
						":Failed to rename '"+temporaryFile.toString()+"' to '"+
						newFile.toString()+"'.");

					o.error(this.getClass().getName()+":doFrame:"+errorString);
					twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2315);
					twilightCalibrateDone.setErrorString(errorString);
					twilightCalibrateDone.setSuccessful(false);
					return false;
				}
			// log rename
				o.log(Logging.VERBOSITY_VERBOSE,
				      "Command:"+twilightCalibrateCommand.getId()+
				      ":doFrame:"+"bin:"+bin+
				      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
				      ":Exposure raw frame rename:renamed "+temporaryFile+" to "+newFile+".");
			// reset twilight calibrate done's filename to renamed file
			// in case pipelined reduced filename does not exist/cannot be renamed
				twilightCalibrateDone.setFilename(filename);
			// real time pipelined processed file
				temporaryFile = new File(reducedFilename);
			// does the temprary file exist? If it doesn't this is not an error,
			// if the DpRt returned the same file it was passed in it will have already been renamed
				if(temporaryFile.exists())
				{
				// get a filename to store pipelined processed frame in
					try
					{
						oFilename.setPipelineProcessing(FitsFilename.
										PIPELINE_PROCESSING_FLAG_REAL_TIME);
						filename = oFilename.getFilename();
						oFilename.setPipelineProcessing(FitsFilename.
										PIPELINE_PROCESSING_FLAG_NONE);
					}
					catch(Exception e)
					{
						String errorString = new String(twilightCalibrateCommand.getId()+
									    ":doFrame:setPipelineProcessing failed:");
						o.error(this.getClass().getName()+":"+errorString,e);
						twilightCalibrateDone.setFilename(reducedFilename);
						twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2321);
						twilightCalibrateDone.setErrorString(errorString+e);
						twilightCalibrateDone.setSuccessful(false);
						return false;
					}
					newFile = new File(filename);
				// rename temporary filename to filename
					if(temporaryFile.renameTo(newFile) == false)
					{
						String errorString = new String(twilightCalibrateCommand.getId()+
							":Failed to rename '"+temporaryFile.toString()+"' to '"+
							newFile.toString()+"'.");

						o.error(this.getClass().getName()+":doFrame:"+errorString);
						twilightCalibrateDone.
							setErrorNum(OConstants.O_ERROR_CODE_BASE+2316);
						twilightCalibrateDone.setErrorString(errorString);
						twilightCalibrateDone.setSuccessful(false);
						return false;
					}// end if renameTo failed
				// reset twilight calibrate done's pipelined processed filename
					twilightCalibrateDone.setFilename(filename);
				// log rename
					o.log(Logging.VERBOSITY_VERBOSE,
					      "Command:"+twilightCalibrateCommand.getId()+
					      ":doFrame:"+"bin:"+bin+
					      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
					      ":Exposure DpRt frame rename:renamed "+temporaryFile+" to "+newFile+".");
				}// end if temporary file exists
			}// end if frameState was OK
		// Test abort status.
			if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
				return false;
		// multiply exposure Length by scale factor 
		// to scale current mean counts to best mean counts
			exposureLength = (int)(((float) exposureLength) * (((float)(bestMeanCounts[bin]))/meanCounts));
		// If the calculated exposure length is out of the acceptable range,
	        // but the last attempt was not at the range limit or 
                // the last attempt was at the limit but acceptable, 
		// reset the exposure length to the limit.
			if((exposureLength > maxExposureLength)&&
			   ((lastExposureLength != maxExposureLength)||(frameState == FRAME_STATE_OK)))
			{
				o.log(Logging.VERBOSITY_VERBOSE,
				      "Command:"+twilightCalibrateCommand.getId()+
				      ":doFrame:"+"bin:"+bin+
				      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
				      ":Calculated exposure length:"+exposureLength+
				      " out of range, but going to try "+maxExposureLength+
				      " as last exposure length was "+lastExposureLength+
				      " with frame state "+FRAME_STATE_NAME_LIST[frameState]+".");
				exposureLength = maxExposureLength;
			}
			if((exposureLength < minExposureLength)&&
			   ((lastExposureLength != minExposureLength)||(frameState == FRAME_STATE_OK)))
			{
				o.log(Logging.VERBOSITY_VERBOSE,
				      "Command:"+twilightCalibrateCommand.getId()+
				      ":doFrame:"+"bin:"+bin+
				      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
				      ":Calculated exposure length:"+exposureLength+
				      " out of range, but going to try "+minExposureLength+
				      " as last exposure length was "+lastExposureLength+
				      " with frame state "+FRAME_STATE_NAME_LIST[frameState]+".");
				exposureLength = minExposureLength;
			}
		// send dp_ack, filename/mean counts/peak counts are all retrieved from twilightCalibrateDone,
		// which had these parameters filled in by reduceCalibrate
		// time to complete is readout overhead + exposure Time for next frame
			if(sendTwilightCalibrateDpAck(twilightCalibrateCommand,twilightCalibrateDone,
				exposureLength+frameOverhead) == false)
				return false;
		// Test abort status.
			if(testAbort(twilightCalibrateCommand,twilightCalibrateDone) == true)
				return false;
		// check loop termination
			now = System.currentTimeMillis();
			if(frameState == FRAME_STATE_OK)
			{
				doneFrame = true;
				calibrationFrameCount++;
			// log
				o.log(Logging.VERBOSITY_VERBOSE,"Command:"+twilightCalibrateCommand.getId()+
				      ":doFrame:"+"bin:"+bin+
				      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
				      ":Frame completed.");
			}
			if(exposureLength > maxExposureLength)
			{
				if(timeOfNight == TIME_OF_NIGHT_SUNSET)
				{
					// try next calibration
					doneFrame = true;
					doneOffset = true;
					// log
					o.log(Logging.VERBOSITY_VERBOSE,
					      "Command:"+twilightCalibrateCommand.getId()+
					      ":doFrame:"+"bin:"+bin+
					      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
					      ":Exposure length too long:"+
					      "(exposureLength:"+exposureLength+") > "+
					      "(maxExposureLength:"+maxExposureLength+").");
				}
				else // retry this calibration - it has got lighter
				{
					// log
					o.log(Logging.VERBOSITY_VERBOSE,
					      "Command:"+twilightCalibrateCommand.getId()+
					      ":doFrame:"+"bin:"+bin+
					      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
					      ":Calulated Exposure length too long:"+
					      "(exposureLength:"+exposureLength+") > "+
					      "(maxExposureLength:"+maxExposureLength+"): retrying as it is dawn.");
					exposureLength = maxExposureLength;
				}
			}
			if(exposureLength < minExposureLength)
			{
				if(timeOfNight == TIME_OF_NIGHT_SUNRISE)
				{
					// try next calibration
					doneFrame = true;
					doneOffset = true;
					// log
					o.log(Logging.VERBOSITY_VERBOSE,
					      "Command:"+twilightCalibrateCommand.getId()+
					      ":doFrame:"+"bin:"+bin+
					      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
					      ":Exposure length too short:"+
					      "(exposureLength:"+exposureLength+") < "+
					      "(minExposureLength:"+minExposureLength+").");
				}
				else // retry this calibration - it has got darker
				{
					// log
					o.log(Logging.VERBOSITY_VERBOSE,
					      "Command:"+twilightCalibrateCommand.getId()+
					      ":doFrame:"+"bin:"+bin+
					      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
					      ":Calulated Exposure length too short:"+
					      "(exposureLength:"+exposureLength+") < "+
					      "(maxExposureLength:"+maxExposureLength+"): retrying as it is dusk.");
					exposureLength = minExposureLength;
				}
			}
			// have we run out of twilight calibrate time?
			// NB test at end to use recalculated exposure length
			if((now+exposureLength+frameOverhead) > 
				(implementationStartTime+twilightCalibrateCommand.getTimeToComplete()))
			{
			// try next calibration
				doneFrame = true;
				doneOffset = true;
			// log
				o.log(Logging.VERBOSITY_VERBOSE,
				      "Command:"+twilightCalibrateCommand.getId()+
				      ":doFrame:"+"bin:"+bin+
				      ":upper slide:"+upperSlide+":lower slide:"+lowerSlide+":filter:"+filter+
				      ":Ran out of time to complete:((now:"+now+
				      ")+(exposureLength:"+exposureLength+
				      ")+(frameOverhead:"+frameOverhead+")) > "+
				      "((implementationStartTime:"+implementationStartTime+
				      ")+(timeToComplete:"+twilightCalibrateCommand.getTimeToComplete()+")).");
			}
		}// end while !doneFrame
		return true;
	}

	/**
	 * Method to send an instance of ACK back to the client. This stops the client timing out, whilst we
	 * work out what calibration to attempt next.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @param timeToComplete The time it will take to complete the next set of operations
	 *	before the next ACK or DONE is sent to the client. The time is in milliseconds. 
	 * 	The server connection thread's default acknowledge time is added to the value before it
	 * 	is sent to the client, to allow for network delay etc.
	 * @return The method returns true if the ACK was sent successfully, false if an error occured.
	 */
	protected boolean sendBasicAck(TWILIGHT_CALIBRATE twilightCalibrateCommand,
				       TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,int timeToComplete)
	{
		ACK acknowledge = null;

		acknowledge = new ACK(twilightCalibrateCommand.getId());
		acknowledge.setTimeToComplete(timeToComplete+serverConnectionThread.getDefaultAcknowledgeTime());
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+twilightCalibrateCommand.getId()+":sendBasicAck(time="+
		      (timeToComplete+serverConnectionThread.getDefaultAcknowledgeTime())+").");
		try
		{
			serverConnectionThread.sendAcknowledge(acknowledge,true);
		}
		catch(IOException e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":sendBasicAck:Sending ACK failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2317);
			twilightCalibrateDone.setErrorString(errorString+e);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to send an instance of TWILIGHT_CALIBRATE_ACK back to the client. This tells the client about
	 * a FITS frame that has been produced, and also stops the client timing out.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * @param timeToComplete The time it will take to complete the next set of operations
	 *	before the next ACK or DONE is sent to the client. The time is in milliseconds. 
	 * 	The server connection thread's default acknowledge time is added to the value before it
	 * 	is sent to the client, to allow for network delay etc.
	 * @param filename The FITS filename to be sent back to the client, that has just completed
	 * 	processing.
	 * @return The method returns true if the ACK was sent successfully, false if an error occured.
	 */
	protected boolean sendTwilightCalibrateAck(TWILIGHT_CALIBRATE twilightCalibrateCommand,
						   TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,
						   int timeToComplete,String filename)
	{
		TWILIGHT_CALIBRATE_ACK twilightCalibrateAck = null;

	// send acknowledge to say frame is completed.
		twilightCalibrateAck = new TWILIGHT_CALIBRATE_ACK(twilightCalibrateCommand.getId());
		twilightCalibrateAck.setTimeToComplete(timeToComplete+
			serverConnectionThread.getDefaultAcknowledgeTime());
		twilightCalibrateAck.setFilename(filename);
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+twilightCalibrateCommand.getId()+
		      ":sendTwilightCalibrateAck(time="+
		      (timeToComplete+serverConnectionThread.getDefaultAcknowledgeTime())+",filename="+filename+").");
		try
		{
			serverConnectionThread.sendAcknowledge(twilightCalibrateAck,true);
		}
		catch(IOException e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":sendTwilightCalibrateAck:Sending TWILIGHT_CALIBRATE_ACK failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2318);
			twilightCalibrateDone.setErrorString(errorString+e);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to send an instance of TWILIGHT_CALIBRATE_DP_ACK back to the client. This tells the client about
	 * a FITS frame that has been produced, and the mean and peak counts in the frame.
	 * The time to complete parameter stops the client timing out.
	 * @param twilightCalibrateCommand The instance of TWILIGHT_CALIBRATE we are currently running.
	 * @param twilightCalibrateDone The instance of TWILIGHT_CALIBRATE_DONE to fill in with errors we receive.
	 * 	It also contains the filename and mean and peak counts returned from the last reduction calibration.
	 * @param timeToComplete The time it will take to complete the next set of operations
	 *	before the next ACK or DONE is sent to the client. The time is in milliseconds. 
	 * 	The server connection thread's default acknowledge time is added to the value before it
	 * 	is sent to the client, to allow for network delay etc.
	 * @return The method returns true if the ACK was sent successfully, false if an error occured.
	 */
	protected boolean sendTwilightCalibrateDpAck(TWILIGHT_CALIBRATE twilightCalibrateCommand,
		TWILIGHT_CALIBRATE_DONE twilightCalibrateDone,int timeToComplete)
	{
		TWILIGHT_CALIBRATE_DP_ACK twilightCalibrateDpAck = null;

	// send acknowledge to say frame is completed.
		twilightCalibrateDpAck = new TWILIGHT_CALIBRATE_DP_ACK(twilightCalibrateCommand.getId());
		twilightCalibrateDpAck.setTimeToComplete(timeToComplete+
			serverConnectionThread.getDefaultAcknowledgeTime());
		twilightCalibrateDpAck.setFilename(twilightCalibrateDone.getFilename());
		twilightCalibrateDpAck.setMeanCounts(twilightCalibrateDone.getMeanCounts());
		twilightCalibrateDpAck.setPeakCounts(twilightCalibrateDone.getPeakCounts());
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+twilightCalibrateCommand.getId()+
		      ":sendTwilightCalibrateDpAck(time="+
		      (timeToComplete+serverConnectionThread.getDefaultAcknowledgeTime())+",filename="+
		      twilightCalibrateDone.getFilename()+
		      ",mean counts="+twilightCalibrateDone.getMeanCounts()+
		      ",peak counts="+twilightCalibrateDone.getPeakCounts()+").");
		try
		{
			serverConnectionThread.sendAcknowledge(twilightCalibrateDpAck,true);
		}
		catch(IOException e)
		{
			String errorString = new String(twilightCalibrateCommand.getId()+
				":sendTwilightCalibrateDpAck:Sending TWILIGHT_CALIBRATE_DP_ACK failed:");
			o.error(this.getClass().getName()+":"+errorString,e);
			twilightCalibrateDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2319);
			twilightCalibrateDone.setErrorString(errorString+e);
			twilightCalibrateDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Private inner class that deals with loading and interpreting the saved state of calibrations
	 * (the TWILIGHT_CALIBRATE calibration database).
	 */
	private class TWILIGHT_CALIBRATESavedState
	{
		private NGATProperties properties = null;

		/**
		 * Constructor.
		 */
		public TWILIGHT_CALIBRATESavedState()
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
			properties.save(filename,"TWILIGHT_CALIBRATE saved state saved on:"+now);
		}

		/**
		 * Method to get the last time a calibration with these attributes was done.
		 * @param bin The binning factor used for this calibration.
		 * @param useWindowAmplifier Whether we are using the default amplifier (false) or the amplifier
		 *        used for windowing (true).
		 * @param filter The filter type string used for this calibration.
		 * @param lowerSlide The lower slide position used for this calibration. 
		 * @param upperSlide The upper slide position used for this calibration. 
		 * @return The number of milliseconds since the EPOCH, the last time a calibration with these
		 * 	parameters was completed. If this calibraion has not been performed before, zero
		 * 	is returned.
		 * @see #LIST_KEY_STRING
		 * @see #LIST_KEY_LAST_TIME_STRING
		 */
		public long getLastTime(int bin,boolean useWindowAmplifier,
					String lowerSlide,String upperSlide,String filter)
		{
			long time;

			try
			{
				time = properties.getLong(LIST_KEY_STRING+LIST_KEY_LAST_TIME_STRING+bin+"."+
					       useWindowAmplifier+"."+lowerSlide+"."+upperSlide+"."+
					       filter);
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
		 * @param bin The binning factor used for this calibration.
		 * @param useWindowAmplifier Whether we are using the default amplifier (false) or the amplifier
		 *        used for windowing (true).
		 * @param lowerSlide The lower slide position used for this calibration.
		 * @param upperSlide The upper slide position used for this calibration.
		 * @param filter The filter type string used for this calibration.
		 * @see #LIST_KEY_STRING
		 * @see #LIST_KEY_LAST_TIME_STRING
		 */
		public void setLastTime(int bin,boolean useWindowAmplifier,
					String lowerSlide,String upperSlide,String filter)
		{
			long now;

			now = System.currentTimeMillis();
			properties.setProperty(LIST_KEY_STRING+LIST_KEY_LAST_TIME_STRING+bin+"."+
					       useWindowAmplifier+"."+lowerSlide+"."+upperSlide+"."+
					       filter,new String(""+now));
		}
	}// end class TWILIGHT_CALIBRATESavedState

	/**
	 * Private inner class that stores data pertaining to one possible calibration run that can take place during
	 * a TWILIGHT_CALIBRATE command invocation.
	 */
	private class TWILIGHT_CALIBRATECalibration
	{
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
		 * What position the lower slide should be in. 
		 */
		protected String lowerSlide;
		/**
		 * What position the upper slide should be in. 
		 */
		protected String upperSlide;
		/**
		 * The filter to use in the filter wheel.
		 */
		protected String filter = null;
		/**
		 * How often we should perform the calibration in milliseconds.
		 */
		protected long frequency;
		/**
		 * How sensitive is the filter (combination) to twilight daylight,
		 * as compared to no filters (1.0). A double between zero and one.
		 */
		protected double filterSensitivity = 0.0;
		/**
		 * The last time this calibration was performed. This is retrieved from the saved state,
		 * not from the calibration list.
		 */
		protected long lastTime;
		
		/**
		 * Constructor.
		 */
		public TWILIGHT_CALIBRATECalibration()
		{
			super();
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
		 * Set the lower slide position.
		 * @param s The lower slide position.
		 * @see #lowerSlide
		 */
		public void setLowerSlide(String s)
		{
			lowerSlide = s;
		}

		/**
		 * Get the lower slide position for this twilight calibration.
		 * @return The lower slide position.
		 * @see #lowerSlide
		 */
		public String getLowerSlide()
		{
			return lowerSlide;
		}

		/**
		 * Set the upper slide position.
		 * @param s The upper slide position.
		 * @see #upperSlide
		 */
		public void setUpperSlide(String s)
		{
			upperSlide = s;
		}

		/**
		 * Get the upper slide position for this twilight calibration.
		 * @return The upper slide position as an integer.
		 * @see #upperSlide
		 */
		public String getUpperSlide()
		{
			return upperSlide;
		}

		/**
		 * Method to set the filter type name.
	 	 * @param s The name to use.
		 * @exception NullPointerException Thrown if the filter string was null.
		 */
		public void setFilter(String s) throws NullPointerException
		{
			if(s == null)
			{
				throw new NullPointerException(this.getClass().getName()+
						":setFilter:Filter was null.");
			}
			filter = s;
		}

		/**
		 * Method to return the filter type for this calibration.
		 * @return A string containing the filter string.
		 */
		public String getFilter()
		{
			return filter;
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
		 * Method to set the relative filter sensitivity of the filters at twilight in this calibration.
		 * @param d The relative filter sensitivity, compared to no filters. 
		 * 	This should be greater than zero and less than 1.0 (inclusive).
		 * @exception IllegalArgumentException Thrown if parameter d is out of range.
		 * @see #filterSensitivity
		 */
		public void setFilterSensitivity(double d) throws IllegalArgumentException
		{
			if((d < 0.0)||(d > 1.0))
			{
				throw new IllegalArgumentException(this.getClass().getName()+
					":setFilterSensitivity failed:"+d+" not a legal relative sensitivity.");
			}
			filterSensitivity = d;
		}

		/**
		 * Method to get the relative filter sensitivity of the filters at twilight for this calibration.
		 * @return The relative filter sensitivity of the filters, between 0.0 and 1.0, where 1.0 is
		 * 	the sensitivity no filters have.
		 * @see #filterSensitivity
		 */
		public double getFilterSensitivity()
		{
			return filterSensitivity;
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
	}// end class TWILIGHT_CALIBRATECalibration

	/**
	 * Private inner class that stores data pertaining to one telescope offset.
	 */
	private class TWILIGHT_CALIBRATEOffset
	{
		/**
		 * The offset in RA, in arcseconds.
		 */
		protected float raOffset;
		/**
		 * The offset in DEC, in arcseconds.
		 */
		protected float decOffset;
		
		/**
		 * Constructor.
		 */
		public TWILIGHT_CALIBRATEOffset()
		{
			super();
		}

		/**
		 * Method to set the offset in RA.
		 * @param o The offset in RA, in arcseconds, to use. This parameter must be in the range
		 * 	[-3600..3600] arcseconds.
		 * @exception IllegalArgumentException Thrown if parameter o is out of range.
		 * @see #raOffset
		 */
		public void setRAOffset(float o) throws IllegalArgumentException
		{
			if((o < -3600)||(o > 3600))
			{
				throw new IllegalArgumentException(this.getClass().getName()+":setRAOffset failed:"+
					o+" out of range.");
			}
			raOffset = o;
		}

		/**
		 * Method to get the offset in RA.
		 * @return The offset, in arcseconds.
		 * @see #raOffset
		 */
		public float getRAOffset()
		{
			return raOffset;
		}

		/**
		 * Method to set the offset in DEC.
		 * @param o The offset in DEC, in arcseconds, to use. This parameter must be in the range
		 * 	[-3600..3600] arcseconds.
		 * @exception IllegalArgumentException Thrown if parameter o is out of range.
		 * @see #decOffset
		 */
		public void setDECOffset(float o) throws IllegalArgumentException
		{
			if((o < -3600)||(o > 3600))
			{
				throw new IllegalArgumentException(this.getClass().getName()+":setDECOffset failed:"+
					o+" out of range.");
			}
			decOffset = o;
		}

		/**
		 * Method to get the offset in DEC.
		 * @return The offset, in arcseconds.
		 * @see #decOffset
		 */
		public float getDECOffset()
		{
			return decOffset;
		}
	}// end TWILIGHT_CALIBRATEOffset

}

//
// $Log: not supported by cvs2svn $
// Revision 1.10  2012/07/26 14:00:21  cjm
// Added extra test in doFrame to capture negative counts, and
// assume the CCD is saturated.
//
// Revision 1.9  2012/07/23 15:29:11  cjm
// Added some logging of ACK times.
//
// Revision 1.8  2012/07/13 11:23:07  cjm
// Fixed Attempting exposure comment.
//
// Revision 1.7  2012/07/13 11:10:35  cjm
// Logging changes to remove newlines - makes the logs harder to read but easier to grep.
// Passed down filter[Slides] and binning to doFrame, so doFrame logs have lots of
// useful into in them.
//
// Revision 1.6  2012/07/12 14:26:59  cjm
// Added extra setting of initial values of lastFilterSensitivity and
// lastBin from first calibration, so first exposure length is calculated to be the
// minimum or maximum as appropriate.
//
// More logging for how the exposure length is calculated.
//
// Revision 1.5  2012/07/12 12:30:28  cjm
// Changed indexing into min/best/maxMeanCounts and BIN_COUNT now 5.
// This allows property keys and array indexes to use the binning factor as an index.
//
// Revision 1.4  2012/02/08 10:47:10  cjm
// Moved beamSteer method to FITSImplementation so it can be called from ACQUIREImplementation.
// Changed beamSteer API so it throws an exception.
//
// Revision 1.3  2012/01/17 15:26:34  cjm
// Changed upperSlide and lowerSlide from parsed integers to Strings, to match new parameters
// to BEAM_STEER command.
//
// Revision 1.2  2012/01/11 14:55:18  cjm
// Added per binning min/best/max counts.
// Calibrations are now for a specific lower / upper slide configuration.
// beamSteer method added to move slides to correct position.
//
// Revision 1.1  2011/11/23 10:55:24  cjm
// Initial revision
//
//
