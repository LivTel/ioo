// SicfTCPServer.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/test/SicfTCPServer.java,v 1.1 2011-11-23 10:59:38 cjm Exp $
package ngat.o.test;

import java.lang.*;
import java.io.*;
import java.net.*;

import ngat.net.*;

/**
 * This class extends the TCPServer class for the SendConfigCommand application. The SendConfigCommand sends
 * CONFIG commands to O to test it's execution. The CONFIG command involves sending commands back to the ISS, 
 * and this class is designed to catch these requests and to spawn a SicfTCPServerConnectionThread to deal with them.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SicfTCPServer extends TCPServer
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: SicfTCPServer.java,v 1.1 2011-11-23 10:59:38 cjm Exp $");
	/**
	 * Field holding the instance of the controller object currently executing this server, 
	 * so we can pass this to spawned threads.
	 */
	private Object controller = null;

	/**
	 * The constructor.
	 */
	public SicfTCPServer(String name,int portNumber)
	{
		super(name,portNumber);
	}

	/**
	 * Routine to set this objects pointer to the controller object.
	 * @param o The controller object.
	 * @see #controller
	 */
	public void setController(Object o)
	{
		this.controller = o;
	}

	/**
	 * This routine spawns threads to handle connection to the server. This routine
	 * spawns SicfTCPServerConnectionThread threads.
	 * @see SicfTCPServerConnectionThread
	 */
	public void startConnectionThread(Socket connectionSocket)
	{
		SicfTCPServerConnectionThread thread = null;

		thread = new SicfTCPServerConnectionThread(connectionSocket);
		thread.setController(controller);
		thread.start();
	}

}
//
// $Log: not supported by cvs2svn $
//
