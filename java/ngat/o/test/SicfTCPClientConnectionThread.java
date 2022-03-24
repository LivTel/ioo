// SicfTCPClientConnectionThread.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/test/SicfTCPClientConnectionThread.java,v 1.1 2011-11-23 10:59:38 cjm Exp $
package ngat.o.test;

import java.lang.*;
import java.io.*;
import java.net.*;

import ngat.net.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.*;

/**
 * The SicfTCPClientConnectionThread extends TCPClientConnectionThread. 
 * It implements the generic ISS instrument command protocol.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SicfTCPClientConnectionThread extends TCPClientConnectionThreadMA
{
	/**
	 * A constructor for this class. Currently just calls the parent class's constructor.
	 */
	public SicfTCPClientConnectionThread(InetAddress address,int portNumber,COMMAND c)
	{
		super(address,portNumber,c);
	}

	/**
	 * This routine processes the acknowledge object returned by the server. Currently
	 * prints out a message, giving the time to completion if the acknowledge was not null.
	 * Also calls an ACK print routine is needed.
	 * @see #printFilenameAck
	 * @see #printMovieAck
	 * @see #printMultRunAck
	 * @see #printMultRunDpAck
	 */
	protected void processAcknowledge()
	{
		if(acknowledge == null)
		{
			System.err.println(this.getClass().getName()+":processAcknowledge:"+
				command.getClass().getName()+":acknowledge was null.");
			return;
		}
		System.err.println(this.getClass().getName()+":processAcknowledge:"+
			command.getClass().getName()+":time:"+acknowledge.getTimeToComplete());
		if(acknowledge instanceof FILENAME_ACK)
			printFilenameAck((FILENAME_ACK)acknowledge);
		if(acknowledge instanceof MOVIE_ACK)
			printMovieAck((MOVIE_ACK)acknowledge);
		if(acknowledge instanceof MULTRUN_ACK)
			printMultRunAck((MULTRUN_ACK)acknowledge);
		if(acknowledge instanceof MULTRUN_DP_ACK)
			printMultRunDpAck((MULTRUN_DP_ACK)acknowledge);
	}

	/**
	 * Routine to print out more details if the acknowledge was a FILENAME_ACK. This is currently the
	 * returned filename.
	 * @param filenameAck The acknowledge object passed back to the client.
	 */
	private void printFilenameAck(FILENAME_ACK filenameAck)
	{
		System.out.println("printFilenameAck:filename ="+filenameAck.getFilename());
	}

	/**
	 * Routine to print out more details if the acknowledge was a MOVIE_ACK. This is currently the
	 * returned filename.
	 * @param movieAck The acknowledge object passed back to the client.
	 */
	private void printMovieAck(MOVIE_ACK movieAck)
	{
		System.out.println("printMovieAck:filename ="+movieAck.getFilename());
	}

	/**
	 * Routine to print out more details if the acknowledge was a MULTRUN_ACK. This is currently the
	 * returned filename.
	 * @param multRunAck The acknowledge object passed back to the client.
	 */
	private void printMultRunAck(MULTRUN_ACK multRunAck)
	{
		System.out.println("printMultRunAck:filename ="+multRunAck.getFilename());
	}

	/**
	 * Routine to print out more details if the acknowledge was a MULLTRUN_DP_ACK. This is currently the
	 * returned filename.
	 * @param multRunDpAck The acknowledge object passed back to the client.
	 */
	private void printMultRunDpAck(MULTRUN_DP_ACK multRunDpAck)
	{
		System.out.println("printMultRunDpAck:filename ="+multRunDpAck.getFilename());
	}

	/**
	 * This routine processes the done object returned by the server. Currently prints out
	 * the basic return values in done.
	 */
	protected void processDone()
	{

		if(done == null)
		{
			System.err.println(this.getClass().getName()+":processDone:"+
				command.getClass().getName()+":done was null.");
			return;
		}
		System.err.println(this.getClass().getName()+":processDone:"+
			command.getClass().getName()+":done has"+
			"\n\terror Number:"+done.getErrorNum()+
			"\n\terror String:"+done.getErrorString()+
			"\n\tsuccessful:"+done.getSuccessful());
		if(done instanceof CALIBRATE_DONE)
			System.err.println("\tFilename:"+((CALIBRATE_DONE)done).getFilename());
		if(done instanceof EXPOSE_DONE)
			System.err.println("\tFilename:"+((EXPOSE_DONE)done).getFilename());
	}
}
//
// $Log: not supported by cvs2svn $
//
