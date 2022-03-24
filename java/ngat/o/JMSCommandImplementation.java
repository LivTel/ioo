// JMSCommandImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/JMSCommandImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.String;
import ngat.message.base.*;

/**
 * This interface provides the generic implementation interface of a command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public interface JMSCommandImplementation
{
	/**
	 * This routine is called after the server has recieved a COMMAND sub-class that is the class of command
	 * this implementation class implements. This enables any initial startup to be performed before the 
	 * command is implemented.
	 * @param command The command passed to the server that is to be implemented.
	 */
	void init(COMMAND command);
	/**
	 * This routine is called by the server when it requires an acknowledge time to send back to the client.
	 * The implementation of this command should take less than the returned value, or else the processCommand
	 * method must call the server threads sendAcknowledge method to keep the server-client link alive.
	 * @param command The command to be implemented.
	 * @return An instance of a (sub)class of ngat.message.base.ACK, with the time, in milliseconds, 
	 * to complete the implementation of the command.
	 * @see ngat.message.base.ACK
	 */
	ACK calculateAcknowledgeTime(COMMAND command);
	/**
	 * This method is called to actually perform the implementation of the passed in command. It
	 * generates a done message that will be sent back to the client describing any errors that
	 * occured.
	 * @param command The command passed to the server for implementation.
	 * @return An object of (sub)class COMMAND_DONE is returned, with it's relevant fields filled in.
	 */
	COMMAND_DONE processCommand(COMMAND command);
}

//
// $Log: not supported by cvs2svn $
//
