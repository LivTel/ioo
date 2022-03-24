// SicfTCPServerConnectionThread.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/test/SicfTCPServerConnectionThread.java,v 1.1 2011-11-23 10:59:38 cjm Exp $
package ngat.o.test;

import java.lang.*;
import java.io.*;
import java.net.*;
import java.util.*;

import ngat.fits.FitsHeaderDefaults;
import ngat.net.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.*;

/**
 * This class extends the TCPServerConnectionThread class for the SendISSCommandFile application. This
 * allows SendISSCommandFile to emulate the ISS's response to O sending it commands.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SicfTCPServerConnectionThread extends TCPServerConnectionThread
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: SicfTCPServerConnectionThread.java,v 1.1 2011-11-23 10:59:38 cjm Exp $");
	/**
	 * Default time taken to respond to a command.
	 */
	private final static int DEFAULT_ACKNOWLEDGE_TIME = 60*1000;
	/**
	 * File name containing FITS defaults properties for responding to a GET_FITS message.
	 */
	private final static String FITS_DEFAULTS_FILE_NAME = "./fits.properties";
	/**
	 * The controller object.
	 */
	private Object controller = null;

	/**
	 * Constructor of the thread. This just calls the superclass constructors.
	 * @param connectionSocket The socket the thread is to communicate with.
	 */
	public SicfTCPServerConnectionThread(Socket connectionSocket)
	{
		super(connectionSocket);
	}

	/**
	 * Routine to set this objects pointer to the controller object.
	 * @param c The controller object.
	 */
	public void setController(Object c)
	{
		this.controller = c;
	}

	/**
	 * This method calculates the time it will take for the command to complete and is called
	 * from the classes inherited run method.
	 */
	protected ACK calculateAcknowledgeTime()
	{
		ACK acknowledge = null;
		int time;

		acknowledge = new ACK(command.getId());
		if((command instanceof AG_START)||
			(command instanceof AG_STOP)||
			(command instanceof GET_FITS)||
			(command instanceof MOVE_FOLD)||
			(command instanceof OFFSET_FOCUS)||
			(command instanceof OFFSET_RA_DEC)||
			(command instanceof OFFSET_ROTATOR)||
			(command instanceof SET_FOCUS))
		{
			acknowledge.setTimeToComplete(DEFAULT_ACKNOWLEDGE_TIME);
		}
		else
			acknowledge.setTimeToComplete(DEFAULT_ACKNOWLEDGE_TIME);
		return acknowledge;
	}

	/**
	 * This method overrides the processCommand method in the ngat.net.TCPServerConnectionThread class.
	 * It is called from the inherited run method. It is responsible for performing the commands
	 * sent to it by the SendISSCommandFile. 
	 * It should also construct the done object to describe the results of the command.
	 */
	protected void processCommand()
	{
		if(command == null)
		{
			synchronized(errorStream)
			{
				errorStream.println("processCommand:command was null.");
			}
			done = new COMMAND_DONE(command.getId());
			done.setErrorNum(1);
			done.setErrorString("processCommand:command was null.");
			done.setSuccessful(false);
			return;
		}
		if(controller == null)
		{
			synchronized(errorStream)
			{
				errorStream.println("processCommand:controller was null.");
			}
			done = new COMMAND_DONE(command.getId());
			done.setErrorNum(2);
			done.setErrorString("processCommand:controller was null.");
			done.setSuccessful(false);
			return;
		}
	// setup return object.
		if(command instanceof AG_START)
		{
			// do nothing 
			AG_START_DONE agStartDone = new AG_START_DONE(command.getId());

			agStartDone.setErrorNum(0);
			agStartDone.setErrorString("");
			agStartDone.setSuccessful(true);
			done = agStartDone;
		}
		if(command instanceof AG_STOP)
		{
			// do nothing 
			AG_STOP_DONE agStopDone = new AG_STOP_DONE(command.getId());

			agStopDone.setErrorNum(0);
			agStopDone.setErrorString("");
			agStopDone.setSuccessful(true);
			done = agStopDone;
		}
		if(command instanceof GET_FITS)
		{
			// do nothing 
			GET_FITS_DONE getFitsDone = new GET_FITS_DONE(command.getId());
			Vector fitsHeaderList = null;
			FitsHeaderDefaults getFitsDefaults = null;

			try
			{
				getFitsDefaults = new FitsHeaderDefaults();
				getFitsDefaults.load(FITS_DEFAULTS_FILE_NAME);
				fitsHeaderList = getFitsDefaults.getCardImageList();
				getFitsDone.setFitsHeader(fitsHeaderList);
				getFitsDone.setErrorNum(0);
				getFitsDone.setErrorString("");
				getFitsDone.setSuccessful(true);
			}
			catch(Exception e)
			{
				fitsHeaderList = new Vector();
				getFitsDone.setFitsHeader(fitsHeaderList);
				getFitsDone.setErrorNum(3);
				getFitsDone.setErrorString("GET_FITS:Getting FITS defaults failed:"+e);
				getFitsDone.setSuccessful(false);
			}
			done = getFitsDone;
		}
		if(command instanceof MOVE_FOLD)
		{
			MOVE_FOLD moveFold = null;
			// do nothing 
			MOVE_FOLD_DONE moveFoldDone = new MOVE_FOLD_DONE(command.getId());

			moveFold = (MOVE_FOLD)command;
			System.out.println(command.getClass().getName()+" has port number "+
				moveFold.getMirror_position()+".");

			moveFoldDone.setErrorNum(0);
			moveFoldDone.setErrorString("");
			moveFoldDone.setSuccessful(true);
			done = moveFoldDone;
		}
		if(command instanceof OFFSET_FOCUS)
		{
			// do nothing 
			OFFSET_FOCUS_DONE offsetFocusDone = new OFFSET_FOCUS_DONE(command.getId());

			offsetFocusDone.setErrorNum(0);
			offsetFocusDone.setErrorString("");
			offsetFocusDone.setSuccessful(true);
			done = offsetFocusDone;
		}
		if(command instanceof OFFSET_RA_DEC)
		{
			// do nothing 
			OFFSET_RA_DEC_DONE offsetRaDecDone = new OFFSET_RA_DEC_DONE(command.getId());

			offsetRaDecDone.setErrorNum(0);
			offsetRaDecDone.setErrorString("");
			offsetRaDecDone.setSuccessful(true);
			done = offsetRaDecDone;
		}
		if(command instanceof OFFSET_ROTATOR)
		{
			// do nothing 
			OFFSET_ROTATOR_DONE offsetRotatorDone = new OFFSET_ROTATOR_DONE(command.getId());

			offsetRotatorDone.setErrorNum(0);
			offsetRotatorDone.setErrorString("");
			offsetRotatorDone.setSuccessful(true);
			done = offsetRotatorDone;
		}
		if(command instanceof SET_FOCUS)
		{
			// do nothing 
			SET_FOCUS_DONE setFocusDone = new SET_FOCUS_DONE(command.getId());

			setFocusDone.setErrorNum(0);
			setFocusDone.setErrorString("");
			setFocusDone.setSuccessful(true);
			done = setFocusDone;
		}
		System.out.println("Command:"+command.getClass().getName()+" Completed.");
	}
}
//
// $Log: not supported by cvs2svn $
//
