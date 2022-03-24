// TestNDFilterArduino.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ndfilter/test/TestNDFilterArduino.java,v 1.1 2013-06-04 08:35:00 cjm Exp $
package ngat.o.ndfilter.test;

import java.lang.*;
import java.io.*;
import java.net.*;
import java.text.*;

import ngat.o.ndfilter.*;
import ngat.util.*;
import ngat.util.logging.*;

/**
 * This class tests the IO:O ND filter arduino.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class TestNDFilterArduino
{
	/**
	 * Revision Control System id string, showing the version of the Class
	 */
	public final static String RCSID = new String("$Id: TestNDFilterArduino.java,v 1.1 2013-06-04 08:35:00 cjm Exp $");
	/**
	 * The NDFilterArduino instance.
	 */
	protected NDFilterArduino ndFilter = null;
	/**
	 * A string holding the IP address or hostname of the Arduino used to control the ND filters.
	 */
	protected String address = null;
	/**
	 * The integer holding the port number of the server on the Arduino used to control the ND filters.
	 */
	protected int portNumber = 23;
	/**
	 * Boolean, if true try to move the ND filters.
	 */
	protected boolean doMove = false;
	/**
	 * Which filter slide to move, or get the current position of, should be either 2|3.
	 */
	protected int filterSlide = 0;
	/**
	 * If we are moving the ND filters, the position to move to.
	 */
	protected boolean deploy = false;
	/**
	 * Boolean, if true try to get the current position.
	 */
	protected boolean doGetPosition = false;
	/**
	 * The logger.
	 */
	protected Logger logger = null;
	/**
	 * The filter used to filter messages sent to the logger.
	 * @see #logger
	 */
	protected BitFieldLogFilter logFilter = null;
	/**
	 * Logger log level.
	 * @see ngat.util.logging.Logging#VERBOSITY_VERY_VERBOSE
	 */
	protected int logFilterLevel = Logging.VERBOSITY_VERY_VERBOSE;

	/**
	 * Constructor.
	 */
	public TestNDFilterArduino()
	{
		super();
	}

	/**
	 * init method.
	 * <ul>
	 * <li>Construct new instance of NDFilterArduino.
	 * <li>We set the address and port number arguments to the ND filter.
	 * </ul>
	 * @exception UnknownHostException Thrown if the address is not known.
	 * @see #ndFilter
	 * @see #address
	 * @see #portNumber
	 * @see ngat.o.ndfilter.NDFilterArduino#setAddress
	 * @see ngat.o.ndfilter.NDFilterArduino#setPortNumber
	 */
	public void init() throws UnknownHostException
	{
		ndFilter = new NDFilterArduino();
		ndFilter.setAddress(address);
		ndFilter.setPortNumber(portNumber);
	}

	/**
	 * Run method.
	 * <ul>
	 * <li>If <b>doMove</b> is set, call ndFilter.move(filterSlide,deploy).
	 * <li>If <b>doGetPosition</b> is set, call ndFilter.getPosition(filterSlide) and print the result.
	 * </ul>
	 * @exception NDFilterArduinoException Thrown if an error occurs.
	 * @exception NDFilterArduinoMoveException Thrown if an error occurs whilst moving the filter slide.
	 * @exception IOException  Thrown if an error occurs whilst communicating with the Arduino.
	 * @see #doMove
	 * @see #filterSlide
	 * @see #deploy
	 * @see #ndFilter
	 * @see #doGetPosition
	 * @see ngat.o.ndfilter.NDFilterArduino#move
	 * @see ngat.o.ndfilter.NDFilterArduino#getPosition
	 */
	public void run() throws NDFilterArduinoException, NDFilterArduinoMoveException, IOException
	{
		int position;

		if(doMove)
		{
			System.out.println(this.getClass().getName()+":run:About to move filter slide "+filterSlide+
					   " to position:"+deploy);
			ndFilter.move(filterSlide,deploy);
			System.out.println(this.getClass().getName()+":run:move done.");
		}
		if(doGetPosition)
		{
			System.out.println(this.getClass().getName()+":run:About to get position of filter slide:"+
					   filterSlide);
			position = ndFilter.getPosition(filterSlide);
			System.out.println(this.getClass().getName()+":run:filter slide "+filterSlide+
					   " position is:"+position);
		}
	}

	/**
	 * Method to initialise the logger.
	 * @see #logger
	 * @see #logFilter
	 * @see #logFilterLevel
	 */
	protected void initLoggers()
	{
		LogHandler handler = null;
		BogstanLogFormatter blf = null;
		String loggerList[] = {"ngat.o.ndfilter.test.TestNDFilterArduino","ngat.o.ndfilter.NDFilterArduino",
				       "ngat.net.TelnetConnection"};

		// setup log formatter
		blf = new BogstanLogFormatter();
		blf.setDateFormat(new SimpleDateFormat("yyyy-MM-dd 'at' HH:mm:ss.SSS z"));
		// setup log handler
		handler = new ConsoleLogHandler(blf);
		handler.setLogLevel(Logging.ALL);
		// setup log filter
		logFilter = new BitFieldLogFilter(Logging.ALL);
		// Apply handler and filter to each logger in the list
		for(int i=0;i < loggerList.length;i++)
		{
			System.out.println(this.getClass().getName()+":initLoggers:Setting up logger:"+loggerList[i]);
			logger = LogManager.getLogger(loggerList[i]);
			logger.addHandler(handler);
			logger.setLogLevel(logFilterLevel);
			logger.setFilter(logFilter);
		}
		// reset logger instance variable to the test program's logger.
		logger = LogManager.getLogger("ngat.o.ndfilter.test.TestNDFilterArduino");
	}

	/**
	 * Parse command line arguments.
	 * @param args The command line argument list.
	 * @see #address
	 * @see #doGetPosition
	 * @see #doMove
	 * @see #help
	 * @see #logFilterLevel
	 * @see #filterSlide
	 * @see #deploy
	 */
	private void parseArgs(String[] args)
	{
		for(int i = 0; i < args.length;i++)
		{
			if(args[i].equals("-address")||args[i].equals("-a"))
			{
				if((i+1) < args.length)
				{
					address = args[i+1];
					i++;
				}
				else
				{
					System.err.println("-address should have an address argument.");
					System.exit(1);
				}
			}
			else if(args[i].equals("-get_position")||args[i].equals("-g"))
			{
				if((i+1) < args.length)
				{
					filterSlide = Integer.parseInt(args[i+1]);
					doGetPosition = true;
					i++;
				}
				else
				{
					System.err.println("-get_position <filter slide (2|3)>.");
					System.exit(1);
				}
			}
			else if(args[i].equals("-h")||args[i].equals("-help"))
			{
				help();
				System.exit(0);
			}
			else if(args[i].equals("-log_level")||args[i].equals("-log"))
			{
				if((i+1) < args.length)
				{
					logFilterLevel = Integer.parseInt(args[i+1]);
					i++;
				}
				else
				{
					System.err.println("-log_level should have an integer argument.");
					System.exit(1);
				}
			}
			else if(args[i].equals("-move")||args[i].equals("-m"))
			{
				if((i+2) < args.length)
				{
					filterSlide = Integer.parseInt(args[i+1]);
					if(args[i+2].equals("deploy"))
						deploy = true;
					else if(args[i+2].equals("stow"))
						deploy = false;
					else
					{
						System.err.println("-move: Second argument should be deploy|stow not"+
								   args[i+2]);
						System.exit(1);
					}
					doMove = true;
					i+= 2;
				}
				else
				{
					System.err.println("-move <filter slide> <deploy|stow>.");
					System.exit(1);
				}
			}
			else if(args[i].equals("-port_number")||args[i].equals("-p"))
			{
				if((i+1) < args.length)
				{
					portNumber = Integer.parseInt(args[i+1]);
					i++;
				}
				else
				{
					System.err.println("-port_number should have an integer argument.");
					System.exit(1);
				}
			}
			else
			{
				System.out.println(this.getClass().getName()+":Option not supported:"+args[i]);
				System.exit(1);
			}
		}
	}

	/**
	 * Help message routine.
	 */
	private void help()
	{
		System.out.println(this.getClass().getName()+" Help:");
		System.out.println("Options are:");
		System.out.println("\t-help");
		System.out.println("\t-a[ddress] <hostname|IP>");
		System.out.println("\t-g[et_position] <filter slide>");
		System.out.println("\t-log[_level] <n>");
		System.out.println("\t-m[ove] <filter slide> <deploy|stow>");
		System.out.println("\t-p[ort_number] <n>");
		System.out.println("");
		System.out.println("<filter slide> determines which ND filter to move, one of:2|3");
	}

	/**
	 * Main program.
	 * <ul>
	 * </ul>
	 * @see #parseArgs
	 * @see #initLoggers
	 * @see #init
	 * @see #run
	 */
	public static void main(String[] args)
	{
		TestNDFilterArduino test = new TestNDFilterArduino();

		try
		{
			test.parseArgs(args);
			test.initLoggers();
			if(test.address == null)
			{
				System.err.println("TestNDFilterArduino:main:No address specified.");
				test.help();
				System.exit(1);
			}
			if((test.doMove == false)&&(test.doGetPosition == false))
			{
				System.err.println("TestNDFilterArduino:main:Neither move nor get position selected.");
				test.help();
				System.exit(1);
			}
			test.init();
			test.run();
		}
		catch(Exception e)
		{
			System.err.println("TestNDFilterArduino failed:"+e);
			e.printStackTrace();
			System.exit(1);
		}
	}

}
//
// $Log: not supported by cvs2svn $
//
