// CCDLibraryTestExposure.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ccd/test/CCDLibraryTestExposure.java,v 1.1 2011-11-23 10:59:34 cjm Exp $
package ngat.o.ccd.test;

import java.io.*;
import java.lang.*;
import java.text.*;
import java.util.*;
import ngat.fits.*;
import ngat.o.ccd.*;
import ngat.util.logging.*;

/**
 * This class tests the ngat.o.ccd.CCDLibrary class, which provides the Java interface for
 * the O SDSU CCD controller. It does a setup and exposure. 
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class CCDLibraryTestExposure implements Runnable
{

	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: CCDLibraryTestExposure.java,v 1.1 2011-11-23 10:59:34 cjm Exp $");
	/**
	 * Default device pathname /dev/astropci0.
	 */
	public final static String DEFAULT_DEVICE_PATHNAME = new String("/dev/astropci0");
	/**
	 * Constant defining number of columns in CCD for O. Currently defaulting to Hamamatsu value.
	 */
	public final static int DEFAULT_NCOLS = 532;
	/**
	 * Constant defining number of rows in CCD for O. Currently defaulting to Hamamatsu value.
	 */
	public final static int DEFAULT_NROWS = 64;
	/**
	 * Default target temperature in deg C.
	 */
	public final static double DEFAULT_TEMPERATURE = -110.0;

	/**
	 * The logger.
	 */
	protected Logger logger = null;
	/**
	 * Which interface device to communicate with the controller over, either TEXT (emulated) or PCI.
	 * Defaults to PCI.
	 * @see ngat.o.ccd.CCDLibrary#INTERFACE_DEVICE_PCI
	 * @see ngat.o.ccd.CCDLibrary#INTERFACE_DEVICE_TEXT
	 */
	protected int interfaceDevice = CCDLibrary.INTERFACE_DEVICE_PCI;
	/**
	 * /dev pathname to communicate with SDSU device driver.
	 * @see #DEFAULT_DEVICE_PATHNAME
	 */
	protected String devicePathname = DEFAULT_DEVICE_PATHNAME;
	/**
	 * The filename of the timing board '.lod' file to download to the SDSU controller.
	 * Defaults to /icc/bin/o/dsp/tim.lod.
	 */
	protected String timingFilename = new String("/icc/bin/o/dsp/tim.lod");
	/**
	 * The filename of the utility board '.lod' file to download to the SDSU controller.
	 * Defaults to /icc/bin/o/dsp/util.lod.
	 */
	protected String utilityFilename = new String("/icc/bin/o/dsp/util.lod");
	/**
	 * Number of columns to read out.
	 * @see #DEFAULT_NCOLS
	 */
	protected int nCols = DEFAULT_NCOLS;
	/**
	 * Number of rows to read out.
	 * @see #DEFAULT_NROWS
	 */
	protected int nRows = DEFAULT_NROWS;
	/**
	 * Target temperature in degrees C.
	 * @see #DEFAULT_TEMPERATURE
	 */
	protected double targetTemperature = DEFAULT_TEMPERATURE;
	/**
	 * The gain to use to setup.
	 * @see ngat.o.ccd.CCDLibrary#DSP_GAIN_ONE
	 */
	protected int gain = CCDLibrary.DSP_GAIN_ONE;
	/**
	 * The gain speed to use to setup.
	 */
	protected boolean gainSpeed = true;
	/**
	 * Whether to idle clock the CCD - setup parameter.
	 */
	protected boolean idle = true;
	/**
	 * Serial (X) binning. Default to 1.
	 */
	protected int nSBin = 1;
	/**
	 * Parallel (Y) binning. Default to 1.
	 */
	protected int nPBin = 1;
	/**
	 * De-Interlace setting.
	 * @see ngat.o.ccd.CCDLibrary#DSP_DEINTERLACE_SINGLE
	 */
	protected int deinterlaceSetting = CCDLibrary.DSP_DEINTERLACE_SINGLE;
	/**
	 * Amplifier setting.
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_TOP_LEFT
	 */
	protected int amplifier = CCDLibrary.DSP_AMPLIFIER_TOP_LEFT;
	/**
	 * The FITS filename to save the image in.
	 */
	protected String filename = null;
	/**
	 * How long to expose the IR CCD for.
	 */
	protected int exposureLength = 0;
	/**
	 * Whether to try an abort command or not.
	 */
	protected boolean abort = false;
	/**
	 * The instance of the class used to control the SDSU controller.
	 */
	protected CCDLibrary ccd = null;
	/**
	 * Boolean used to decide when to stop running this class as a thread.
	 */
	private boolean quit = false;
	/**
	 * The logging level/verbosity to use.
	 */
	protected int logFilterLevel = Logging.VERBOSITY_VERY_VERBOSE;
	/**
	 * Whether to do the setup.
	 */
	protected boolean doSetup = false;
	/**
	 * Whether to do shutdown.
	 */
	protected boolean doShutdown = false;
	/**
	 * Whether to do the exposure.
	 */
	protected boolean doExposure = false;

	/**
	 * Run method for this class, which implements Runnable. 
	 * The class is run from a thread, if abort is requested.
	 * This method waits for a keypress, and then calls the CCDLibrary abort exposure method.
	 * @see #abort
	 * @see #ccd
	 */
	public void run()
	{
		int retval = -1;

		if(abort == false)
			return;
		System.out.println("Press a key to abort:");
		try
		{
			while((System.in.available() == 0)&&(quit == false))
				Thread.yield();
			if(System.in.available() != 0)
				retval = System.in.read();
		}
		catch (IOException e)
		{
			System.err.println(this.getClass().getName()+":run:available/read:"+e);
			retval = -1;
		}
		if(retval >= 0)
		{
			try
			{
				System.out.println("CCD Library Test Exposure:About to abort exposure.");
				ccd.abort();
				System.out.println("CCD Library Test Exposure:Abort exposure completed.");
			}
			catch(CCDLibraryNativeException e)
			{
				System.err.println(this.getClass().getName()+":run:abort failed:"+e);
			}
		}
		System.out.println("Abort thread finished.");
	}

	/**
	 * Method to parse arguments.
	 * @param args The argument list.
	 * @see #abort
	 * @see #exposureLength
	 * @see #filename
	 * @see #interfaceDevice
	 * @see #help
	 * @see #nCols
	 * @see #nRows
	 * @see #timingFilename
	 * @see #doExposure
	 * @see #doSetup
	 * @see #doShutdown
	 */
	protected void parseArguments(String args[]) throws Exception
	{
		for(int i = 0;i < args.length; i++)
		{
			if(args[i].equals("-abort")||args[i].equals("-a"))
			{
				abort = true;
			}
			else if(args[i].equals("-device_pathname")||args[i].equals("-dp"))
			{
				if((i+1) < args.length)
				{
					devicePathname = args[i+1];
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
								    ":parseArguments:No device pathname specified.");
				}
			}
			else if(args[i].equals("-exposure"))
			{
				doExposure = true;
			}
			else if(args[i].equals("-exposure_length")||args[i].equals("-e"))
			{
				if((i+1) < args.length)
				{
					exposureLength = Integer.parseInt(args[i+1]);
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
							      ":parseArguments:No exposure length specified.");
				}
				
			}
			else if(args[i].equals("-filename")||args[i].equals("-f"))
			{
				if((i+1) < args.length)
				{
					filename = args[i+1];
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
							      ":parseArguments:No filename specified.");
				}
			}
			else if(args[i].equals("-help")||args[i].equals("-h"))
			{
				help();
				System.exit(1);
			}
			else if(args[i].equals("-interface_device")||args[i].equals("-id"))
			{
				if((i+1) < args.length)
				{
					// throws CCDLibraryFormatException
					interfaceDevice = ccd.interfaceDeviceFromString(args[i+1]);
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
								    ":parseArguments:No interface device specified.");
				}
			}
			else if(args[i].equals("-log_level")||args[i].equals("-l"))
			{
				if((i+1) < args.length)
				{
					logFilterLevel = Integer.parseInt(args[i+1]);
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
						   ":parseArguments:Cannot parse log level:"+args[i+1]);
				}
			}
			else if(args[i].equals("-ncols")||args[i].equals("-nc"))
			{
				if((i+1) < args.length)
				{
					nCols = Integer.parseInt(args[i+1]);
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
							      ":parseArguments:Integer representing nCols required.");
				}
				
			}
			else if(args[i].equals("-setup"))
			{
				doSetup = true;
			}
			else if(args[i].equals("-shutdown"))
			{
				doShutdown = true;
			}
			else if(args[i].equals("-nrows")||args[i].equals("-nr"))
			{
				if((i+1) < args.length)
				{
					nRows = Integer.parseInt(args[i+1]);
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
							      ":parseArguments:Integer representing nRows required.");
				}
			}
			else if(args[i].equals("-timing_filename")||args[i].equals("-tf"))
			{
				if((i+1) < args.length)
				{
					timingFilename = args[i+1];
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
							      ":parseArguments:No filename specified.");
				}
			}
			else if(args[i].equals("-utility_filename")||args[i].equals("-uf"))
			{
				if((i+1) < args.length)
				{
					utilityFilename = args[i+1];
					i++;
				}
				else
				{
					throw new IllegalArgumentException(this.getClass().getName()+
							      ":parseArguments:No filename specified.");
				}
			}
			else
			{
				throw new IllegalArgumentException(this.getClass().getName()+
								   ":parseArguments:Illegal argument:"+args[i]);
			}
		}
	}

	/**
	 * Method to initialise the logger.
	 * @see #logger
	 * @see #logFilterLevel
	 */
	protected void initLoggers()
	{
		LogHandler handler = null;
		BogstanLogFormatter blf = null;

		logger = LogManager.getLogger("ngat.o.ccd.CCDLibrary");
		// setup log formatter
		blf = new BogstanLogFormatter();
		blf.setDateFormat(new SimpleDateFormat("yyyy-MM-dd 'at' HH:mm:ss.SSS z"));
		// setup log handler
		handler = new ConsoleLogHandler(blf);
		handler.setLogLevel(Logging.ALL);
		logger.addHandler(handler);
		logger.setLogLevel(logFilterLevel);
	}

	/**
	 * Routine to save some basic FITS headers, later used when saving image data.
	 * @param filename The filename to save the FITS headers in.
	 * @param ncols The number of columns (unbinned).
	 * @param nrows The number of rows (unbinned).
	 * @param nSBin The serial (X) binning factor.
	 * @param nPBin The parallel (Y) binning factor.
	 * @param exposureLength Length of the exposure, in milliseconds.
	 * @exception FitsHeaderException Thrown if the header can't be saved to the filename.
	 */
	protected void setFITSHeaders(String filename,int ncols,int nrows,int nSBin,int nPBin,int exposureLength) 
		throws FitsHeaderException
	{
		FitsHeader fitsHeader = null;
		int index;

		fitsHeader = new FitsHeader();
		index = 0;
		fitsHeader.add("SIMPLE",new Boolean(true),"Valid FITS file",null,index++);
		fitsHeader.add("BITPIX",new Integer(16),"Bits per pixel","bits",index++);
		fitsHeader.add("NAXIS",new Integer(2),"Number of axes",null,index++);
		fitsHeader.add("NAXIS1",new Integer(ncols/nSBin),"Number of columns","pixels",index++);
		fitsHeader.add("NAXIS2",new Integer(nrows/nPBin),"Number of rows","pixels",index++);
		fitsHeader.add("BZERO",new Double(32768.0),"Offset","counts",index++);
		fitsHeader.add("BSCALE",new Double(1.0),"Scale",null,index++);
		fitsHeader.add("EXPTIME",new Double(((double)exposureLength)/1000.0),"Exposure Length",
			       "seconds",index++);
		fitsHeader.add("DATE-OBS",new Date(),"Date of observation","UTC",index++);
		fitsHeader.add("CCDBINX",new Integer(nSBin),"X Binning","pixels",index++);
		fitsHeader.add("CCDBINY",new Integer(nPBin),"Y Binning","pixels",index++);
		fitsHeader.writeFitsHeader(filename);
	}

	/**
	 * Help method.
	 */
	protected void help()
	{
		System.out.println("CCD Library Test Exposure Help:");
		System.out.println("java ngat.o.ccd.test.CCDLibraryTestExposure ");
		System.out.println("\t[-interface_device|-id <INTERFACE_DEVICE_TEXT|INTERFACE_DEVICE_PCI>]");
		System.out.println("\t[-device_pathname|-dp <filename>]");
		System.out.println("\t[-setup][-timing_filename|-tf <'.lod' filename>][-utility_filename|-uf <'.lod' filename>]");
		System.out.println("\t[-exposure][-ncols|-nc <integer>][-nrows|-nr <integer>]");
		System.out.println("\t[-f[ilename] <FITS filename>][-e[xposure_length] <millis>][-a[bort]]");
		System.out.println("\t[-shutdown]");
		System.out.println("\t-device specifies which interface to use:INTERFACE_DEVICE_TEXT|INTERFACE_DEVICE_PCI.");
		System.out.println("\t-log_level specifies the logging(0..5).");
		System.out.println("\t-filename specifies the FITS filename to save the image into.");
		System.out.println("\t-exposure_length specifies the length of exposure in milliseconds.");
		System.out.println("\t-abort enables aborting. Press a key during the exposure to abort.");
		System.out.println("\t-device_pathname specifies the device pathname, /dev/astropci<n> for PCI and a filename for the text interface device.");
		System.out.println("\t-setup is needed to setup the SDSU controller.");
 		System.out.println("\t-exposure takes an actual exposure.");
 		System.out.println("\t-shutdown switches off analogue power after the exposure.");
	}

	/**
	 * Main program. 
	 * <ul>
	 * <li>The arguments are parsed.
	 * <li>The library loggers are initialised.
	 * <li>The library is initialised.
	 * <li>The specified interface device is opened.
	 * <li>If abort is set, start a thread with the instance of this class as a Runnable.
	 * <li>Setup the SDSU Controller.
	 * <li>The FITS headers are saved to the specified filename.
	 * <li>The exposure is taken, and saved to the specified filename.
	 * <li>The setup is shutdown..
	 * <li>Close the interface device.
	 * </ul>
	 * @param args The arguments to the program. Should have 1, the device name.
	 * @see #initLoggers
	 * @see #devicePathname
	 * @see #filename
	 * @see #exposureLength
	 * @see #ccd
	 * @see #quit
	 * @see #setFITSHeaders
	 * @see #logFilterLevel
	 * @see #doExposure
	 * @see #doSetup
	 * @see #doShutdown
	 */
	public static void main(String args[])
	{
		CCDLibraryTestExposure clte = null;
		CCDLibrarySetupWindow[] windowList = new CCDLibrarySetupWindow[CCDLibrary.SETUP_WINDOW_COUNT];

		clte = new CCDLibraryTestExposure();
		clte.quit = false;
		// create library before parsing arguments, some arguments parsed with CCDLibrary methods.
		clte.ccd = new CCDLibrary();
		try
		{
			clte.parseArguments(args);
		}
		catch(Exception e)
		{
			System.err.println("CCD Library Test Exposure parse arguments failed:"+e);
			System.exit(1);
		}
		clte.initLoggers();
		clte.ccd.setLogFilterLevel(clte.logFilterLevel);
		clte.ccd.setTextPrintLevel(CCDLibrary.TEXT_PRINT_LEVEL_ALL);
		try
		{
			clte.ccd.initialise();
			clte.ccd.interfaceOpen(clte.interfaceDevice,clte.devicePathname);
			if(clte.abort)
			{
				Thread abortThread = new Thread(clte);
				abortThread.start();
			}
			if(clte.doSetup)
			{
				clte.ccd.setup(CCDLibrary.SETUP_LOAD_ROM,null,
					       CCDLibrary.SETUP_LOAD_FILENAME,0,clte.timingFilename,
					       CCDLibrary.SETUP_LOAD_FILENAME,0,clte.utilityFilename,
					       clte.targetTemperature,clte.gain,clte.gainSpeed,clte.idle);
			}
			else
			{
				clte.ccd.interfaceMemoryMap();
			}
			if(clte.doExposure)
			{
				clte.ccd.setupDimensions(clte.nCols,clte.nRows,clte.nSBin,clte.nPBin,
							 clte.amplifier,clte.deinterlaceSetting,0,windowList);
				clte.setFITSHeaders(clte.filename,clte.nCols,clte.nRows,clte.nSBin,clte.nPBin,
						    clte.exposureLength);
				clte.ccd.expose(true,-1L,clte.exposureLength,clte.filename);
			}
			if(clte.doShutdown)
				clte.ccd.setupShutdown();
			clte.ccd.interfaceClose();
		}
		catch(Exception e)
		{
			System.err.println("CCDLibrary Test Exposure failed:"+e);
			System.exit(2);
		}
		clte.quit = true;
		System.exit(0);
	}
}
//
// $Log: not supported by cvs2svn $
//
