// SETUPImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/SETUPImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import ngat.message.base.*;
import ngat.message.ISS_INST.SETUP_DONE;

/**
 * This class provides the generic implementation for SETUP commands sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SETUPImplementation extends FITSImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: SETUPImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * This method gets the SETUP command's acknowledge time. It returns the server connection 
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
	 * This method is a generic implementation for the SETUP command, that does nothing.
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
	       	// do nothing 
		SETUP_DONE setupDone = new SETUP_DONE(command.getId());

		setupDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		setupDone.setErrorString("");
		setupDone.setSuccessful(true);
		return setupDone;
	}
}

//
// $Log: not supported by cvs2svn $
//
