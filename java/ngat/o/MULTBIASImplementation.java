// MULTBIASImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/MULTBIASImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.*;
import java.util.*;
import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.MULTBIAS;
import ngat.message.ISS_INST.FILENAME_ACK;
import ngat.message.ISS_INST.CALIBRATE_DP_ACK;
import ngat.message.ISS_INST.MULTBIAS_DONE;

/**
 * This class provides the implementation for the MULTBIAS command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class MULTBIASImplementation extends CALIBRATEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: MULTBIASImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public MULTBIASImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.MULTBIAS&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.MULTBIAS";
	}

	/**
	 * This method gets the MULTBIAS command's acknowledge time. The MULTBIAS command has no exposure time, 
	 * so this returns the server connection threads default acknowledge time plus the status's max
	 * readout time.
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
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime()+
					      status.getMaxReadoutTime());
		return acknowledge;
	}

	/**
	 * This method implements the MULTBIAS command. 
	 * <ul>
	 * <li>We call checkNonWindowedSetup to ensure we are configured to read out the whole frame.
	 * <li>The status object's exposure count and exposure number are setup.
	 * <li>The filename object's Multrun and exposure code are setup.
	 * <li>For each bias frame:
	 *	<ul>
	 * 	<li>We generate some FITS headers from the CCD setup, the BSS and the ISS.
	 * 	<li>We generate a new FITS image and save the headers.
	 *      <li>We set the exposure filename.
	 * 	<li>Sets the time of bias frame and saves the Fits headers.
	 * 	<li>It performs an bias frame and saves the data from this to disc.
	 *      <li>We send a filename ACK containing the newly generated FITS filename.
	 * 	<li>Keeps track of the generated filenames in the list.
	 * 	</ul>
	 * <li>For each bias frame just taken:
	 *	<ul>
	 *      <li>We call reduceCalibrate to reduce the BIAS frame.
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
	 * @see ngat.o.ccd.CCDLibrary#bias
	 * @see CALIBRATEImplementation#reduceCalibrate
	 * @see OStatus#getMaxReadoutTime
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		MULTBIAS multBiasCommand = (MULTBIAS)command;
		MULTBIAS_DONE multBiasDone = new MULTBIAS_DONE(command.getId());
		FILENAME_ACK filenameAck = null;
		CALIBRATE_DP_ACK calibrateDpAck = null;
		List reduceFilenameList = null;
		String filename = null;
		int index;

		if(testAbort(multBiasCommand,multBiasDone) == true)
			return multBiasDone;
		if(checkNonWindowedSetup(multBiasDone) == false)
			return multBiasDone;
	// setup exposure status.
		status.setExposureCount(multBiasCommand.getNumberExposures());
		status.setExposureNumber(0);
	// get a filename to store frame in
		oFilename.nextMultRunNumber();
		try
		{
			oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_BIAS);
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+":processCommand:"+command+":"+e.toString(),e);
			multBiasDone.setFilename(filename);
			multBiasDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2600);
			multBiasDone.setErrorString(e.toString());
			multBiasDone.setSuccessful(false);
			return multBiasDone;
		}
	// do exposures
		index = 0;
		reduceFilenameList = new Vector();
		while(index < multBiasCommand.getNumberExposures())
		{
		// clear pause and resume times.
			status.clearPauseResumeTimes();
			// fits headers
			clearFitsHeaders();
			if(setFitsHeaders(multBiasCommand,multBiasDone,FitsHeaderDefaults.OBSTYPE_VALUE_BIAS,0,
					  multBiasCommand.getNumberExposures()) == false)
				return multBiasDone;
			if(getFitsHeadersFromISS(multBiasCommand,multBiasDone) == false)
				return multBiasDone;
			if(getFitsHeadersFromBSS(multBiasCommand,multBiasDone) == false)
				return multBiasDone;
			if(testAbort(multBiasCommand,multBiasDone) == true)
				return multBiasDone;
			// get a new filename.
			oFilename.nextRunNumber();
			filename = oFilename.getFilename();
			if(saveFitsHeaders(multBiasCommand,multBiasDone,filename) == false)
			{
				unLockFile(multBiasCommand,multBiasDone,filename);
				return multBiasDone;
			}
			status.setExposureFilename(filename);
			// do exposure
			try
			{
				ccd.bias(filename);
			}
			catch(CCDLibraryNativeException e)
			{
				o.error(this.getClass().getName()+":processCommand:"+
					multBiasCommand+":"+e.toString());
				multBiasDone.setFilename(filename);
				multBiasDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2603);
				multBiasDone.setErrorString(e.toString());
				multBiasDone.setSuccessful(false);
				unLockFile(command,multBiasDone,filename);
				return multBiasDone;
			}
			// remove lock files created in saveFitsHeaders
			if(unLockFile(multBiasCommand,multBiasDone,filename) == false)
				return multBiasDone;
			// Test abort status.
			if(testAbort(multBiasCommand,multBiasDone) == true)
				return multBiasDone;
		// send acknowledge to say frame is completed.
			filenameAck = new FILENAME_ACK(multBiasCommand.getId());
			filenameAck.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime()+
						      status.getMaxReadoutTime());
			filenameAck.setFilename(filename);
			try
			{
				serverConnectionThread.sendAcknowledge(filenameAck);
			}
			catch(IOException e)
			{
				o.error(this.getClass().getName()+
					":processCommand:sendAcknowledge:"+command+":"+e.toString(),e);
				multBiasDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2601);
				multBiasDone.setErrorString(e.toString());
				multBiasDone.setSuccessful(false);
				return multBiasDone;
			}
			status.setExposureNumber(index+1);
		// add filename to list for data pipeline processing.
			reduceFilenameList.add(filename);
		// test whether an abort has occured.
			if(testAbort(multBiasCommand,multBiasDone) == true)
			{
				return multBiasDone;
			}
			index++;
		}/* end while */
		// start reduction loop
		index = 0;
		while(index < multBiasCommand.getNumberExposures())
		{
			filename = (String)reduceFilenameList.get(index);
			// do reduction.
			// Call pipeline to reduce data.
			if(reduceCalibrate(multBiasCommand,multBiasDone,filename) == false)
				return multBiasDone;
			// send acknowledge to say frame has been reduced.
			calibrateDpAck = new CALIBRATE_DP_ACK(multBiasCommand.getId());
			calibrateDpAck.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime());
			// copy Data Pipeline results from DONE to ACK
			calibrateDpAck.setFilename(multBiasDone.getFilename());
			calibrateDpAck.setMeanCounts(multBiasDone.getMeanCounts());
			calibrateDpAck.setPeakCounts(multBiasDone.getPeakCounts());
			try
			{
				serverConnectionThread.sendAcknowledge(calibrateDpAck);
			}
			catch(IOException e)
			{
				o.error(this.getClass().getName()+
					":processCommand:sendAcknowledge(DP):"+command+":"+e.toString(),e);
				multBiasDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2602);
				multBiasDone.setErrorString(e.toString());
				multBiasDone.setSuccessful(false);
				return multBiasDone;
			}
			if(testAbort(multBiasCommand,multBiasDone) == true)
			{
				return multBiasDone;
			}
			index++;
		}// end while on MULTRUN exposures
	// return done object.
		multBiasDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		multBiasDone.setErrorString("");
		multBiasDone.setSuccessful(true);
		return multBiasDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
