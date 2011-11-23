// CONFIGImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/CONFIGImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import ngat.message.base.*;
import ngat.message.INST_BSS.*;
import ngat.message.ISS_INST.*;

import ngat.o.ccd.*;
import ngat.phase2.*;
import ngat.util.logging.*;

/**
 * This class provides the implementation for the CONFIG command sent to a server using the
 * Java Message System. It extends SETUPImplementation.
 * @see SETUPImplementation
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class CONFIGImplementation extends SETUPImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: CONFIGImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor. 
	 */
	public CONFIGImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.CONFIG&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.CONFIG";
	}

	/**
	 * This method gets the CONFIG command's acknowledge time.
	 * This method returns an ACK with timeToComplete set to the &quot; o.config.acknowledge_time &quot;
	 * held in the O configuration file. 
	 * If this cannot be found/is not a valid number the default acknowledge time is used instead.
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set to a time (in milliseconds).
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OTCPServerConnectionThread#getDefaultAcknowledgeTime
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		ACK acknowledge = null;
		int timeToComplete = 0;

		acknowledge = new ACK(command.getId());
		try
		{
			timeToComplete += o.getStatus().getPropertyInteger("o.config.acknowledge_time");
		}
		catch(NumberFormatException e)
		{
			o.error(this.getClass().getName()+":calculateAcknowledgeTime:"+e);
			timeToComplete += serverConnectionThread.getDefaultAcknowledgeTime();
		}
		acknowledge.setTimeToComplete(timeToComplete);
		return acknowledge;
	}

	/**
	 * This method implements the CONFIG command. 
	 * <ul>
	 * <li>It checks the message contains a suitable OConfig object to configure the controller.
	 * <li>It gets the number of rows and columns from the loaded O properties file.
	 * <li>It gets binning information from the OConfig object passed with the command.
	 * <li>It gets windowing information from the OConfig object passed with the command.
	 * <li>It gets filter wheel filter names from the OConfig object and converts them to positions
	 * 	using a configuration file.
	 * <li>It sends the information to the SDSU CCD Controller to configure it.
	 * <li>If filter wheels are enabled, we call setFocusOffset to send a focus offset to the ISS.
	 * <li>It increments the unique configuration ID.
	 * </ul>
	 * An object of class CONFIG_DONE is returned. If an error occurs a suitable error message is returned.
	 * @see #testAbort
	 * @see ngat.phase2.OConfig
	 * @see O#getStatus
	 * @see FITSImplementation#setFocusOffset
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		CONFIG configCommand = null;
		OConfig config = null;
		Detector detector = null;
		CONFIG_DONE configDone = null;
		CCDLibrarySetupWindow windowList[] = new CCDLibrarySetupWindow[CCDLibrary.SETUP_WINDOW_COUNT];
		OStatus status = null;
		int numberColumns,numberRows,amplifier,deInterlaceSetting;
		int filterWheelPosition;
		boolean filterWheelEnable;

	// test contents of command.
		configCommand = (CONFIG)command;
		configDone = new CONFIG_DONE(command.getId());
		status = o.getStatus();
		if(testAbort(configCommand,configDone) == true)
			return configDone;
		if(configCommand.getConfig() == null)
		{
			o.error(this.getClass().getName()+":processCommand:"+command+":Config was null.");
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+800);
			configDone.setErrorString(":Config was null.");
			configDone.setSuccessful(false);
			return configDone;
		}
		if((configCommand.getConfig() instanceof OConfig) == false)
		{
			o.error(this.getClass().getName()+":processCommand:"+command+":Config has wrong class:"+
				configCommand.getConfig().getClass().getName());
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+801);
			configDone.setErrorString(":Config has wrong class:"+
				configCommand.getConfig().getClass().getName());
			configDone.setSuccessful(false);
			return configDone;
		}
	// get oConfig from configCommand.
		config = (OConfig)configCommand.getConfig();
	// get local detector copy
		detector = config.getDetector(0);
	// load other required config for dimension configuration from O properties file.
		try
		{
			numberColumns = status.getNumberColumns(detector.getXBin());
			numberRows = status.getNumberRows(detector.getYBin());
	                amplifier = getAmplifier(detector.getWindowFlags() > 0);
			deInterlaceSetting = getDeInterlaceSetting(detector.getWindowFlags() > 0);
			filterWheelEnable = status.getPropertyBoolean("o.config.filter_wheel.enable");
			filterWheelPosition = status.getFilterWheelPosition(config.getFilterWheel());
		}
	// CCDLibraryFormatException is caught and re-thrown by this method.
	// Other exceptions (IllegalArgumentException,NumberFormatException) are not caught here, 
	// but by the calling method catch(Exception e)
		catch(CCDLibraryFormatException e)
		{
			o.error(this.getClass().getName()+":processCommand:"+command+":",e);
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+802);
			configDone.setErrorString("processCommand:"+command+":"+e);
			configDone.setSuccessful(false);
			return configDone;
		}
	// test abort
		if(testAbort(configCommand,configDone) == true)
			return configDone;
	// check xbin and ybin: greater than zero (hardware restriction), xbin == ybin (pipeline restriction)
		if((detector.getXBin()<1)||(detector.getYBin()<1))
		{
			String errorString = null;

			errorString = new String("Illegal xBin and yBin:xBin="+detector.getXBin()+",yBin="+
						detector.getYBin());
			o.error(this.getClass().getName()+":processCommand:"+command+":"+errorString);
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+807);
			configDone.setErrorString(errorString);
			configDone.setSuccessful(false);
			return configDone;
		}
	// We can either bin, or window, but not both at once. We only check one binning direction - see above
		if((detector.getWindowFlags() > 0) && (detector.getXBin() > 1))
		{
			String errorString = null;

			errorString = new String("Illegal binning and windowing:xBin="+detector.getXBin()+",window="+
						 detector.getWindowFlags());
			o.error(this.getClass().getName()+":processCommand:"+command+":"+errorString);
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+808);
			configDone.setErrorString(errorString);
			configDone.setSuccessful(false);
			return configDone;
		}
	// setup window list from ccdConfig.
		for(int i = 0; i < detector.getMaxWindowCount(); i++)
		{
			Window w = null;

			if(detector.isActiveWindow(i))
			{
				w = detector.getWindow(i);
				if(w == null)
				{
					String errorString = new String("Window "+i+" is null.");

					o.error(this.getClass().getName()+":processCommand:"+
						command+":"+errorString);
					configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+803);
					configDone.setErrorString(errorString);
					configDone.setSuccessful(false);
					return configDone;
				}
				windowList[i] = new CCDLibrarySetupWindow(w.getXs(),w.getYs(),
									  w.getXe(),w.getYe());
			}
			else
			{
				windowList[i] = new CCDLibrarySetupWindow(-1,-1,-1,-1);
			}
		}
		if(testAbort(configCommand,configDone) == true)
			return configDone;
	// send dimension/filter wheel configuration to the SDSU controller
		try
		{
			ccd.setupDimensions(numberColumns,numberRows,detector.getXBin(),detector.getYBin(),
					    amplifier,deInterlaceSetting,detector.getWindowFlags(),windowList);
			if(testAbort(configCommand,configDone) == true)
				return configDone;
			if(filterWheelEnable)
			{
				ccd.filterWheelMove(filterWheelPosition);
			}
			else
			{
				o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
					":processCommand:Filter wheels not enabled:Filter wheels NOT moved.");
			}
		}
		catch(CCDLibraryNativeException e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":",e);
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+804);
			configDone.setErrorString(":processCommand:"+command+":"+e);
			configDone.setSuccessful(false);
			return configDone;
		}
	// test abort
		if(testAbort(configCommand,configDone) == true)
			return configDone;
	// send focus offset based on filter and other mechanisms
		if(filterWheelEnable)
		{
			try
			{
				setFocusOffset(configCommand.getId(),config.getFilterWheel());
			}
			catch(Exception e)
			{
				o.error(this.getClass().getName()+":processCommand:"+
					command+":setFocusOffset failed:",e);
				configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+805);
				configDone.setErrorString("setFocusOffset failed:"+e.toString());
				configDone.setSuccessful(false);
				return configDone;
			}
		}
		else
		{
			o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
				":processCommand:Filter wheels not enabled:Focus offset NOT set.");
		}
	// Increment unique config ID.
	// This is queried when saving FITS headers to get the CONFIGID value.
		try
		{
			status.incConfigId();
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+":processCommand:"+
				command+":Incrementing configuration ID:"+e.toString());
			configDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+806);
			configDone.setErrorString("Incrementing configuration ID:"+e.toString());
			configDone.setSuccessful(false);
			return configDone;
		}
	// Store name of configuration used in status object.
	// This is queried when saving FITS headers to get the CONFNAME value.
		status.setConfigName(config.getId());
	// setup return object.
		configDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		configDone.setErrorString("");
		configDone.setSuccessful(true);
	// return done object.
		return configDone;
	}

}
//
// $Log: not supported by cvs2svn $
//
