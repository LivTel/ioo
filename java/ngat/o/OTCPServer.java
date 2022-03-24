// OTCPServer.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/OTCPServer.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.*;
import java.net.*;

import ngat.net.*;

/**
 * This class extends the TCPServer class for the O application.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class OTCPServer extends TCPServer
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: OTCPServer.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");
	/**
	 * Field holding the instance of the O control system currently executing, 
	 * so we can pass this to spawned threads.
	 */
	private O o = null;

	/**
	 * The constructor.
	 */
	public OTCPServer(String name,int portNumber)
	{
		super(name,portNumber);
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
	 * This routine spawns threads to handle connection to the server. This routine
	 * spawns <a href="OTCPServerConnectionThread.html">OTCPServerConnectionThread</a> threads.
	 * The routine also sets the new threads priority to higher than normal. This makes the thread
	 * reading it's command a priority so we can quickly determine whether the thread should
	 * continue to execute at a higher priority.
	 * @see OTCPServerConnectionThread
	 */
	public void startConnectionThread(Socket connectionSocket)
	{
		OTCPServerConnectionThread thread = null;

		thread = new OTCPServerConnectionThread(connectionSocket);
		thread.setO(o);
		thread.setPriority(o.getStatus().getThreadPriorityInterrupt());
		thread.start();
	}

}
//
// $Log: not supported by cvs2svn $
//
