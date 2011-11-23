// ABORTImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ABORTImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import ngat.o.ccd.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.ABORT_DONE;

/**
 * This class provides the implementation for the ABORT command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class ABORTImplementation extends INTERRUPTImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: ABORTImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public ABORTImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.ABORT&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.ABORT";
	}

	/**
	 * This method gets the ABORT command's acknowledge time. This takes the default acknowledge time to implement.
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
	 * This method implements the ABORT command. 
	 * <ul>
	 * <li>It tells the currently executing thread to abort itself.
	 * <li>If O is currently waiting for the SDSU CCD Controller to setup,expose or readout
	 * 	these are aborted with suitable libo_ccd routines.
	 * <li>If O is currently waiting for the SDSU CCD Controller to move/reset the filter wheel,
	 * 	this is aborted.
	 * </ul>
	 * An object of class ABORT_DONE is returned.
	 * @see OStatus#getCurrentThread
	 * @see OTCPServerConnectionThread#setAbortProcessCommand
	 * @see ngat.o.ccd.CCDLibrary#EXPOSURE_STATUS_NONE
	 * @see ngat.o.ccd.CCDLibrary#FILTER_WHEEL_STATUS_NONE
	 * @see ngat.o.ccd.CCDLibrary#setupAbort
	 * @see ngat.o.ccd.CCDLibrary#getSetupInProgress
	 * @see ngat.o.ccd.CCDLibrary#getExposureStatus
	 * @see ngat.o.ccd.CCDLibrary#abort
	 * @see ngat.o.ccd.CCDLibrary#filterWheelAbort
	 * @see ngat.o.ccd.CCDLibrary#filterWheelGetStatus
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		ngat.message.INST_DP.ABORT dprtAbort = new ngat.message.INST_DP.ABORT(command.getId());
		ABORT_DONE abortDone = new ABORT_DONE(command.getId());
		OTCPServerConnectionThread thread = null;
		OStatus status = null;
     		int readoutRemainingTime = 0;

	// tell the thread itself to abort at a suitable point
		status = o.getStatus();
		thread = (OTCPServerConnectionThread)status.getCurrentThread();
		if(thread != null)
			thread.setAbortProcessCommand();
	// if we are in the middle of a libo_ccd exposure command, abort it
		if(ccd.getExposureStatus() != ccd.EXPOSURE_STATUS_NONE)
		{
			try
			{
				ccd.abort();
			}
			catch (CCDLibraryNativeException e)
			{
				o.error(this.getClass().getName()+":Aborting exposure failed:"+e);
				abortDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2400);
				abortDone.setErrorString(e.toString());
				abortDone.setSuccessful(false);
				return abortDone;
			}
		}
	// If we are setting up the camera, abort the setup
		if(ccd.getSetupInProgress())
		{
			ccd.setupAbort();
		}
	// if we are moving the filter wheel, stop it
		try
		{
		// note we don't check o.config.filter_wheel.enable here,
		// if this is false the status must be FILTER_WHEEL_STATUS_NONE.
			if(ccd.filterWheelGetStatus() != ccd.FILTER_WHEEL_STATUS_NONE)
				ccd.filterWheelAbort();
		}
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":Aborting filter wheel failed:"+e);
			abortDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+2403);
			abortDone.setErrorString(e.toString());
			abortDone.setSuccessful(false);
			return abortDone;
		}
	// abort the dprt
		o.sendDpRtCommand(dprtAbort,serverConnectionThread);
	// return done object.
		abortDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		abortDone.setErrorString("");
		abortDone.setSuccessful(true);
		return abortDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
