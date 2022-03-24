// GLANCEImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/GLANCEImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.File;
import ngat.o.ccd.*;
import ngat.fits.FitsHeaderDefaults;
import ngat.message.base.*;
import ngat.message.ISS_INST.GLANCE;
import ngat.message.ISS_INST.GLANCE_DONE;

/**
 * This class provides the implementation for the GLANCE command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class GLANCEImplementation extends EXPOSEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: GLANCEImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor. 
	 */
	public GLANCEImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.GLANCE&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.GLANCE";
	}

	/**
	 * This method gets the GLANCE command's acknowledge time. The GLANCE command takes the exposure time
	 * plus the default acknowledge time plus the status's max readout time to implement.
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
		GLANCE glanceCommand = (GLANCE)command;
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(glanceCommand.getExposureTime()+status.getMaxReadoutTime()+
					      serverConnectionThread.getDefaultAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method implements the GLANCE command. 
	 * <ul>
	 * <li>It generates some FITS headers from the CCD setup, BSS and 
	 * the ISS and saves this to disc. The filename is a temporary one got from the &quot; o.file.glance.tmp &quot;
	 * configuration property.
	 * <li>It moves the fold mirror to the correct location.
	 * <li>It performs an exposure and saves the data from this to disc.
	 * <li>It calls unLockFile to remove the FITS lock file created by saveFitsHeader.
	 * <li>Note it does <b>NOT</b> call the Real Time Data Pipeline to reduce the data.
	 * </ul>
	 * The resultant filename or the relevant error code is put into the an object of class GLANCE_DONE and
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
	 * @see EXPOSEImplementation#reduceExpose
	 * @see FITSImplementation#status
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		GLANCE glanceCommand = (GLANCE)command;
		GLANCE_DONE glanceDone = new GLANCE_DONE(command.getId());
		File file = null;
		String filename = null;
		String obsType = null;

		if(testAbort(glanceCommand,glanceDone) == true)
			return glanceDone;
		if(checkNonWindowedSetup(glanceDone) == false)
			return glanceDone;
	// setup exposure status.
		status.setExposureCount(1);
		status.setExposureNumber(0);
		status.clearPauseResumeTimes();
	// get fits headers
		clearFitsHeaders();
		if(glanceCommand.getStandard())
			obsType = FitsHeaderDefaults.OBSTYPE_VALUE_STANDARD;
		else
			obsType = FitsHeaderDefaults.OBSTYPE_VALUE_EXPOSURE;
		if(setFitsHeaders(glanceCommand,glanceDone,obsType,glanceCommand.getExposureTime()) == false)
			return glanceDone;
		if(getFitsHeadersFromISS(glanceCommand,glanceDone) == false)
			return glanceDone;
		if(getFitsHeadersFromBSS(glanceCommand,glanceDone) == false)
			return glanceDone;
		if(testAbort(glanceCommand,glanceDone) == true)
			return glanceDone;
	// move the fold mirror to the correct location
		if(moveFold(glanceCommand,glanceDone) == false)
			return glanceDone;
		if(testAbort(glanceCommand,glanceDone) == true)
			return glanceDone;
	// get filename
		filename = status.getProperty("o.file.glance.tmp");
	// delete old file if it exists
		file = new File(filename);
		if(file.exists())
			file.delete();
	// save FITS headers
		if(saveFitsHeaders(glanceCommand,glanceDone,filename) == false)
		{
			unLockFile(glanceCommand,glanceDone,filename);
			return glanceDone;
		}
		if(testAbort(glanceCommand,glanceDone) == true)
		{
			unLockFile(glanceCommand,glanceDone,filename);
			return glanceDone;
		}
	// do glance
		status.setExposureFilename(filename);
		try
		{
			ccd.expose(true,-1,glanceCommand.getExposureTime(),filename);
		}
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":processCommand:"+command+":"+e.toString());
			glanceDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1000);
			glanceDone.setErrorString(e.toString());
			glanceDone.setSuccessful(false);
			unLockFile(glanceCommand,glanceDone,filename);
			return glanceDone;
		}
		// unlock FITS file lock locked by saveFitsHeaders
		if(unLockFile(glanceCommand,glanceDone,filename) == false)
			return glanceDone;
		status.setExposureNumber(1);
	// don't call pipeline to reduce data
		glanceDone.setCounts(0.0f);
		glanceDone.setFilename(filename);
		glanceDone.setSeeing(0.0f);
		glanceDone.setXpix(0.0f);
		glanceDone.setYpix(0.0f);
		glanceDone.setPhotometricity(0.0f);
		glanceDone.setSkyBrightness(0.0f);
		glanceDone.setSaturation(false);
		glanceDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		glanceDone.setErrorString("");
		glanceDone.setSuccessful(true);
	// return done object.
		return glanceDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
