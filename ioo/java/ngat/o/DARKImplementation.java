// DARKImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/DARKImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.DARK;
import ngat.message.ISS_INST.DARK_DONE;

/**
 * This class provides the implementation for the DARK command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class DARKImplementation extends CALIBRATEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: DARKImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public DARKImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.DARK&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.DARK";
	}

	/**
	 * This method gets the DARK command's acknowledge time. This returns the server connection threads 
	 * default acknowledge time plus the status's max readout time plus the dark exposure time.
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
		DARK darkCommand = (DARK)command;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(darkCommand.getExposureTime()+status.getMaxReadoutTime()+
			serverConnectionThread.getDefaultAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method implements the DARK command. It generates some FITS headers from the CCD setup, BSS and
	 * ISS and saves this to disc. It performs a dark exposure and saves the data from this to disc.
	 * It sends the generated FITS data to the Real Time Data Pipeline to get some data from it.
	 * The resultant data or the relevant error code is put into the an object of class DARK_DONE and
	 * returned. During execution of these operations the abort flag is tested to see if we need to
	 * stop the implementation of this command.
	 * @see CommandImplementation#testAbort
	 * @see FITSImplementation#checkNonWindowedSetup
	 * @see FITSImplementation#clearFitsHeaders
	 * @see FITSImplementation#setFitsHeaders
	 * @see FITSImplementation#getFitsHeadersFromISS
	 * @see FITSImplementation#getFitsHeadersFromBSS
	 * @see FITSImplementation#saveFitsHeaders
	 * @see FITSImplementation#unLockFile
	 * @see ngat.o.ccd.CCDLibrary#expose
	 * @see CALIBRATEImplementation#reduceCalibrate
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		DARK darkCommand = (DARK)command;
		DARK_DONE darkDone = new DARK_DONE(command.getId());
		OStatus status = null;
		String filename = null;

		status = o.getStatus();
		if(testAbort(command,darkDone) == true)
			return darkDone;
		if(checkNonWindowedSetup(darkDone) == false)
			return darkDone;
	// Clear the pause and resume times.
		status.clearPauseResumeTimes();
	// get fits headers
		clearFitsHeaders();
		if(setFitsHeaders(command,darkDone,FitsHeaderDefaults.OBSTYPE_VALUE_DARK,
			darkCommand.getExposureTime()) == false)
			return darkDone;
		if(getFitsHeadersFromISS(command,darkDone) == false)
			return darkDone;
		if(getFitsHeadersFromBSS(command,darkDone) == false)
			return darkDone;
		if(testAbort(command,darkDone) == true)
			return darkDone;
	// get a filename to store frame in
		oFilename.nextMultRunNumber();
		try
		{
			oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_DARK);
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":"+e.toString());
			darkDone.setFilename(filename);
			darkDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+901);
			darkDone.setErrorString(e.toString());
			darkDone.setSuccessful(false);
			return darkDone;
		}
		oFilename.nextRunNumber();
		filename = oFilename.getFilename();
		if(saveFitsHeaders(command,darkDone,filename) == false)
		{
			unLockFile(command,darkDone,filename);
			return darkDone;
		}
	// do exposure
		status.setExposureFilename(filename);
		try
		{
			ccd.expose(false,-1,darkCommand.getExposureTime(),filename);
		}
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":"+e.toString());
			darkDone.setFilename(filename);
			darkDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+900);
			darkDone.setErrorString(e.toString());
			darkDone.setSuccessful(false);
			unLockFile(command,darkDone,filename);
			return darkDone;
		}
		// unlock FITS lock file created by saveFitsHeaders
		if(unLockFile(command,darkDone,filename) == false)
			return darkDone;
	// Test abort status.
		if(testAbort(command,darkDone) == true)
			return darkDone;
	// Call pipeline to reduce data.
		if(reduceCalibrate(command,darkDone,filename) == false)
			return darkDone;
	// set return values to indicate success.
		darkDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		darkDone.setErrorString("");
		darkDone.setSuccessful(true);
	// return done object.
		return darkDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
