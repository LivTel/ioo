// MULTDARKImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/MULTDARKImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.*;
import java.util.*;
import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.MULTDARK;
import ngat.message.ISS_INST.FILENAME_ACK;
import ngat.message.ISS_INST.CALIBRATE_DP_ACK;
import ngat.message.ISS_INST.MULTDARK_DONE;

/**
 * This class provides the implementation for the MULTDARK command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class MULTDARKImplementation extends CALIBRATEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: MULTDARKImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public MULTDARKImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.MULTDARK&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.MULTDARK";
	}

	/**
	 * This method gets the MULTDARK command's acknowledge time. The MULTDARK command's exposure time is added 
	 * to the server connection threads default acknowledge time and the status's max readout time to make the 
	 * ACK time.
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set.
	 * @see #serverConnectionThread
	 * @see #status
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OStatus#getMaxReadoutTime
	 * @see OTCPServerConnectionThread#getDefaultAcknowledgeTime
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		MULTDARK multDarkCommand = (MULTDARK)command;
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(multDarkCommand.getExposureTime()+status.getMaxReadoutTime()+
					      serverConnectionThread.getDefaultAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method implements the MULTDARK command. 
	 * <ul>
	 * <li>We call checkNonWindowedSetup to ensure we are configured to read out the whole frame.
	 * <li>The status object's exposure count and exposure number are setup.
	 * <li>The filename object's Multrun and exposure code are setup.
	 * <li>For each dark frame:
	 *	<ul>
	 * 	<li>We generate some FITS headers from the CCD setup, the ISS and the BSS.
	 * 	<li>We generate a new FITS image and save the headers.
	 *      <li>We set the exposure filename.
	 * 	<li>Sets the time of dark frame and saves the Fits headers.
	 * 	<li>It performs an dark frame and saves the data from this to disc.
	 *      <li>We send a filename ACK containing the newly generated FITS filename.
	 * 	<li>Keeps track of the generated filenames in the list.
	 * 	</ul>
	 * <li>For each dark frame just taken:
	 *	<ul>
	 *      <li>We call reduceCalibrate to reduce the DARK frame.
	 *      <li>We send a CALIBRATE_DP_ACK back to the client.
	 *      </ul>
	 * <li>It sets up the return values to return to the client.
	 * </ul>
	 * During execution of these operations the abort flag is tested to see if we need to
	 * stop the implementation of this command.
	 * saveFitsHeaders now creates lock files for the FITS images being created, modified, and these lock files
	 * are removed after the raw data has been written to disk.
	 * @see #status
	 * @see #oFilename
	 * @see #ccd
	 * @see CommandImplementation#testAbort
	 * @see FITSImplementation#checkNonWindowedSetup
	 * @see FITSImplementation#clearFitsHeaders
	 * @see FITSImplementation#setFitsHeaders
	 * @see FITSImplementation#getFitsHeadersFromISS
	 * @see FITSImplementation#getFitsHeadersFromBSS
	 * @see FITSImplementation#saveFitsHeaders
	 * @see FITSImplementation#unLockFiles
	 * @see ngat.o.ccd.CCDLibrary#expose
	 * @see CALIBRATEImplementation#reduceCalibrate
	 * @see OStatus#getMaxReadoutTime
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		MULTDARK multDarkCommand = (MULTDARK)command;
		MULTDARK_DONE multDarkDone = new MULTDARK_DONE(command.getId());
		FILENAME_ACK filenameAck = null;
		CALIBRATE_DP_ACK calibrateDpAck = null;
		List reduceFilenameList = null;
		String filename = null;
		int index;


		if(testAbort(multDarkCommand,multDarkDone) == true)
			return multDarkDone;
		if(checkNonWindowedSetup(multDarkDone) == false)
			return multDarkDone;
	// setup exposure status.
		status.setExposureCount(multDarkCommand.getNumberExposures());
		status.setExposureNumber(0);
	// get a filename to store frame in
		oFilename.nextMultRunNumber();
		try
		{
			oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_DARK);
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+":processCommand:"+command+":"+e.toString(),e);
			multDarkDone.setFilename(filename);
			multDarkDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2700);
			multDarkDone.setErrorString(e.toString());
			multDarkDone.setSuccessful(false);
			return multDarkDone;
		}
	// do exposures
		index = 0;
		reduceFilenameList = new Vector();
		while(index < multDarkCommand.getNumberExposures())
		{
		// clear pause and resume times.
			status.clearPauseResumeTimes();
			// fits headers
			clearFitsHeaders();
			if(setFitsHeaders(multDarkCommand,multDarkDone,FitsHeaderDefaults.OBSTYPE_VALUE_DARK,
				multDarkCommand.getExposureTime(),multDarkCommand.getNumberExposures()) == false)
				return multDarkDone;
			if(getFitsHeadersFromISS(multDarkCommand,multDarkDone) == false)
				return multDarkDone;
			if(getFitsHeadersFromBSS(multDarkCommand,multDarkDone) == false)
				return multDarkDone;
			if(testAbort(multDarkCommand,multDarkDone) == true)
				return multDarkDone;
			// get a new filename.
			oFilename.nextRunNumber();
			filename = oFilename.getFilename();
			if(saveFitsHeaders(multDarkCommand,multDarkDone,filename) == false)
			{
				unLockFile(multDarkCommand,multDarkDone,filename);
				return multDarkDone;
			}
			status.setExposureFilename(filename);
			// do exposure
			try
			{
				ccd.expose(false,-1,multDarkCommand.getExposureTime(),filename);
			}
			catch(CCDLibraryNativeException e)
			{
				o.error(this.getClass().getName()+":processCommand:"+
					multDarkCommand+":"+e.toString());
				multDarkDone.setFilename(filename);
				multDarkDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2701);
				multDarkDone.setErrorString(e.toString());
				multDarkDone.setSuccessful(false);
				unLockFile(command,multDarkDone,filename);
				return multDarkDone;
			}
			// remove lock files created in saveFitsHeaders
			if(unLockFile(multDarkCommand,multDarkDone,filename) == false)
				return multDarkDone;
			// Test abort status.
			if(testAbort(multDarkCommand,multDarkDone) == true)
				return multDarkDone;
		// send acknowledge to say frame is completed.
			filenameAck = new FILENAME_ACK(multDarkCommand.getId());
			filenameAck.setTimeToComplete(multDarkCommand.getExposureTime()+status.getMaxReadoutTime()+
						      serverConnectionThread.getDefaultAcknowledgeTime());
			filenameAck.setFilename(filename);
			try
			{
				serverConnectionThread.sendAcknowledge(filenameAck);
			}
			catch(IOException e)
			{
				o.error(this.getClass().getName()+
					":processCommand:sendAcknowledge:"+command+":"+e.toString(),e);
				multDarkDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2702);
				multDarkDone.setErrorString(e.toString());
				multDarkDone.setSuccessful(false);
				return multDarkDone;
			}
			status.setExposureNumber(index+1);
		// add filename to list for data pipeline processing.
			reduceFilenameList.add(filename);
		// test whether an abort has occured.
			if(testAbort(multDarkCommand,multDarkDone) == true)
			{
				return multDarkDone;
			}
			index++;
		}/* end while */
		// start reduction loop
		index = 0;
		while(index < multDarkCommand.getNumberExposures())
		{
			filename = (String)reduceFilenameList.get(index);
			// do reduction.
			// Call pipeline to reduce data.
			if(reduceCalibrate(multDarkCommand,multDarkDone,filename) == false)
				return multDarkDone;
			// send acknowledge to say frame has been reduced.
			calibrateDpAck = new CALIBRATE_DP_ACK(multDarkCommand.getId());
			calibrateDpAck.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime());
			// copy Data Pipeline results from DONE to ACK
			calibrateDpAck.setFilename(multDarkDone.getFilename());
			calibrateDpAck.setMeanCounts(multDarkDone.getMeanCounts());
			calibrateDpAck.setPeakCounts(multDarkDone.getPeakCounts());
			try
			{
				serverConnectionThread.sendAcknowledge(calibrateDpAck);
			}
			catch(IOException e)
			{
				o.error(this.getClass().getName()+
					":processCommand:sendAcknowledge(DP):"+command+":"+e.toString(),e);
				multDarkDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2703);
				multDarkDone.setErrorString(e.toString());
				multDarkDone.setSuccessful(false);
				return multDarkDone;
			}
			if(testAbort(multDarkCommand,multDarkDone) == true)
			{
				return multDarkDone;
			}
			index++;
		}// end while on MULTRUN exposures
	// return done object.
		multDarkDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		multDarkDone.setErrorString("");
		multDarkDone.setSuccessful(true);
		return multDarkDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
