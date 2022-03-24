// INTERRUPTImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/INTERRUPTImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import ngat.message.base.*;
import ngat.message.ISS_INST.INTERRUPT_DONE;
import ngat.o.ccd.*;

/**
 * This class provides the generic implementation for INTERRUPT commands sent to a server using the
 * Java Message System. It extends HardwareImplementation, as INTERRUPT commands need to be able to
 * talk to the hardware.
 * @see HardwareImplementation
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class INTERRUPTImplementation extends HardwareImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: INTERRUPTImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");
	/**
	 * Local copy of the O status object.
	 * @see O#getStatus
	 * @see OStatus
	 */
	protected OStatus status = null;

	/**
	 * This method calls the super-classes method, and tries to fill in the reference to the
	 * O status object.
	 * @param command The command to be implemented.
	 * @see #status
	 * @see O#getStatus
	 */
	public void init(COMMAND command)
	{
		super.init(command);
		if(o != null)
		{
			status = o.getStatus();
		}
	}

	/**
	 * This method gets the INTERRUPT command's acknowledge time. It returns the server connection 
	 * threads min acknowledge time. This method should be over-written in sub-classes.
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set.
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OTCPServerConnectionThread#getMinAcknowledgeTime
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(serverConnectionThread.getMinAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method is a generic implementation for the INTERRUPT command, that does nothing.
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
	       	// do nothing 
		INTERRUPT_DONE interruptDone = new INTERRUPT_DONE(command.getId());

		interruptDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		interruptDone.setErrorString("");
		interruptDone.setSuccessful(true);
		return interruptDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
