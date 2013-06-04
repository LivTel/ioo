// NDFilterArduino.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ndfilter/NDFilterArduino.java,v 1.1 2013-06-04 08:34:07 cjm Exp $
package ngat.o.ndfilter;

import java.io.*;
import java.lang.*;
import java.net.*;
import ngat.net.*;
import ngat.util.logging.*;

/**
 * This class provides an interface to drive the IO:O neutral density filters. These are two independantly
 * driven slides controlled by an Arduino Ethernet + POE board. This has a telnet like interface and command set 
 * to move the two filters, and query their position.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class NDFilterArduino
{
	/**
	 * Revision Control System id string, showing the version of the Class
	 */
	public final static String RCSID = new String("$Id: NDFilterArduino.java,v 1.1 2013-06-04 08:34:07 cjm Exp $");
	/**
	 * Basic log level.
	 * @see ngat.util.logging.Logging#VERBOSITY_INTERMEDIATE
	 */
	public final static int LOG_LEVEL_NDFILTER_BASIC = Logging.VERBOSITY_INTERMEDIATE;
	/**
	 * Constant specifying an UNKNOWN slide position.
	 */
	protected final static int SLIDE_POSITION_UNKNOWN = -1;

// per instance variables
	/**
	 * The socket connection to the Arduino Ethernet + POE.
	 * @see ngat.net.TelnetConnection
	 */
	protected TelnetConnection connection = null;
	/**
	 * The logger to log messages to.
	 */
	protected Logger logger = null;

