// BIASImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/BIASImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.BIAS_DONE;

/**
 * This class provides the implementation for the BIAS command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class BIASImplementation extends CALIBRATEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: BIASImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public BIASImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.BIAS&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.BIAS";
	}

	/**
	 * This method gets the BIAS command's acknowledge time. The BIAS command has no exposure time, 
	 * so this returns the server connection thread's default acknowledge time plus the status's max
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
	 * This method implements the BIAS command. It generates some FITS headers from the CCD setup, the BSS and
	 * ISS and saves this to disc. It performs a bias exposure and saves the data from this to disc.
	 * It sends the generated FITS data to the Real Time Data Pipeline to get some data from it.
	 * The resultant data or the relevant error code is put into the an object of class BIAS_DONE and
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
	 * @see ngat.o.ccd.CCDLibrary#bias
	 * @see CALIBRATEImplementation#reduceCalibrate
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		BIAS_DONE biasDone = new BIAS_DONE(command.getId());
		String filename = null;

		if(testAbort(command,biasDone) == true)
			return biasDone;
		if(checkNonWindowedSetup(biasDone) == false)
			return biasDone;
	// fits headers
		clearFitsHeaders();
		if(setFitsHeaders(command,biasDone,FitsHeaderDefaults.OBSTYPE_VALUE_BIAS,0) == false)
			return biasDone;
		if(getFitsHeadersFromISS(command,biasDone) == false)
			return biasDone;
		if(getFitsHeadersFromBSS(command,biasDone) == false)
			return biasDone;
		if(testAbort(command,biasDone) == true)
			return biasDone;
	// get a filename to store frame in
		oFilename.nextMultRunNumber();
		try
		{
			oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_BIAS);
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":"+e.toString());
			biasDone.setFilename(filename);
			biasDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+701);
			biasDone.setErrorString(e.toString());
			biasDone.setSuccessful(false);
			return biasDone;
		}
		oFilename.nextRunNumber();
		filename = oFilename.getFilename();
		if(saveFitsHeaders(command,biasDone,filename) == false)
		{
			unLockFile(command,biasDone,filename);
			return biasDone;
		}
	// do exposure
		try
		{
			ccd.bias(filename);
		}
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":"+e.toString());
			biasDone.setFilename(filename);
			biasDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+700);
			biasDone.setErrorString(e.toString());
			biasDone.setSuccessful(false);
			unLockFile(command,biasDone,filename);
			return biasDone;
		}
		// remove lock files created in saveFitsHeaders
		if(unLockFile(command,biasDone,filename) == false)
			return biasDone;
	// Test abort status.
		if(testAbort(command,biasDone) == true)
			return biasDone;
	// Call pipeline to reduce data.
		if(reduceCalibrate(command,biasDone,filename) == false)
			return biasDone;
	// set the done object to indicate success.
		biasDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		biasDone.setErrorString("");
		biasDone.setSuccessful(true);
	// return done object.
		return biasDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
