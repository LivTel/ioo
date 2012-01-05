// OTCPClientConnectionThread.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/OTCPClientConnectionThread.java,v 1.2 2012-01-05 17:01:23 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.*;
import java.net.*;
import java.util.Date;

import ngat.net.*;
import ngat.message.base.*;

/**
 * The OTCPClientConnectionThread extends TCPClientConnectionThread. 
 * It implements the generic ISS/DP(RT) instrument command protocol with multiple acknowledgements. 
 * O starts one of these threads each time it wishes to send a message to the ISS/DP(RT)/BSS.
 * @author Chris Mottram
 * @version $Revision: 1.2 $
 */
public class OTCPClientConnectionThread extends TCPClientConnectionThreadMA
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: OTCPClientConnectionThread.java,v 1.2 2012-01-05 17:01:23 cjm Exp $");
	/**
	 * The commandThread was spawned by the O to deal with a O command request. 
	 * As part of the running of
	 * the commandThread, this client connection thread was created. We need to know the server thread so
	 * that we can pass back any acknowledge times from the ISS/DpRt/BSS back to the O client (ISS/IcsGUI etc).
	 */
	private OTCPServerConnectionThread commandThread = null;
	/**
	 * The O object.
	 */
	private O o = null;

	/**
	 * A constructor for this class. Currently just calls the parent class's constructor.
	 * @param address The internet address to send this command to.
	 * @param portNumber The port number to send this command to.
	 * @param c The command to send to the specified address.
	 * @param ct The O command thread, the implementation of which spawned this command.
	 */
	public OTCPClientConnectionThread(InetAddress address,int portNumber,COMMAND c,OTCPServerConnectionThread ct)
	{
		super(address,portNumber,c);
		commandThread = ct;
	}

	/**
	 * Routine to set this objects pointer to the O object.
	 * @param o The O object.
	 */
	public void setO(O o)
	{
		this.o = o;
	}

	/**
	 * This routine processes the acknowledge object returned by the server. It
	 * prints out a message, giving the time to completion if the acknowledge was not null.
	 * It sends the acknowledgement to the O client for this sub-command of the command,
	 * so that the O's client does not time out if,say, a zero is returned.
	 * @see OTCPServerConnectionThread#sendAcknowledge
	 * @see #commandThread
	 */
	protected void processAcknowledge()
	{
		if(acknowledge == null)
		{
			o.error(this.getClass().getName()+":processAcknowledge:"+
				command.getClass().getName()+":acknowledge was null.");
			return;
		}
	// send acknowledge to O client.
		try
		{
			commandThread.sendAcknowledge(acknowledge);
		}
		catch(IOException e)
		{
			o.error(this.getClass().getName()+":processAcknowledge:"+
				command.getClass().getName()+":sending acknowledge to client failed:",e);
		}
	}

	/**
	 * This routine processes the done object returned by the server. 
	 * It prints out the basic return values in done.
	 */
	protected void processDone()
	{
		ACK acknowledge = null;

		if(done == null)
		{
			o.error(this.getClass().getName()+":processDone:"+
				command.getClass().getName()+":done was null.");
			return;
		}
	// construct an acknowledgement to sent to the O client to tell it how long to keep waiting
	// it currently returns the time the O origianally asked for to complete this command
	// This is because the O assumed zero time for all sub-commands.
		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(commandThread.getAcknowledgeTime());
		try
		{
			commandThread.sendAcknowledge(acknowledge);
		}
		catch(IOException e)
		{
			o.error(this.getClass().getName()+":processDone:"+
				command.getClass().getName()+":sending acknowledge to client failed:",e);
		}
	}
}
//
// $Log: not supported by cvs2svn $
// Revision 1.1  2011/11/23 10:55:24  cjm
// Initial revision
//
//
