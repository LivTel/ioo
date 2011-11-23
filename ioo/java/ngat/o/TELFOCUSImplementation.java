// TELFOCUSImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/TELFOCUSImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.File;
import java.io.IOException;
import java.util.Vector;
import ngat.o.ccd.*;
import ngat.fits.FitsHeaderDefaults;
import ngat.math.QuadraticFit;
import ngat.message.base.*;
import ngat.message.ISS_INST.TELFOCUS;
import ngat.message.ISS_INST.TELFOCUS_ACK;
import ngat.message.ISS_INST.TELFOCUS_DP_ACK;
import ngat.message.ISS_INST.TELFOCUS_DONE;
import ngat.message.ISS_INST.SET_FOCUS;
import ngat.message.ISS_INST.SET_FOCUS_DONE;
import ngat.message.ISS_INST.OFFSET_FOCUS;
import ngat.message.ISS_INST.OFFSET_FOCUS_DONE;
import ngat.message.ISS_INST.INST_TO_ISS_DONE;
import ngat.message.INST_DP.EXPOSE_REDUCE;
import ngat.message.INST_DP.EXPOSE_REDUCE_DONE;
import ngat.message.INST_DP.INST_TO_DP_DONE;
import ngat.util.logging.*;

/**
 * This class provides the implementation for the TELFOCUS command sent to a server using the
 * Java Message System. It extends SETUPImplementation. 
 * <br>This command requires the instrument to issue telescope SET_FOCUS commands to the ISS in order to attempt to 
 * focus the telescope. An exposure is taken at each focus position, and then reduced.
 * @see SETUPImplementation
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class TELFOCUSImplementation extends SETUPImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: TELFOCUSImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");
	/**
	 * A small number. Used in getFocus to prevent a division by zero.
	 * @see #getFocus
	 */	 
	protected final static double NEARLY_ZERO = 0.00001;
	/**
	 * Copy of the command parameters startFocus, the first position to drive the telescope focus to.
	 */
	private float startFocus = 0.0f;
	/**
	 * Copy of the command parameters endFocus, the last position to drive the telescope focus to.
	 */
	private float endFocus = 0.0f;
	/**
	 * Copy of the command parameters step, the amount to increment the telescope focus by.
	 */
	private float step = 0.0f;
	/**
	 * Copy of the command parameters exposureTime, how long to expose at each focus position.
	 */
	private int exposureTime = 0;
	/**
	 * This overhead is loaded from the <b>&quot;o.telfocus.ack_time.per_exposure_overhead&quot;</b>
	 * and represents the number of milliseconds overhead for every exposure (readout time, saving to
	 * disc etc). It is setup in the <i>init</i> method and used for sending
	 * acknowledgements with the correct time to complete.
	 */
	private int perExposureOverhead = 0;
	/**
	 * This overhead is loaded from the <b>&quot;o.telfocus.ack_time.reduce_overhead&quot;</b>
	 * and represents the number of milliseconds overhead for the reduction of each frame (de-biasing, saving to
	 * disc etc). It is setup in the <i>init</i> method and used for sending data pipeline
	 * acknowledgements with the correct time to complete.
	 */
	private int reduceOverhead = 0;

	/**
	 * Constructor.
	 */
	public TELFOCUSImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.TELFOCUS&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.TELFOCUS";
	}

	/**
	 * This method is the first to be called in this class, to initialise command variables needed
	 * when implementing TELFOCUS. 
	 * <ul>
	 * <li>It calls the superclass's init method.
	 * <li>It copies the command parameters.
	 * <li>It initialises perExposureOverhead and reduceOverhead by querying the O status for them.
	 * </ul>
	 * @param command The command to be implemented.
	 * @see #startFocus
	 * @see #endFocus
	 * @see #step
	 * @see #exposureTime
	 * @see #perExposureOverhead
	 * @see #reduceOverhead
	 */
	public void init(COMMAND command)
	{
		TELFOCUS telFocusCommand = (TELFOCUS)command;

		super.init(command);
	// get copy of command args
		startFocus = telFocusCommand.getStartFocus();
		endFocus = telFocusCommand.getEndFocus();
		step = telFocusCommand.getStep();
		exposureTime = telFocusCommand.getExposureTime();
	// Get the amount of time to add to the exposure time for readout/setup
		try
		{
			perExposureOverhead = status.getPropertyInteger("o.telfocus.ack_time.per_exposure_overhead");
		}
		catch (Exception e)
		{
			perExposureOverhead = 10000;
			o.error(this.getClass().getName()+":calculateAcknowledgeTime:"+
				"getting per exposure overhead failed:"+
				"using default value:"+perExposureOverhead+"\n\t"+e);
		}
	// Get the amount of time to reduce each frame
		try
		{
			reduceOverhead = status.getPropertyInteger("o.telfocus.ack_time.reduce_overhead");
		}
		catch (Exception e)
		{
			reduceOverhead = 5000;
			o.error(this.getClass().getName()+":calculateAcknowledgeTime:"+
				"getting reduce overhead failed:"+
				"using default value:"+reduceOverhead+"\n\t"+e);
		}
	}

	/**
	 * This method gets the TELFOCUS command's acknowledge time. The time to complete returned
	 * is for the first frame only, as an ACK is returned after every frame.
	 * <ul>
	 * <li>The time to complete is set to the default acknowledge time.
	 * <li>The exposure time plus an exposure overhead time is added for the first exposure.
	 * <li>The acknowledges time to complete is set.
	 * </ul>
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set.
	 * @exception IllegalArgumentException Thrown if the TELFOCUS step size is zero.
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OTCPServerConnectionThread#getDefaultAcknowledgeTime
	 */
	public ACK calculateAcknowledgeTime(COMMAND command) throws IllegalArgumentException
	{
		TELFOCUS telFocusCommand = (TELFOCUS)command;
		ACK acknowledge = null;
		int time;

		acknowledge = new ACK(command.getId());
	// calculate acknowledge time.
		time = serverConnectionThread.getDefaultAcknowledgeTime();
		time += (exposureTime+perExposureOverhead);
		acknowledge.setTimeToComplete(time);
		return acknowledge;
	}

	/**
	 * This method implements the TELFOCUS command. 
	 * <ul>
	 * <li>The directory and exposure status are setup.
	 * <li>The fold mirror is driven to a suitable location.
	 * <li>The focus offset is reset to zero using resetFocusOffset.
	 * <li>A loop is entered, from the startFocus to the endFocus in step sizes.
	 *     <ul>
	 *     <li>The exposure status is set, and a new element in the list of frame parameters setup.
	 *     <li>setFocus is called to drive the telescope to the required focus.
	 *     <li>An exposure is taken, using exposeFrame.
	 *     <li>An acknowledgement is sent back to the client, using sendFrameAcknowledge.
	 *     </ul>
	 * <li>A loop is entered, for each filename to reduce.
	 *     <ul>
	 *     <li>reduceFrame is called for each filename, which sends each filename to the real, time data
	 * 		pipeline to be reduced, and saves the returned parameters in the list of frame parameters.
	 *     <li>An acknowledgement is sent back to the client, using sendDpAcknowledge, with the returned
	 * 		data.
	 *     </ul>
	 * <li>The best focus is then calculated, fitting a curve to the seeing generated from each reduced frame.
	 *	The bottom of the curve is the best seeing - i.e. the telescope is in focus.
	 * 	The quadraticFit method is called to do this.
	 * </ul>
	 * An object of class TELFOCUS_DONE is returned.
	 * During the implementation of this command testAbort is regularily called, to see if this command
	 * has been aborted. If it has the done parameters are setup accordingly.
	 * @see O#sendISSCommand
	 * @see #testAbort
	 * @see #moveFold
	 * @see #resetFocusOffset
	 * @see #setFocus
	 * @see #exposeFrame
	 * @see #sendFrameAcknowledge
	 * @see #reduceFrame
	 * @see #sendDpAcknowledge
	 * @see #quadraticFit
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		TELFOCUS telFocusCommand = (TELFOCUS)command;
		TELFOCUS_DONE telFocusDone = new TELFOCUS_DONE(command.getId());
		TELFOCUSFrameParameters frameParameters = null;
		Vector list = null;
		int exposureNumber;

		try
		{
		// check step is safe before dividing by it
			if(step < NEARLY_ZERO)
			{
				String errorString = new String(command.getId()+
					":processCommand:step was too small:"+step);
				o.error(this.getClass().getName()+":"+errorString);
				telFocusDone.setSeeing(0.0f);
				telFocusDone.setCurrentFocus(0.0f);
				telFocusDone.setA(0.0f);
				telFocusDone.setB(0.0f);
				telFocusDone.setC(0.0f);
				telFocusDone.setChiSquared(0.0f);
				telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2008);
				telFocusDone.setErrorString(errorString);
				telFocusDone.setSuccessful(false);
				return telFocusDone;
			}
		// setup exposure status/directory
			exposureNumber = 0;
			status.setExposureCount((int)((endFocus-startFocus)/step));
			list = new Vector();
		// move the fold mirror to the correct location
			if(moveFold(telFocusCommand,telFocusDone) == false)
				return telFocusDone;
			if(testAbort(telFocusCommand,telFocusDone) == true)
				return telFocusDone;
		// reset the FOCUS_OFFSET (DFOCUS) to zero
			if(resetFocusOffset(telFocusCommand,telFocusDone) == false)
				return telFocusDone;
		// start exposure loop for each focus
			for(float focus=startFocus; focus <= endFocus; focus += step)
			{
				status.setExposureNumber(exposureNumber);
			// Clear pause and resume times.
				status.clearPauseResumeTimes();
				frameParameters = new TELFOCUSFrameParameters();
				list.add(frameParameters);
			// send ISS SET_FOCUS command.
				if(setFocus(telFocusCommand,telFocusDone,focus) == false)
				{
					return telFocusDone;
				}
				frameParameters.setFocus(focus);
			// do exposure to telFocusN.fits
				if(exposeFrame(telFocusCommand,telFocusDone,exposureNumber,frameParameters) == false)
				{
					return telFocusDone;
				}
			// send acknowledge
				if(sendFrameAcknowledge(telFocusCommand,telFocusDone,
							frameParameters.getFilename()) == false)
				{
					return telFocusDone;
				}
			// increment frame number.
				exposureNumber++;
			}// end for on focus
		}// end try
	// Other exceptions (IllegalArgumentException,NumberFormatException) are not caught here, 
	// but by the calling method catch(Exception e)
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":processCommand:"+e);
			telFocusDone.setSeeing(0.0f);
			telFocusDone.setCurrentFocus(0.0f);
			telFocusDone.setA(0.0f);
			telFocusDone.setB(0.0f);
			telFocusDone.setC(0.0f);
			telFocusDone.setChiSquared(0.0f);
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2000);
			telFocusDone.setErrorString(e.toString());
			telFocusDone.setSuccessful(false);
			return telFocusDone;
		}
	// reduce data
		for(int i = 0; i < list.size(); i++)
		{
			frameParameters = (TELFOCUSFrameParameters)list.get(i);
			if(testAbort(telFocusCommand,telFocusDone) == true)
				return telFocusDone;
		// call pipeline
			if(reduceFrame(telFocusCommand,telFocusDone,frameParameters) == false)
				return telFocusDone;
			if(testAbort(telFocusCommand,telFocusDone) == true)
				return telFocusDone;
		// send data pipeline acknowledge
			if(sendDpAcknowledge(telFocusCommand,telFocusDone,frameParameters) == false)
				return telFocusDone;
		}// end for on exposures
	// calculate seeing / optimum focus from list of frameParameters, using a quadratic fit
		if(quadraticFit(telFocusCommand,telFocusDone,list) == false)
			return telFocusDone;
	// send ISS SET_FOCUS command for bestFocus. The best focus is returned by quadraticFit in the
	// done message.
		if(setFocus(telFocusCommand,telFocusDone,telFocusDone.getCurrentFocus()) == false)
			return telFocusDone;
		telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		telFocusDone.setErrorString("");
		telFocusDone.setSuccessful(true);
	// return done object.
		return telFocusDone;
	}

	/**
	 * Routine to reset the telescope focus offset to zero. This means the telescope virtual focus set by
	 * setFocus is the actual virtual focus with no DFOCUS applied.
	 * @param telFocusCommand The TELFOCUS command that is causing this focus setting to occur. The Id is used
	 * 	as the SET_FOCUS command's id.
	 * @param telFocusDone The instance of TELFOCUS_DONE. This is filled in with an error message if the
	 * 	SET_FOCUS fails.
	 * @return The method returns true if the telescope focus offset was reset to 0, otherwise false is
	 * 	returned an telFocusDone is filled in with an error message.
	 */
	private boolean resetFocusOffset(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone)
	{
		OFFSET_FOCUS focusOffsetCommand = null;
		INST_TO_ISS_DONE instToISSDone = null;
		float focusOffset;

		focusOffsetCommand = new OFFSET_FOCUS(telFocusCommand.getId());
		focusOffset = 0.0f;
	// do not apply default focus offset
	// do not apply focus offset?
	// set the commands focus offset
		focusOffsetCommand.setFocusOffset(focusOffset);
		o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+":resetFocusOffset:To "+
		      focusOffset+".");
		instToISSDone = o.sendISSCommand(focusOffsetCommand,serverConnectionThread);
		if(instToISSDone.getSuccessful() == false)
		{
			o.error(this.getClass().getName()+":resetFocusOffset failed:"+focusOffset+":"+
				instToISSDone.getErrorString());
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2009);
			telFocusDone.setErrorString(instToISSDone.getErrorString());
			telFocusDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Routine to set the telescope focus. Called for each frame. Sends a SET_FOCUS command to
	 * the ISS.
	 * @param telFocusCommand The TELFOCUS command that is causing this focus setting to occur. The Id is used
	 * 	as the SET_FOCUS command's id.
	 * @param telFocusDone The instance of TELFOCUS_DONE. This is filled in with an error message if the
	 * 	SET_FOCUS fails.
	 * @param focus The position to set the telescope's focus to.
	 * @return The method returns true if the telescope attained the required focus, otherwise false is
	 * 	returned an telFocusDone is filled in with an error message.
	 */
	private boolean setFocus(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone,float focus)
	{
		SET_FOCUS setFocusCommand = null;
		INST_TO_ISS_DONE instToISSDone = null;

		setFocusCommand = new SET_FOCUS(telFocusCommand.getId());
		setFocusCommand.setFocus(focus);
		instToISSDone = o.sendISSCommand(setFocusCommand,serverConnectionThread);
		if(instToISSDone.getSuccessful() == false)
		{
			o.error(this.getClass().getName()+":setFocus failed:"+focus+":"+
				instToISSDone.getErrorString());
			telFocusDone.setSeeing(0.0f);
			telFocusDone.setCurrentFocus(0.0f);
			telFocusDone.setA(0.0f);
			telFocusDone.setB(0.0f);
			telFocusDone.setC(0.0f);
			telFocusDone.setChiSquared(0.0f);
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2001);
			telFocusDone.setErrorString(instToISSDone.getErrorString());
			telFocusDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to take one frame needed for a TELFOCUS.
	 * <ul>
	 * <li>The filename is generated from the exposureNumber and the "o.file.fits.path" property.
	 * <li>Any old files of this name are deleted.
	 * <li>The FITS headers are generated from data and ISS GET_FITS command 
	 * 	(clearFitsHeaders, setFitsHeaders, getFitsHeadersFromISS, getFitsHeadersFromBSS).
	 * <li>The FITS headers for this frame are saved using the saveFitsHeaders method.
	 * <li>The exposure is performed and saved in the filename, using the expose method in libo_ccd.
	 * <li>unLockFile is called to remove the FITS lock file created in saveFitsHeaders
	 * <li>The frameParameters filename field is set to the saved filename.
	 * </ul>
	 * testAbort is called during this method to see if the command has been aborted.
	 * @param telFocusCommand The TELFOCUS command that is causing this exposure. It is passed
	 * 	to saveFitsHeaders and testAbort.
	 * @param telFocusDone The instance of TELFOCUS_DONE. This is filled in with an error message if the
	 * 	exposure fails. It is passed to saveFitsHeaders and testAbort.
	 * @param exposureNumber The frame number. Used to generate the filename.
	 * @param frameParameters The frame parameters for this exposure. The filename is set in this,
	 * 	if the exposure is completed successfully.
	 * @return The method returns true if the exposure was completed successfully. Otherwise false is returned,
	 * 	and the error fields in telFocusDone are filled in.
	 * @exception CCDLibraryNativeException Thrown if the exposure fails (expose).
	 * @see FITSImplementation#clearFitsHeaders
	 * @see FITSImplementation#setFitsHeaders
	 * @see FITSImplementation#getFitsHeadersFromISS
	 * @see FITSImplementation#getFitsHeadersFromBSS
	 * @see FITSImplementation#saveFitsHeaders
	 * @see FITSImplementation#unLockFile
	 * @see FITSImplementation#testAbort
	 * @see CCDLibrary#expose
	 */
	private boolean exposeFrame(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone,int exposureNumber,
		TELFOCUSFrameParameters frameParameters) throws CCDLibraryNativeException
	{
		File file = null;
		String directoryString = null;
		String filename = null;

	// get directory/filename
		directoryString = status.getProperty("o.file.fits.path");
		if(directoryString.endsWith(System.getProperty("file.separator")) == false)
			directoryString = directoryString.concat(System.getProperty("file.separator"));
		filename = new String(directoryString+status.getProperty("o.telfocus.file")+exposureNumber+".fits");
	// delete old file if it exists
		file = new File(filename);
		if(file.exists())
			file.delete();
	// get fits headers
		clearFitsHeaders();
		if(setFitsHeaders(telFocusCommand,telFocusDone,FitsHeaderDefaults.OBSTYPE_VALUE_EXPOSURE,
			exposureTime) == false)
			return false;
		if(getFitsHeadersFromISS(telFocusCommand,telFocusDone) == false)
			return false;
		if(getFitsHeadersFromBSS(telFocusCommand,telFocusDone) == false)
			return false;
		if(testAbort(telFocusCommand,telFocusDone) == true)
			return false;
	// save FITS headers
		if(saveFitsHeaders(telFocusCommand,telFocusDone,filename) == false)
		{
			unLockFile(telFocusCommand,telFocusDone,filename);
			return false;
		}
		if(testAbort(telFocusCommand,telFocusDone) == true)
		{
			unLockFile(telFocusCommand,telFocusDone,filename);
			return false;
		}
	// do glance
		status.setExposureFilename(filename);
		try
		{
			ccd.expose(true,-1,exposureTime,filename);
		}
		finally
		{
			// unlock FITS file lock created in saveFitsHeaders
			if(unLockFile(telFocusCommand,telFocusDone,filename) == false)
				return false;
		}
		if(testAbort(telFocusCommand,telFocusDone) == true)
			return false;
		frameParameters.setFilename(filename);
		return true;
	}

	/**
	 * Method to send an acknowledgement back to the ISS, after a frame has been exposed.
	 * An instance of TELFOCUS_ACK is constructed and returned to the client (the ISS).
	 * The filename sent is the one passed to this method, i.e. the last frame exposed.
	 * The timeToComplete is set to the frame <b>exposureTime</b> plus the <b>perExposureOverhead</b>
	 * plus the default acknowledge time.
	 * @param telFocusCommand The instance of COMMAND that caused this TELFOCUS to occur.
	 * @param telFocusDone The DONE command that will be sent back to the ISS. Filled in with
	 * 	an error message if this method fails.
	 * @param filename The filename of the frame just exposed, to be returned to the ISS in the ACK.
	 * @return The method returns true if it was successful, if it fails (sendAcknowledge throws an
	 * 	IOException) false is returned, and an error is sent to the log, and telFocusDone has
	 * 	it's error flag/message set accordingly.
	 * @see #exposureTime
	 * @see #perExposureOverhead
	 * @see #serverConnectionThread
	 * @see #o
	 */
	private boolean sendFrameAcknowledge(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone,String filename)
	{
		TELFOCUS_ACK telFocusAck = null;

	// send acknowledge to say frame is completed.
		telFocusAck = new TELFOCUS_ACK(telFocusCommand.getId());
		telFocusAck.setTimeToComplete(exposureTime+perExposureOverhead+
			serverConnectionThread.getDefaultAcknowledgeTime());
		telFocusAck.setFilename(filename);
		try
		{
			serverConnectionThread.sendAcknowledge(telFocusAck);
		}
		catch(IOException e)
		{
			o.error(this.getClass().getName()+
				":processCommand:sendFrameAcknowledge:"+telFocusCommand+":"+e.toString());
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2006);
			telFocusDone.setErrorString(e.toString());
			telFocusDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to reduce a frame.
	 * @param telFocusCommand The TELFOCUS command that is caused the frame reduction to occur. The Id is used
	 * 	as the EXPOSE_REDUCE command's id.
	 * @param telFocusDone The instance of TELFOCUS_DONE. This is filled in with an error message if the
	 * 	EXPOSE_REDUCE fails.
	 * @param frameParameters The frame parameters for this frame. The filename to reduce is retrieved from this.
	 * 	It's other parameters are set (including reducedFlename) with the EXPOSE_REDUCE return values.
	 * @return The method returns true if the reduction was successfull. Otherwise it returns false,
	 * 	and telFocusDone's error fields are set.
	 * @see O#sendDpRtCommand
	 * @see #testAbort
	 * @see ngat.message.INST_DP.EXPOSE_REDUCE
	 */
	private boolean reduceFrame(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone,
		TELFOCUSFrameParameters frameParameters)
	{
		EXPOSE_REDUCE reduceCommand = null;
		EXPOSE_REDUCE_DONE reduceDone = null;
		INST_TO_DP_DONE instToDPDone = null;

		reduceCommand = new EXPOSE_REDUCE(telFocusCommand.getId());
		reduceCommand.setFilename(frameParameters.getFilename());
		instToDPDone = o.sendDpRtCommand(reduceCommand,serverConnectionThread);
		if(instToDPDone.getSuccessful() == false)
		{
			o.error(this.getClass().getName()+":reduceFrame:"+reduceCommand+":"+
				instToDPDone.getErrorNum()+":"+instToDPDone.getErrorString());
			telFocusDone.setSeeing(0.0f);
			telFocusDone.setCurrentFocus(0.0f);
			telFocusDone.setA(0.0f);
			telFocusDone.setB(0.0f);
			telFocusDone.setC(0.0f);
			telFocusDone.setChiSquared(0.0f);
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2002);
			telFocusDone.setErrorString(instToDPDone.getErrorString());
			telFocusDone.setSuccessful(false);
			return false;
		}
		if(testAbort(telFocusCommand,telFocusDone) == true)
			return false;
	// Copy the DP REDUCE DONE parameters to the frameParameters list
		if(instToDPDone instanceof EXPOSE_REDUCE_DONE)
		{
			reduceDone = (EXPOSE_REDUCE_DONE)instToDPDone;

			frameParameters.setReducedFilename(reduceDone.getFilename());
			frameParameters.setSeeing(reduceDone.getSeeing());
			frameParameters.setCounts(reduceDone.getCounts());
			frameParameters.setXPix(reduceDone.getXpix());
			frameParameters.setYPix(reduceDone.getYpix());
			frameParameters.setPhotometricity(reduceDone.getPhotometricity());
			frameParameters.setSkyBrightness(reduceDone.getSkyBrightness());
			frameParameters.setSaturation(reduceDone.getSaturation());
		}
		else
		{
			frameParameters.setReducedFilename(null);
			frameParameters.setSeeing(0.0f);
			frameParameters.setCounts(0.0f);
			frameParameters.setXPix(0.0f);
			frameParameters.setYPix(0.0f);
			frameParameters.setPhotometricity(0.0f);
			frameParameters.setSkyBrightness(0.0f);
			frameParameters.setSaturation(false);
			o.error(this.getClass().getName()+":reduceFrame:"+reduceCommand+
				":Done messsage not sub-class of EXPOSE_REDUCE_DONE");
			telFocusDone.setSeeing(0.0f);
			telFocusDone.setCurrentFocus(0.0f);
			telFocusDone.setA(0.0f);
			telFocusDone.setB(0.0f);
			telFocusDone.setC(0.0f);
			telFocusDone.setChiSquared(0.0f);
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2003);
			telFocusDone.setErrorString(this.getClass().getName()+":reduceFrame:"+reduceCommand+
				":Done messsage not sub-class of EXPOSE_REDUCE_DONE");
			telFocusDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to send an acknowledgement back to the ISS, after a frame has been reduced.
	 * An instance of TELFOCUS_DP_ACK is constructed and returned to the client (the ISS).
	 * The filename sent is the reduced filename in frameParameters.
	 * The timeToComplete is set to the the <b>perExposureOverhead</b> plus the default acknowledge time.
	 * The other fields are copied from the frameParameters argument.
	 * @param telFocusCommand The instance of COMMAND that caused this TELFOCUS to occur.
	 * @param telFocusDone The DONE command that will be sent back to the ISS. Filled in with
	 * 	an error message if this method fails.
	 * @param frameParameters The frame parameters for this frame. The following fields are retrieved and copied
	 * 	into the TELFOCUS_DP_ACK, reducedFilename, counts, seeing, Xpix, Ypix. They should have been
	 * 	set by the reduceFrame method.
	 * @return The method returns true if it succeeded. It returns false if it failed (an IOException occured
	 * 	whilst sending the ACK), an error is logged and telFocusDone is filled in with the relevant error
	 * 	code/string.
	 */
	private boolean sendDpAcknowledge(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone,
					TELFOCUSFrameParameters frameParameters)
	{
		TELFOCUS_DP_ACK telFocusDpAck = null;

	// send acknowledge to say frame is completed.
		telFocusDpAck = new TELFOCUS_DP_ACK(telFocusCommand.getId());
		telFocusDpAck.setTimeToComplete(exposureTime+perExposureOverhead+
			serverConnectionThread.getDefaultAcknowledgeTime());
		telFocusDpAck.setFilename(frameParameters.getReducedFilename());
		telFocusDpAck.setCounts(frameParameters.getCounts());
		telFocusDpAck.setSeeing(frameParameters.getSeeing());
		telFocusDpAck.setXpix(frameParameters.getXPix());
		telFocusDpAck.setYpix(frameParameters.getYPix());
		telFocusDpAck.setPhotometricity(frameParameters.getPhotometricity());
		telFocusDpAck.setSkyBrightness(frameParameters.getSkyBrightness());
		telFocusDpAck.setSaturation(frameParameters.getSaturation());
		try
		{
			serverConnectionThread.sendAcknowledge(telFocusDpAck);
		}
		catch(IOException e)
		{
			o.error(this.getClass().getName()+
				":processCommand:sendDpAcknowledge:"+telFocusCommand+":"+e.toString());
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2007);
			telFocusDone.setErrorString(e.toString());
			telFocusDone.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * This method takes the data supplied by reducing the frames, and tries to fit an
	 * imaginary quadratic curve through a graph of x = focus, y = seeing. It uses ngat.math.QuadraticFit,
	 * which uses a three degree of freedom parameter search Chi Squared Fit. The best focus
	 * is derived from this using the first derivative (find x when the slope of the curve is zero i.e.
	 * the bottom of the curve). The calculated best seeing is got by plugging the best focus back into
	 * the model to get a y value (seeing).
	 * @param telFocusCommand The command we are implementing. Used for logging.
	 * @param telFocusDone The done message. We fill in the currentFocus/seeing done parameters with the
	 * 	results calulated here.
	 * @param list A list, containing instances of TELFOCUSFrameParameters. The focus/seeing 
	 * 	combinations are extracted from the list and used to make the quadratic fit.
	 * @see ngat.math.QuadraticFit
	 * @see #getFocus
	 * @see #quadraticY
	 */
	protected boolean quadraticFit(TELFOCUS telFocusCommand,TELFOCUS_DONE telFocusDone,Vector list)
	{
		QuadraticFit quadraticFit = null;
		TELFOCUSFrameParameters frameParameters = null;
		String propertyString = null;
		float focus,seeing;
		double targetChiSquared,parameterStepCount;
		double a,b,c,chiSquared;
		double parameterStartMinValue[] = new double[QuadraticFit.PARAMETER_COUNT];
		double parameterStartMaxValue[] = new double[QuadraticFit.PARAMETER_COUNT];
		double parameterStartStepSize[] = new double[QuadraticFit.PARAMETER_COUNT];
		int loopCount;

	// get configuration parameter - loop count
		try
		{
			propertyString = "o.telfocus.quadratic_fit.loop_count";
			loopCount = status.getPropertyInteger(propertyString);
		}
		catch (Exception e)
		{
			loopCount = 10;
			o.error(this.getClass().getName()+":quadraticFit:getting loop count configuration failed:"+
				"\n\t:"+propertyString+":using default value:"+loopCount+"\n\t"+e);
		}
	// get configuration parameter - target chi squared
		try
		{
			propertyString = "o.telfocus.quadratic_fit.target_chi_squared";
			targetChiSquared = status.getPropertyDouble(propertyString);
		}
		catch (Exception e)
		{
			targetChiSquared = 0.01;
			o.error(this.getClass().getName()+":quadraticFit:"+
				"getting target chi squared configuration failed:"+
				"\n\t:"+propertyString+":using default value:"+targetChiSquared+"\n\t"+e);
		}
	// get configuration parameter - parameter step count
		try
		{
			propertyString = "o.telfocus.quadratic_fit.parameter_step_count";
			parameterStepCount = status.getPropertyDouble(propertyString);
		}
		catch (Exception e)
		{
			parameterStepCount = QuadraticFit.DEFAULT_PARAMETER_STEP_COUNT;
			o.error(this.getClass().getName()+":quadraticFit:"+
				"getting parameter step count configuration failed:"+
				"\n\t:"+propertyString+":using default value:"+parameterStepCount+"\n\t"+e);
		}
	// get configuration parameters - parameter start ranges
		for(int i=0;i<QuadraticFit.PARAMETER_COUNT;i++)
		{
			try
			{
				propertyString = "o.telfocus.quadratic_fit."+
					QuadraticFit.PARAMETER_NAME_LIST[i]+".start_min";
				parameterStartMinValue[i] = status.getPropertyDouble(propertyString);
			}
			catch (Exception e)
			{
				parameterStartMinValue[i] = QuadraticFit.DEFAULT_PARAMETER_START_MIN_VALUE[i];
				o.error(this.getClass().getName()+":quadraticFit:"+
					"getting parameter start min value configuration for parameter "+i+" failed:"+
					"\n\t:"+propertyString+":using default value:"+parameterStartMinValue[i],e);
			}
			try
			{
				propertyString = "o.telfocus.quadratic_fit."+
					QuadraticFit.PARAMETER_NAME_LIST[i]+".start_max";
				parameterStartMaxValue[i] = status.getPropertyDouble(propertyString);
			}
			catch (Exception e)
			{
				parameterStartMaxValue[i] = QuadraticFit.DEFAULT_PARAMETER_START_MAX_VALUE[i];
				o.error(this.getClass().getName()+":quadraticFit:"+
					"getting parameter start max value configuration for parameter "+i+" failed:"+
					"\n\t:"+propertyString+":using default value:"+parameterStartMaxValue[i],e);
			}
			try
			{
				propertyString = "o.telfocus.quadratic_fit."+
					QuadraticFit.PARAMETER_NAME_LIST[i]+".start_step_size";
				parameterStartStepSize[i] = status.getPropertyDouble(propertyString);
			}
			catch (Exception e)
			{
				parameterStartStepSize[i] = QuadraticFit.DEFAULT_PARAMETER_START_STEP_SIZE[i];
				o.error(this.getClass().getName()+":quadraticFit:"+
					"getting parameter start step size configuration for parameter "+i+" failed:"+
					"\n\t:"+propertyString+":using default value:"+parameterStartStepSize[i],e);
			}
		}// end for on parameters
	// quadratic fit - construct
		quadraticFit = new QuadraticFit();
	// set parameter step count - print log if requested
		quadraticFit.setParameterStepCount(parameterStepCount);
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+telFocusCommand.getClass().getName()+
		      ":quadraticFit:parameter step count = "+parameterStepCount+".");
	// set parameter start values - print log if requested
		for(int i=0;i<QuadraticFit.PARAMETER_COUNT;i++)
		{
			quadraticFit.setParameterStartValues(i,parameterStartMinValue[i],parameterStartMaxValue[i],
							     parameterStartStepSize[i]);
			o.log(Logging.VERBOSITY_VERBOSE,"Command:"+telFocusCommand.getClass().getName()+
			      ":quadraticFit:parameter start values:"+QuadraticFit.PARAMETER_NAME_LIST[i]+
			      ":min = "+parameterStartMinValue[i]+":max = "+parameterStartMaxValue[i]+
			      ":step size = "+parameterStartStepSize[i]+".");
		}
	// set data points to fit - print log if required
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+telFocusCommand.getClass().getName()+
		      ":quadraticFit:data points.");
		for(int i = 0; i < list.size(); i++)
		{
			frameParameters = (TELFOCUSFrameParameters)list.get(i);
			focus = frameParameters.getFocus();
			seeing = frameParameters.getSeeing();
			quadraticFit.addPoint(focus,seeing);
			o.log(Logging.VERBOSITY_VERBOSE,"\tx(focus) = "+focus+",y(seeing) = "+seeing+".");
		}// end for on reduced exposures
	// do quadratic fit - print log if requested
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+telFocusCommand.getClass().getName()+
		      ":quadraticFit:loop count = "+loopCount+
		      ":target chi squared = "+targetChiSquared+".");
		quadraticFit.quadraticFit(loopCount,targetChiSquared);
	// get computed best fit parameter values - print log if requested
		a = quadraticFit.getA();
		b = quadraticFit.getB();
		c = quadraticFit.getC();
		// Note, getChiSquared assumes one three degree of freedom method called.
		chiSquared = quadraticFit.getChiSquared();
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+telFocusCommand.getClass().getName()+
		      ":quadraticFit:"+"\n\ta = "+a+":b = "+b+":c = "+c+":chi squared ="+chiSquared+".");
	// get focus - use 1st differential to find bottom of curve
		focus = (float)getFocus(a,b);
	// plug focus back in as x position to get model seeing at this focus.
		seeing = (float)quadraticY(focus,a,b,c);
		o.log(Logging.VERBOSITY_VERBOSE,"Command:"+telFocusCommand.getClass().getName()+
		      ":quadraticFit:"+"\n\tfocus = "+focus+":seeing = "+seeing+".");
	// test focus is in range min - max
		if((focus < startFocus) || (focus > endFocus))
		{
			String errorString = new String(this.getClass().getName()+":quadraticFit:computed focus:"+
							focus+" not in range ("+startFocus+","+endFocus+").");
			o.error(errorString);
			telFocusDone.setSeeing(0.0f);
			telFocusDone.setCurrentFocus(0.0f);
			telFocusDone.setA(0.0f);
			telFocusDone.setB(0.0f);
			telFocusDone.setC(0.0f);
			telFocusDone.setChiSquared(0.0f);
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2004);
			telFocusDone.setErrorString(errorString);
			telFocusDone.setSuccessful(false);
			return false;
		}
	// test seeing is sensible i.e. > 0
		if(seeing < 0.0f)
		{
			String errorString = new String(this.getClass().getName()+":quadraticFit:computed seeing:"+
							seeing+" too good.");
			o.error(errorString);
			telFocusDone.setSeeing(0.0f);
			telFocusDone.setCurrentFocus(0.0f);
			telFocusDone.setA(0.0f);
			telFocusDone.setB(0.0f);
			telFocusDone.setC(0.0f);
			telFocusDone.setChiSquared(0.0f);
			telFocusDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2005);
			telFocusDone.setErrorString(errorString);
			telFocusDone.setSuccessful(false);
			return false;
		}
	// return fwhm telFocusDone.setSeeing and best focus currentFocus. Chi-Squared parameters.
		telFocusDone.setSeeing(seeing);
		telFocusDone.setCurrentFocus(focus);
		telFocusDone.setA(a);
		telFocusDone.setB(b);
		telFocusDone.setC(c);
		telFocusDone.setChiSquared(chiSquared);
		return true;
	}

	/**
	 * Method to return y = a*(x*x) + (b*x) +c.
	 * @param x The value of x to get a model y value for.
	 * @param a The a parameter in the model.
	 * @param b The b parameter in the model.
	 * @param c The c parameter in the model.
	 * @return The model value of y.
	 */
	public double quadraticY(double x,double a,double b,double c)
	{
		return (a*(x*x))+(b*x)+c;
	}

	/**
	 * Method to return the best focus position for the telescope.
	 * This is done using computed values of <b>a</b> and <b>b</b> in the equation:
	 * y = a(x*x) + bx +c. This is differentiated to give the rate of change of y: 
	 * 2ax + b, which is zero at the best focus. Therefore this method returns:
	 * (-b)/(2a).
	 * @param a The computed <b>a</b> coefficient in the quadratic. If a is nearly zero an exception is thrown.
	 * @param b The computed <b>b</b> coefficient in the quadratic.
	 * @return The focus.
	 * @exception IllegalArgumentException Thrown if a is nearly zero, to prevent NaN being returned as the
	 * 	focus.
	 * @see #NEARLY_ZERO
	 */
	public double getFocus(double a,double b) throws IllegalArgumentException
	{
		if(Math.abs(a) < NEARLY_ZERO)
			throw new IllegalArgumentException(this.getClass().getName()+":getFocus:a nearly zero:"+a+".");
		return (-b)/(2*a);
	}

	/**
	 * Inner class for TELFOCUS. Stores all the data needed for each frame of the TELFOCUS.
	 */
	class TELFOCUSFrameParameters
	{
		/**
	 	 * The focus used for this frame.
	 	 */
		private float focus;
		/**
	 	 * The filename used for this frame.
	 	 */
		private String filename;
		/**
	 	 * The filename used for this frame, after it has been reduced.
	 	 */
		private String reducedFilename;
		/**
	 	 * The seeing returned for this frame.
	 	 */
		private float seeing;
		/**
	 	 * The counts returned for this frame, for the brightest object in the frame.
	 	 */
		private float counts;
		/**
	 	 * The xpix returned for this frame, for the brightest object in the frame.
	 	 */
		private float xPix;
		/**
	 	 * The ypix returned for this frame, for the brightest object in the frame.
	 	 */
		private float yPix;
		/**
	 	 * The photometricity returned for this frame, usually set for standard fields only.
	 	 */
		private float photometricity;
		/**
	 	 * The sky brightness returned for this frame.
	 	 */
		private float skyBrightness;
		/**
	 	 * Whether the field is saturated or not.
	 	 */
		private boolean saturation;

		/**
	 	 * Default constructor.
	 	 */
		public TELFOCUSFrameParameters()
		{
			focus = 0.0f;
			filename = null;
			reducedFilename = null;
			seeing = 0.0f;
			counts = 0.0f;
			xPix = 0.0f;
			yPix = 0.0f;
			photometricity = 0.0f;
			skyBrightness = 0.0f;
			saturation = false;
		}

		/**
		 * Set method for focus.
		 */
		public void setFocus(float f)
		{
			focus = f;
		}

		/**
		 * Get method for focus.
		 */
		public float getFocus()
		{
			return focus;
		}

		/**
		 * Set method for filename.
		 */
		public void setFilename(String s)
		{
			filename = s;
		}

		/**
		 * Get method for filename.
		 */
		public String getFilename()
		{
			return filename;
		}

		/**
		 * Set method for reduced filename.
		 */
		public void setReducedFilename(String s)
		{
			reducedFilename = s;
		}

		/**
		 * Get method for reduced filename.
		 */
		public String getReducedFilename()
		{
			return reducedFilename;
		}

		/**
		 * Set method for seeing.
		 */
		public void setSeeing(float f)
		{
			seeing = f;
		}

		/**
		 * Get method for seeing.
		 */
		public float getSeeing()
		{
			return seeing;
		}

		/**
		 * Set method for counts.
		 */
		public void setCounts(float f)
		{
			counts = f;
		}

		/**
		 * Get method for counts.
		 */
		public float getCounts()
		{
			return counts;
		}

		/**
		 * Set method for xPix.
		 */
		public void setXPix(float f)
		{
			xPix = f;
		}

		/**
		 * Get method for xPix.
		 */
		public float getXPix()
		{
			return xPix;
		}

		/**
		 * Set method for yPix.
		 */
		public void setYPix(float f)
		{
			yPix = f;
		}

		/**
		 * Get method for yPix.
		 */
		public float getYPix()
		{
			return yPix;
		}

		/**
		 * Set method for photometricity.
		 */
		public void setPhotometricity(float f)
		{
			photometricity = f;
		}

		/**
		 * Get method for photometricity.
		 */
		public float getPhotometricity()
		{
			return photometricity;
		}

		/**
		 * Set method for skyBrightness.
		 */
		public void setSkyBrightness(float f)
		{
			skyBrightness = f;
		}

		/**
		 * Get method for skyBrightness.
		 */
		public float getSkyBrightness()
		{
			return skyBrightness;
		}

		/**
		 * Set method for saturation.
		 */
		public void setSaturation(boolean f)
		{
			saturation = f;
		}

		/**
		 * Get method for saturation.
		 */
		public boolean getSaturation()
		{
			return saturation;
		}
	}
}

//
// $Log: not supported by cvs2svn $
//
