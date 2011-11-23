// SendConfigCommand.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/test/SendConfigCommand.java,v 1.1 2011-11-23 10:59:38 cjm Exp $
package ngat.o.test;

import java.lang.*;
import java.io.*;
import java.net.*;
import java.util.*;

import ngat.o.test.*;
import ngat.message.ISS_INST.*;
import ngat.phase2.*;
import ngat.util.*;

/**
 * This class send an O camera configuration to O. 
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class SendConfigCommand
{
	/**
	 * The default port number to send ISS commands to.
	 */
	static final int DEFAULT_O_PORT_NUMBER = 7979;
	/**
	 * The default port number for the (fake ISS) server, to get commands from the O from.
	 */
	static final int DEFAULT_SERVER_PORT_NUMBER = 7383;
	/**
	 * The filename of a current filter wheel property file.
	 */
	private String filename = null;
	/**
	 * The ip address to send the messages read from file to, this should be the machine the FrodoSpec is on.
	 */
	private InetAddress address = null;
	/**
	 * The port number to send commands from the file to the O.
	 */
	private int oPortNumber = DEFAULT_O_PORT_NUMBER;
	/**
	 * The port number for the server, to recieve commands from the O.
	 */
	private int serverPortNumber = DEFAULT_SERVER_PORT_NUMBER;
	/**
	 * The server class that listens for connections from the O.
	 */
	private SicfTCPServer server = null;
	/**
	 * The stream to write error messages to - defaults to System.err.
	 */
	private PrintStream errorStream = System.err;
	/**
	 * X Binning of configuration. Defaults to 1.
	 */
	private int xBin = 1;
	/**
	 * Y Binning of configuration. Defaults to 1.
	 */
	private int yBin = 1;
	/**
	 * Whether exposures taken using this configuration, should do a calibration (dark) frame
	 * before the exposure.
	 */
	private boolean calibrateBefore = false;
	/**
	 * Whether exposures taken using this configuration, should do a calibration (dark) frame
	 * after the exposure.
	 */
	private boolean calibrateAfter = false;
	/**
	 * A property list of filter wheel properties.
	 */
	private NGATProperties filterWheelProperties = null;
	/**
	 * filter wheel string. Defaults to null, which will cause the Ccs
	 * to return an error.
	 */
	private String filterString = null;

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
	 * Routine to load the current filter wheel properties from filename
	 * into filterWheelProperties.
	 * @exception FileNotFoundException Thrown if the load failed.
	 * @exception IOException Thrown if the load failed.
	 * @see #filename
	 * @see #filterWheelProperties
	 */
	private void loadCurrentFilterProperties() throws FileNotFoundException, IOException
	{
		filterWheelProperties = new NGATProperties();
		filterWheelProperties.load(filename);
	}

	/**
	 * Routine to select and return a random filter from the wheel,
	 * using the loaded filter property database.
	 * @return A string, a filter type, that is present in the wheel according to
	 * 	the loaded properties.
	 * @exception NGATPropertyException Thrown if a property retrieve fails.
	 * @see #filterWheelProperties
	 */
	private String selectRandomFilter() throws NGATPropertyException
	{
		int positionCount,position;
		Random random = null;
		String filterType;

		positionCount = filterWheelProperties.getInt("filterwheel.0.count");
		random = new Random();
		position = random.nextInt(positionCount);
		filterType = (String)(filterWheelProperties.get("filterwheel.0."+position+".type"));
		return filterType;
	}

	/**
	 * This routine creates a CONFIG command. This object
	 * has a OConfig phase2 object with it, this is created and it's fields initialised.
	 * @return An instance of CONFIG.
	 * @see #filterString
	 * @see #xBin
	 * @see #yBin
	 * @see #calibrateBefore
	 * @see #calibrateAfter
	 */
	private CONFIG createConfig()
	{
		String string = null;
		CONFIG configCommand = null;
		OConfig oConfig = null;
		ODetector detector = null;

		configCommand = new CONFIG("Object Id");
		oConfig = new OConfig("Object Id");
	// detector for config
		detector = new ODetector();
		detector.setXBin(xBin);
		detector.setYBin(yBin);
		detector.setWindowFlags(0);
		oConfig.setDetector(0,detector);
	// filter
		oConfig.setFilterWheel(filterString);
	// InstrumentConfig fields.
		oConfig.setCalibrateBefore(calibrateBefore);
		oConfig.setCalibrateAfter(calibrateAfter);
	// CONFIG command fields
		configCommand.setConfig(oConfig);
		return configCommand;
	}

	/**
	 * This is the run routine. It creates a CONFIG object and sends it to the using a 
	 * SicfTCPClientConnectionThread, and awaiting the thread termination to signify message
	 * completion. 
	 * @return The routine returns true if the command succeeded, false if it failed.
	 * @exception Exception Thrown if an exception occurs.
	 * @see #loadCurrentFilterProperties
	 * @see #selectRandomFilter
	 * @see #createConfig
	 * @see SicfTCPClientConnectionThread
	 * @see #getThreadResult
	 */
	private boolean run() throws Exception
	{
		ISS_TO_INST issCommand = null;
		SicfTCPClientConnectionThread thread = null;
		boolean retval;

		if(filename != null)
		{
			loadCurrentFilterProperties();
			if(filterString == null)
				filterString = selectRandomFilter();
		}
		else
		{
			if(filterString == null)
				System.err.println("Program should fail:No filter specified.");
		}
		issCommand = (ISS_TO_INST)(createConfig());
		if(issCommand instanceof CONFIG)
		{
			CONFIG configCommand = (CONFIG)issCommand;
			OConfig oConfig = (OConfig)(configCommand.getConfig());
			System.err.println("CONFIG:"+
				oConfig.getFilterWheel()+":"+
				oConfig.getDetector(0).getXBin()+":"+
				oConfig.getDetector(0).getYBin()+".");
		}
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
			System.err.println("Done was null");
			retval = false;
		}
		else
		{
			if(thread.getDone().getSuccessful())
			{
				System.err.println("Done was successful");
				retval = true;
			}
			else
			{
				System.err.println("Done returned error("+thread.getDone().getErrorNum()+
					"): "+thread.getDone().getErrorString());
				retval = false;
			}
		}
		return retval;
	}

	/**
	 * This routine parses arguments passed into SendConfigCommand.
	 * @see #filename
	 * @see #oPortNumber
	 * @see #address
	 * @see #filterString
	 * @see #xBin
	 * @see #yBin
	 * @see #help
	 */
	private void parseArgs(String[] args)
	{
		for(int i = 0; i < args.length;i++)
		{
			if(args[i].equals("-file")||args[i].equals("-filename"))
			{
				if((i+1)< args.length)
				{
					filename = new String(args[i+1]);
					i++;
				}
				else
					errorStream.println("-filename requires a filename");
			}
			else if(args[i].equals("-f")||args[i].equals("-filter"))
			{
				if((i+1)< args.length)
				{
					filterString = args[i+1];
					i++;
				}
				else
					errorStream.println("-filter requires a filter name");
			}
			else if(args[i].equals("-h")||args[i].equals("-help"))
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
			else if(args[i].equals("-x")||args[i].equals("-xBin"))
			{
				if((i+1)< args.length)
				{
					xBin = Integer.parseInt(args[i+1]);
					i++;
				}
				else
					errorStream.println("-xBin requires a valid number.");
			}
			else if(args[i].equals("-y")||args[i].equals("-yBin"))
			{
				if((i+1)< args.length)
				{
					yBin = Integer.parseInt(args[i+1]);
					i++;
				}
				else
					errorStream.println("-yBin requires a valid number.");
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
		System.out.println("\t-file[name] <filename> - filter wheel filename.");
		System.out.println("\t-f[ilter] <filter type name> - Specify lower filter type.");
		System.out.println("\t-[ip]|[address] <address> - Address to send commands to.");
		System.out.println("\t-p[ort] <port number> - Port to send commands to.");
		System.out.println("\t-s[erverport] <port number> - Port for the CCS to send commands back.");
		System.out.println("\t-x[Bin] <binning factor> - X readout binning factor the CCD.");
		System.out.println("\t-y[Bin] <binning factor> - Y readout binning factor the CCD.");
		System.out.println("The default server port is "+DEFAULT_SERVER_PORT_NUMBER+".");
		System.out.println("The default O port is "+DEFAULT_O_PORT_NUMBER+".");
		System.out.println("The filters can be specified, otherwise if the filename is specified\n"+
			"the filters are selected randomly from that, otherwise 'null' is sent as a filter\n"+
			"and an error should occur.");
	}

	/**
	 * The main routine, called when SendConfigCommand is executed. This initialises the object, parses
	 * it's arguments, opens the filename, runs the run routine, and then closes the file.
	 * @see #parseArgs
	 * @see #init
	 * @see #run
	 */
	public static void main(String[] args)
	{
		boolean retval;
		SendConfigCommand scc = new SendConfigCommand();

		scc.parseArgs(args);
		scc.init();
		if(scc.address == null)
		{
			System.err.println("No O Address Specified.");
			scc.help();
			System.exit(1);
		}
		try
		{
			retval = scc.run();
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
