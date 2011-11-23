// SAVEImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/SAVEImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.File;
import ngat.o.ccd.*;
import ngat.fits.FitsFilename;
import ngat.message.base.*;
import ngat.message.ISS_INST.SAVE;
import ngat.message.ISS_INST.SAVE_DONE;

/**
 * This class provides the implementation for the SAVE command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SAVEImplementation extends EXPOSEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: SAVEImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public SAVEImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.SAVE&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.SAVE";
	}

	/**
	 * This method gets the SAVE command's acknowledge time. The SAVE command renames the
	 * temporary file, and then reduces the data.This takes the default acknowledge time to implement.
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
	 * This method implements the SAVE command. 
	 * <ul>
	 * <li>It checks the temporary file exists, and renames it to a &quot;real&quot; file. The temporary file
	 * is specified in the &quot;o.file.glance.tmp&quot; property held in the O object.
	 * <li>It calls the Real Time Data Pipeline to reduce the data, if applicable.
	 * </ul>
	 * The resultant filename or the relevant error code is put into the an object of class SAVE_DONE and
	 * returned. During execution of these operations the abort flag is tested to see if we need to
	 * stop the implementation of this command.
	 * @see CommandImplementation#testAbort
	 * @see EXPOSEImplementation#reduceExpose
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		SAVE saveCommand = (SAVE)command;
		SAVE_DONE saveDone = new SAVE_DONE(command.getId());
		File temporaryFile = null;
		File newFile = null;
		String filename = null;

		if(testAbort(saveCommand,saveDone) == true)
			return saveDone;
	// get temporary filename
		filename = o.getStatus().getProperty("o.file.glance.tmp");
		temporaryFile = new File(filename);
	// does the temprary file exist?
		if(temporaryFile.exists() == false)
		{
			o.error(this.getClass().getName()+
					":processCommand:"+command+":file does not exist:"+filename);
			saveDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1700);
			saveDone.setErrorString("file does not exist:"+filename);
			saveDone.setSuccessful(false);
			return saveDone;
		}
	// get a filename to store frame in
		oFilename.nextMultRunNumber();
		try
		{
			if(saveCommand.getStandard())
				oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_STANDARD);
			else
				oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_EXPOSURE);
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+
					":processCommand:"+command+":"+e);
			saveDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1702);
			saveDone.setErrorString(e.toString());
			saveDone.setSuccessful(false);
			return saveDone;
		}
		oFilename.nextRunNumber();
		filename = oFilename.getFilename();
		newFile = new File(filename);
	// test abort
		if(testAbort(saveCommand,saveDone) == true)
			return saveDone;
	// rename temporary filename to filename
		if(temporaryFile.renameTo(newFile) == false)
		{
			o.error(this.getClass().getName()+
				":processCommand:"+command+":failed to rename '"+
				temporaryFile.toString()+"' to '"+newFile.toString()+"'.");
			saveDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1701);
			saveDone.setErrorString("Failed to rename '"+
				temporaryFile.toString()+"' to '"+newFile.toString()+"'.");
			saveDone.setSuccessful(false);
			return saveDone;
		}
	// setup done object
		saveDone.setCounts(0.0f);
		saveDone.setFilename(filename);// this is the new filename
		saveDone.setSeeing(0.0f);
		saveDone.setXpix(0.0f);
		saveDone.setYpix(0.0f);
		saveDone.setPhotometricity(0.0f);
		saveDone.setSkyBrightness(0.0f);
		saveDone.setSaturation(false);
		saveDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		saveDone.setErrorString("");
		saveDone.setSuccessful(true);
	// test abort
		if(testAbort(saveCommand,saveDone) == true)
			return saveDone;
	// call pipeline to reduce data
	// done values should be set by this routine.
		if(saveCommand.getPipelineProcess())
		{
			if(reduceExpose(saveCommand,saveDone,filename) == false)
				return saveDone;
		}
	// return done object.
		return saveDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
