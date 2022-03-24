// SendMultDarkCommand.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/test/SendMultDarkCommand.java,v 1.1 2011-11-23 10:59:38 cjm Exp $
package ngat.o.test;

import java.lang.*;
import java.io.*;
import java.net.*;
import java.util.*;

import ngat.o.*;
import ngat.message.ISS_INST.*;
import ngat.phase2.*;
import ngat.util.*;

/**
 * This class sends a MULTDARK command to O. 
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SendMultDarkCommand
{
	/**
	 * The default port number to send ISS commands to.
	 */
	static final int DEFAULT_O_PORT_NUMBER = 7979;
	/**
	 * The default port number for the server, to get commands from the O from.
	 */
	static final int DEFAULT_SERVER_PORT_NUMBER = 7383;
	/**
	 * The ip address to send the messages read from file to, this should be the machine the O is on.
	 */
	private InetAddress address = null;
	/**
	 * The port number to send commands from the file to O.
	 */
	private int oPortNumber = DEFAULT_O_PORT_NUMBER;
	/**
	 * The port number for the server, to recieve commands from O.
	 */
	private int serverPortNumber = DEFAULT_SERVER_PORT_NUMBER;
	/**
	 * The server class that listens for connections from O.
	 */
	private SicfTCPServer server = null;
	/**
	 * The stream to write error messages to - defaults to System.err.
	 */
	private PrintStream errorStream = System.err;
	/**
	 * Exposure length. Defaults to zero, which should cause MULTDARK to return an error.
	 */
	private int exposureLength = 0;
	/**
	 * Number of exposures for the MULTDARK to take. 
	 * Defaults to zero, which should cause MULTDARK to return an error.
	 */
	private int exposureCount = 0;

	/**
	 * This is the initialisation routine. This starts the server thread.
	 */
	private void init()
	{
		server = new SicfTCPServer(this.getClass().getName(),serverPortNumber);
		server.setController(this);
		server.start();
	}

	/**
	 * This routine creates a MULTDARK command. 
	 * @return An instance of MULTDARK.
	 * @see #exposureLength
	 * @see #exposureCount
	 */
	private MULTDARK createMultDark()
	{
		String string = null;
		MULTDARK multDarkCommand = null;

		multDarkCommand = new MULTDARK("SendMultDarkCommand");
		multDarkCommand.setExposureTime(exposureLength);
		multDarkCommand.setNumberExposures(exposureCount);
		return multDarkCommand;
	}

	/**
	 * This is the run routine. It creates a MULTDARK object and sends it to the using a 
	 * SicfTCPClientConnectionThread, and awaiting the thread termination to signify message
	 * completion. 
	 * @return The routine returns true if the command succeeded, false if it failed.
	 * @exception Exception Thrown if an exception occurs.
	 * @see #createMultDark
	 * @see SicfTCPClientConnectionThread
	 * @see #getThreadResult
	 * @see #address
	 * @see #oPortNumber
	 */
	private boolean run() throws Exception
	{
		ISS_TO_INST issCommand = null;
		SicfTCPClientConnectionThread thread = null;
		boolean retval;

		issCommand = (ISS_TO_INST)(createMultDark());
		thread = new SicfTCPClientConnectionThread(address,oPortNumber,issCommand);
		thread.start();
		while(thread.isAlive())
		{
			try
			{
				thread.join();
			}
			catch(InterruptedException e)
			{
				System.err.println("run:join interrupted:"+e);
			}
		}// end while isAlive
		retval = getThreadResult(thread);
		return retval;
	}

	/**
	 * Find out the completion status of the thread and print out the final status of some variables.
	 * @param thread The Thread to print some information for.
	 * @return The routine returns true if the thread completed successfully,
	 * 	false if some error occured.
	 */
	private boolean getThreadResult(SicfTCPClientConnectionThread thread)
	{
		boolean retval;

		if(thread.getAcknowledge() == null)
			System.err.println("Acknowledge was null");
		else
			System.err.println("Acknowledge with timeToComplete:"+
				thread.getAcknowledge().getTimeToComplete());
		if(thread.getDone() == null)
		{
			System.out.println("Done was null");
			retval = false;
		}
		else
		{
			if(thread.getDone().getSuccessful())
			{
				System.out.println("Done was successful");
				if(thread.getDone() instanceof MULTDARK_DONE)
				{
					System.out.println("\tFilename:"+
						((MULTDARK_DONE)(thread.getDone())).getFilename());
				}
				retval = true;
			}
			else
			{
				System.out.println("Done returned error("+thread.getDone().getErrorNum()+
					"): "+thread.getDone().getErrorString());
				retval = false;
			}
		}
		return retval;
	}

	/**
	 * This routine parses arguments passed into SendMultBiasCommand.
	 * @see #exposureLength
	 * @see #exposureCount
	 * @see #oPortNumber
	 * @see #address
	 * @see #help
	 */
	private void parseArgs(String[] args)
	{
		for(int i = 0; i < args.length;i++)
		{
			if(args[i].equals("-h")||args[i].equals("-help"))
			{
				help();
				System.exit(0);
			}
			else if(args[i].equals("-ip")||args[i].equals("-address"))
			{
				if((i+1)< args.length)
				{
					try
					{
						address = InetAddress.getByName(args[i+1]);
					}
					catch(UnknownHostException e)
					{
						System.err.println(this.getClass().getName()+":illegal address:"+
							args[i+1]+":"+e);
					}
					i++;
				}
				else
					errorStream.println("-address requires an address");
			}
			else if(args[i].equals("-l")||args[i].equals("-exposureLength"))
			{
				if((i+1)< args.length)
				{
					exposureLength = Integer.parseInt(args[i+1]);
					i++;
				}
				else
					errorStream.println("-exposureLength requires an argument.");
			}
			else if(args[i].equals("-n")||args[i].equals("-exposureCount"))
			{
				if((i+1)< args.length)
				{
					exposureCount = Integer.parseInt(args[i+1]);
					i++;
				}
				else
					errorStream.println("-exposureCount requires an argument.");
			}
			else if(args[i].equals("-p")||args[i].equals("-port"))
			{
				if((i+1)< args.length)
				{
					oPortNumber = Integer.parseInt(args[i+1]);
					i++;
				}
				else
					errorStream.println("-port requires a port number");
			}
			else if(args[i].equals("-s")||args[i].equals("-serverport"))
			{
				if((i+1)< args.length)
				{
					serverPortNumber = Integer.parseInt(args[i+1]);
					i++;
				}
				else
					errorStream.println("-serverport requires a port number");
			}
			else
				System.out.println(this.getClass().getName()+":Option not supported:"+args[i]);
		}
	}

	/**
	 * Help message routine.
	 */
	private void help()
	{
		System.out.println(this.getClass().getName()+" Help:");
		System.out.println("Options are:");
		System.out.println("\t-p[ort] <port number> - Port to send commands to.");
		System.out.println("\t-[ip]|[address] <address> - Address to send commands to.");
		System.out.println("\t-[l]|[exposureLength] <time in millis> - Specify exposure length.");
		System.out.println("\t-[n]|[exposureCount] <number> - Specify number of exposures.");
		System.out.println("\t-s[erverport] <port number> - Port for the FRODOSPEC to send commands back.");
		System.out.println("The default server port is "+DEFAULT_SERVER_PORT_NUMBER+".");
		System.out.println("The default O port is "+DEFAULT_O_PORT_NUMBER+".");
	}

	/**
	 * The main routine, called when SendMultDarkCommand is executed. This initialises the object, parses
	 * it's arguments, opens the filename, runs the run routine, and then closes the file.
	 * @see #parseArgs
	 * @see #init
	 * @see #run
	 */
	public static void main(String[] args)
	{
		boolean retval;
		SendMultDarkCommand smdc = new SendMultDarkCommand();

		smdc.parseArgs(args);
		smdc.init();
		if(smdc.address == null)
		{
			System.err.println("No O Address Specified.");
			smdc.help();
			System.exit(1);
		}
		try
		{
			retval = smdc.run();
		}
		catch (Exception e)
		{
			retval = false;
			System.err.println("run failed:"+e);

		}
		if(retval)
			System.exit(0);
		else
			System.exit(2);
	}
}
//
// $Log: not supported by cvs2svn $
//