// constructor
	/**
	 * Constructor. Constructs the logger, and (telnet) connection.
	 * @see #logger
	 * @see #connection
	 */
	public NDFilterArduino()
	{
		super();
		logger = LogManager.getLogger(this);
		connection = new TelnetConnection();
	}

	/**
	 * Constructor.
	 * @param a The address of the telnet connection.
	 * @param p The port number of the telnet connection.
	 */
	public NDFilterArduino(InetAddress a,int p)
	{
		super();
		logger = LogManager.getLogger(this);
		connection = new TelnetConnection(a,p);
	}

	/**
	 * Set the IP address of the Arduino.
	 * @param a The IP address, as a InetAddress.
	 * @see #connection
	 * @see ngat.net.TelnetConnection#setAddress
	 * @see #logger
	 * @see #LOG_LEVEL_NDFILTER_BASIC
	 */
	public void setAddress(InetAddress a)
	{
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+":setAddress:"+a);
		connection.setAddress(a);
	}

	/**
	 * Set the IP address of the Arduino.
	 * @param addressName The IP address, as a String.
	 * @see #connection
	 * @see ngat.net.TelnetConnection#setAddress
	 * @see #logger
	 * @see #LOG_LEVEL_NDFILTER_BASIC
	 */
	public void setAddress(String addressName) throws UnknownHostException
	{
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+":setAddress:"+addressName);
		connection.setAddress(addressName);
	}

	/**
	 * Set the port number of the server on the Arduino.
	 * @param p The port number.
	 * @see #connection
	 * @see ngat.net.TelnetConnection#setPortNumber
	 * @see #logger
	 * @see #LOG_LEVEL_NDFILTER_BASIC
	 */
	public void setPortNumber(int p)
	{
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+":setPortNumber:"+p);
		connection.setPortNumber(p);
	}

	/**
	 * Move the specified ND filter (slide) to a specified position (deployed or stowed).
	 * <ul>
	 * <li>Check the filter slide number argument is valid.
	 * <li>Check the deploy argument is valid.
	 * <li>We open the telnet connection.
	 * <li>We construct a move command string.
	 * <li>We send the command string to the open connection using sendLine.
	 * <li>We receive a reply from the connection using readLine.
	 * <li>If the reply was non-null, we parse the reply (which should be a valid integer) to get the
	 *     error code returned by the arduino. If it is non-zero, we throw an exception.
	 * <li>If the reply is null, we failed to get a reply from the arduino, and throw an exception.
	 * <li>Finally, we always close the connection.
	 * </ul>
	 * Note the method is synchronized on the object instance, so two thread cannot access the move
	 * and getPosition simultaneously.
	 * @param filterSlide An integer resresenting which filter slide to move, one of : 2|3. The filter slides
	 *        are numbered from the detector upwards, 1 is IO:O's filter wheel.
	 * @param deploy An boolean, true means deploy the filter slide, false means stow the filter slide.
	 * @exception IOException Thrown if opening/closing/reading from the connection fails.
	 * @exception NullPointerException Thrown if opening/closing/reading from/writing to the connection fails.
	 * @exception NDFilterArduinoMoveException Thrown if the move returns a non-zero error code from the arduino,
	 *            or nothing is returned. Also thrown if the position is not legal,
	 * @see #connection
	 * @see #logger
	 * @see #isFilterSlide
	 * @see #LOG_LEVEL_NDFILTER_BASIC
	 * @see ngat.net.TelnetConnection#open
	 * @see ngat.net.TelnetConnection#sendLine
	 * @see ngat.net.TelnetConnection#readLine
	 * @see ngat.net.TelnetConnection#close
	 */
	public synchronized void move(int filterSlide, boolean deploy) throws IOException,NullPointerException,
							   NDFilterArduinoMoveException
	{
		int errorCode;
		String deployStowString = null;
		String commandString = null;
		String replyString = null;

		// check parameters
		if(!isFilterSlide(filterSlide))
		{
			throw new NDFilterArduinoMoveException(this.getClass().getName()+
							       ":move(filterSlide="+filterSlide+
							       "):Illegal filter slide argument.");
		}
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
			   ":move(filterSlide="+filterSlide+",deploy="+deploy+") started.");
		// open connection
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
			   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Opening connection.");
		connection.open();
		try
		{
			// send move command
			if(deploy)
				deployStowString = new String("deploy");
			else
				deployStowString = new String("stow");
			commandString = new String("move "+filterSlide+" "+deployStowString);
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Sending command:"+
				   commandString);
			connection.sendLine(commandString);
			// read reply
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Reading reply.");
			replyString = connection.readLine();
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):move returned:"+replyString);
			// check reply returned
			if(replyString != null)
			{
				// if reply returned parse error code
				logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
					   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Parsing error code.");
				errorCode = Integer.parseInt(replyString);
				logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
					   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Error code:"+
					   errorCode);
				if(errorCode != 0)
					throw new NDFilterArduinoMoveException(errorCode);
			}
			else // end of stream reached / no string returned.
			{
				throw new NDFilterArduinoMoveException(this.getClass().getName()+
								":move(filterSlide="+filterSlide+",deploy="+deploy+
								"):reply was null.");
			}
		}
		// always close connection even if move failed.
		finally
		{
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Closing connection.");
			connection.close();
		}
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
			   ":move(filterSlide="+filterSlide+",deploy="+deploy+"):Finished.");
	}

	/**
	 * Method to get the current position of the specified filter slide.
	 * <ul>
	 * <li>We open the telnet connection.
	 * <li>We send the get position command string to the open connection using sendLine.
	 * <li>We receive a reply from the connection using readLine.
	 * <li>If the reply was non-null, we parse the reply (which should be 0|1|-1) to get the
	 *     position, and return this as an integer. If the reply cannot be parsed, we throw an exception.
	 * <li>If the reply is null, we failed to get a reply from the arduino, and throw an exception.
	 * <li>Finally, we always close the connection.
	 * </ul>
	 * Note the method is synchronized on the object instance, so two thread cannot access the move
	 * and getPosition simultaneously.
	 * @param filterSlide An integer resresenting which filter slide to move, one of : 2|3. The filter slides
	 *        are numbered from the detector upwards, 1 is IO:O's filter wheel.
	 * @return The filter slide's current position, one of: 0|1|-1. Here, 0 represents stowed, 1 represents
	 *         deployed, and -1 signifies neither deployed nor stowed.
	 * @exception IOException Thrown if opening/closing/reading from the connection fails.
	 * @exception NullPointerException Thrown if opening/closing/reading from/writing to the connection fails.
	 * @exception NDFilterArduinoException Thrown if the get position command does not return a string, 
	 *            or returns a string that is not known.
	 * @exception NumberFormatException Thrown if parsing the reply as an integer fails.
	 * @see #connection
	 * @see #logger
	 * @see #isFilterSlide
	 * @see #isPosition
	 * @see #LOG_LEVEL_NDFILTER_BASIC
	 * @see ngat.net.TelnetConnection#open
	 * @see ngat.net.TelnetConnection#sendLine
	 * @see ngat.net.TelnetConnection#readLine
	 * @see ngat.net.TelnetConnection#close
	 */
	public synchronized int getPosition(int filterSlide) throws IOException, NullPointerException, 
								    NDFilterArduinoException, NumberFormatException
	{
		String commandString = null;
		String replyString = null;
		int position;

		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
			   ":getPosition(filterSlide="+filterSlide+"):Started.");
		// check parameters
		if(!isFilterSlide(filterSlide))
		{
			throw new NDFilterArduinoException(this.getClass().getName()+
							   ":move(filterSlide="+filterSlide+
							   "):Illegal filter slide argument.");
		}
		// open conenction
		logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
			   ":getPosition(filterSlide="+filterSlide+"):Opening connection.");
		connection.open();
		try
		{
			// send command
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":getPosition(filterSlide="+filterSlide+"):Sending command.");
			commandString = new String("get "+filterSlide+" position");
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":getPosition(filterSlide="+filterSlide+"):Sending command:"+
				   commandString);
			connection.sendLine(commandString);
			// get reply
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":getPosition(filterSlide="+filterSlide+"):Reading reply.");
			replyString = connection.readLine();
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":getPosition(filterSlide="+filterSlide+"):Read reply:"+replyString);
			if(replyString != null)
			{
				logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
					   ":getPosition(filterSlide="+filterSlide+"):Parsing reply:"+replyString);
				// if reply returned parse string
				position = Integer.parseInt(replyString);
				if(!isPosition(position))
				{
					throw new NDFilterArduinoException(this.getClass().getName()+
									   ":getPosition(filterSlide="+filterSlide+
									   "):Failed to parse reply:"+replyString);
				}
				return position;
			}
			else // end of stream reached / no string returned.
			{
				throw new NDFilterArduinoException(this.getClass().getName()+
								   ":getPosition(filterSlide="+filterSlide+
								   "):reply was null.");
			}
		}
		// always close connection even if get position failed.
		finally
		{
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":getPosition(filterSlide="+filterSlide+"):Closing connection.");
			connection.close();
			logger.log(LOG_LEVEL_NDFILTER_BASIC,this.getClass().getName()+
				   ":getPosition(filterSlide="+filterSlide+"):Finished.");
		}
	}


	/**
	 * Return whether the specified position integer is a valid filter slide position or not.
	 * Valid in this case includes "unknown" (-1).
	 * @param position The position integer to test.
	 * @return True if the specified position integer is a valid filter slide position (0|1|-1), 
	 *         false if it is not.
	 */
	protected boolean isPosition(int position)
	{
		return ((position == 0)||(position == 1)||(position == -1));
	}

	/**
	 * Return whether the specified filter slide number integer is a valid filter slide position or not.
	 * @param filterSlide The filter slide integer to test.
	 * @return True if the specified filter slide numberinteger is a valid filter slide, false if it is not.
	 */
	protected boolean isFilterSlide(int filterSlide)
	{
		return ((filterSlide == 2)||(filterSlide == 3));
	}
}
//
// $Log: not supported by cvs2svn $
//
