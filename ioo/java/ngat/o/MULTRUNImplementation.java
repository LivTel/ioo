// MULTRUNImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/MULTRUNImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $
package ngat.o;

import java.lang.*;
import java.io.File;
import java.io.IOException;
import java.util.Date;
import java.util.Vector;
import ngat.o.ccd.*;
import ngat.fits.*;
import ngat.message.base.*;
import ngat.message.ISS_INST.MULTRUN;
import ngat.message.ISS_INST.MULTRUN_ACK;
import ngat.message.ISS_INST.MULTRUN_DP_ACK;
import ngat.message.ISS_INST.MULTRUN_DONE;

/**
 * This class provides the implementation for the MULTRUN command sent to a server using the
 * Java Message System.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class MULTRUNImplementation extends EXPOSEImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: MULTRUNImplementation.java,v 1.1 2011-11-23 10:55:24 cjm Exp $");

	/**
	 * Constructor.
	 */
	public MULTRUNImplementation()
	{
		super();
	}

	/**
	 * This method allows us to determine which class of command this implementation class implements.
	 * This method returns &quot;ngat.message.ISS_INST.MULTRUN&quot;.
	 * @return A string, the classname of the class of ngat.message command this class implements.
	 */
	public static String getImplementString()
	{
		return "ngat.message.ISS_INST.MULTRUN";
	}

	/**
	 * This method returns the MULTRUN command's acknowledge time. Each frame in the MULTRUN takes 
	 * the exposure time plus the status's max readout time plus the default acknowledge time to complete. 
	 * The default acknowledge time
	 * allows time to setup the camera, get information about the telescope and save the frame to disk.
	 * This method returns the time for the first frame in the MULTRUN only, as a MULTRUN_ACK message
	 * is returned to the client for each frame taken.
	 * @param command The command instance we are implementing.
	 * @return An instance of ACK with the timeToComplete set.
	 * @see #serverConnectionThread
	 * @see #status
	 * @see ngat.message.base.ACK#setTimeToComplete
	 * @see OStatus#getMaxReadoutTime
	 * @see OTCPServerConnectionThread#getDefaultAcknowledgeTime
	 * @see MULTRUN#getExposureTime
	 * @see MULTRUN#getNumberExposures
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		MULTRUN multRunCommand = (MULTRUN)command;
		ACK acknowledge = null;

		acknowledge = new ACK(command.getId());
		acknowledge.setTimeToComplete(multRunCommand.getExposureTime()+status.getMaxReadoutTime()+
			serverConnectionThread.getDefaultAcknowledgeTime());
		return acknowledge;
	}

	/**
	 * This method implements the MULTRUN command. 
	 * <ul>
	 * <li>It moves the fold mirror to the correct location.
	 * <li>For each exposure it performs the following:
	 *	<ul>
	 * 	<li>It generates some FITS headers from the CCD setup, ISS and BSS. 
	 * 	<li>Sets the time of exposure and saves the Fits headers.
	 * 	<li>It performs an exposure and saves the data from this to disc.
	 * 	<li>Keeps track of the generated filenames in the list.
	 * 	</ul>
	 * <li>It sets up the return values to return to the client.
	 * </ul>
	 * The resultant filename or the relevant error code is put into the an object of class MULTRUN_DONE and
	 * returned. During execution of these operations the abort flag is tested to see if we need to
	 * stop the implementation of this command.
	 * saveFitsHeaders now creates lock files for the FITS images being created, modified, and these lock files
	 * are removed after the raw data has been written to disk.
	 * @see CommandImplementation#testAbort
	 * @see FITSImplementation#clearFitsHeaders
	 * @see FITSImplementation#setFitsHeaders
	 * @see FITSImplementation#getFitsHeadersFromISS
	 * @see FITSImplementation#getFitsHeadersFromBSS
	 * @see FITSImplementation#saveFitsHeaders
	 * @see FITSImplementation#unLockFiles
	 * @see ngat.o.ccd.CCDLibrary#expose
	 * @see EXPOSEImplementation#reduceExpose
	 * @see OStatus#getMaxReadoutTime
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		MULTRUN multRunCommand = (MULTRUN)command;
		MULTRUN_ACK multRunAck = null;
		MULTRUN_DP_ACK multRunDpAck = null;
		MULTRUN_DONE multRunDone = new MULTRUN_DONE(command.getId());
		String obsType = null;
		String filename = null;
		Vector filenameList = null;
		Vector reduceFilenameList = null;
		int index;
		boolean retval = false;

		if(testAbort(multRunCommand,multRunDone) == true)
			return multRunDone;
	// gsetup exposure status.
		status.setExposureCount(multRunCommand.getNumberExposures());
		status.setExposureNumber(0);
	// move the fold mirror to the correct location
		if(moveFold(multRunCommand,multRunDone) == false)
			return multRunDone;
		if(testAbort(multRunCommand,multRunDone) == true)
			return multRunDone;
	// setup filename object
		oFilename.nextMultRunNumber();
		try
		{
			if(multRunCommand.getStandard())
			{
				oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_STANDARD);
				obsType = FitsHeaderDefaults.OBSTYPE_VALUE_STANDARD;
			}
			else
			{
				oFilename.setExposureCode(FitsFilename.EXPOSURE_CODE_EXPOSURE);
				obsType = FitsHeaderDefaults.OBSTYPE_VALUE_EXPOSURE;
			}
		}
		catch(Exception e)
		{
			o.error(this.getClass().getName()+
				  ":processCommand:"+command+":"+e.toString());
			multRunDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1204);
			multRunDone.setErrorString(e.toString());
			multRunDone.setSuccessful(false);
			return multRunDone;
		}
	// do exposures
		index = 0;
		retval = true;
		reduceFilenameList = new Vector();
		while(retval&&(index < multRunCommand.getNumberExposures()))
		{
		// initialise list of FITS filenames for this frame
			filenameList = new Vector();
		// clear pause and resume times.
			status.clearPauseResumeTimes();
		// get a new filename.
			oFilename.nextRunNumber();
			filename = oFilename.getFilename();
// diddly window 1 only
		// get fits headers
			clearFitsHeaders();
			if(setFitsHeaders(multRunCommand,multRunDone,obsType,
				multRunCommand.getExposureTime(),multRunCommand.getNumberExposures()) == false)
			{
				return multRunDone;
			}
			if(getFitsHeadersFromISS(multRunCommand,multRunDone) == false)
			{
				return multRunDone;
			}
			if(testAbort(multRunCommand,multRunDone) == true)
			{
				return multRunDone;
			}
			if(getFitsHeadersFromBSS(multRunCommand,multRunDone) == false)
			{
				return multRunDone;
			}
			if(testAbort(multRunCommand,multRunDone) == true)
			{
				return multRunDone;
			}
		// save FITS headers
			if(saveFitsHeaders(multRunCommand,multRunDone,filenameList) == false)
			{
				unLockFiles(multRunCommand,multRunDone,filenameList);
				return multRunDone;
			}
		// do exposure.
// diddly window 1 only
			status.setExposureFilename(filename);
			try
			{
// diddly window 1 filename only
				ccd.expose(true,-1,multRunCommand.getExposureTime(),filenameList);
			}
			catch(CCDLibraryNativeException e)
			{
				o.error(this.getClass().getName()+
					":processCommand:"+command+":"+e.toString());
				multRunDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1201);
				multRunDone.setErrorString(e.toString());
				multRunDone.setSuccessful(false);
				unLockFiles(multRunCommand,multRunDone,filenameList);
				return multRunDone;
			}
			// remove FITS lock files for this frame created in saveFitsHeaders
			if(unLockFiles(multRunCommand,multRunDone,filenameList) == false)
			{
				return multRunDone;
			}
		// send acknowledge to say frame is completed.
			multRunAck = new MULTRUN_ACK(command.getId());
			multRunAck.setTimeToComplete(multRunCommand.getExposureTime()+status.getMaxReadoutTime()+
						     serverConnectionThread.getDefaultAcknowledgeTime());
// diddly window 1 filename only
			multRunAck.setFilename(filename);
			try
			{
				serverConnectionThread.sendAcknowledge(multRunAck);
			}
			catch(IOException e)
			{
				retval = false;
				o.error(this.getClass().getName()+
					":processCommand:sendAcknowledge:"+command+":"+e.toString());
				multRunDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1202);
				multRunDone.setErrorString(e.toString());
				multRunDone.setSuccessful(false);
				return multRunDone;
			}
			status.setExposureNumber(index+1);
		// add filename to list for data pipeline processing.
// diddly window 1 filename only
			reduceFilenameList.addAll(filenameList);
		// test whether an abort has occured.
			if(testAbort(multRunCommand,multRunDone) == true)
			{
				retval = false;
			}
			index++;
		}
	// if a failure occurs, return now
		if(!retval)
			return multRunDone;
		index = 0;
		retval = true;
	// call pipeline to process data and get results
		if(multRunCommand.getPipelineProcess())
		{
			while(retval&&(index < multRunCommand.getNumberExposures()))
			{
				filename = (String)reduceFilenameList.get(index);
			// do reduction.
				retval = reduceExpose(multRunCommand,multRunDone,filename);
			// send acknowledge to say frame has been reduced.
				multRunDpAck = new MULTRUN_DP_ACK(command.getId());
				multRunDpAck.setTimeToComplete(serverConnectionThread.getDefaultAcknowledgeTime());
			// copy Data Pipeline results from DONE to ACK
				multRunDpAck.setFilename(multRunDone.getFilename());
				multRunDpAck.setCounts(multRunDone.getCounts());
				multRunDpAck.setSeeing(multRunDone.getSeeing());
				multRunDpAck.setXpix(multRunDone.getXpix());
				multRunDpAck.setYpix(multRunDone.getYpix());
				multRunDpAck.setPhotometricity(multRunDone.getPhotometricity());
				multRunDpAck.setSkyBrightness(multRunDone.getSkyBrightness());
				multRunDpAck.setSaturation(multRunDone.getSaturation());
				try
				{
					serverConnectionThread.sendAcknowledge(multRunDpAck);
				}
				catch(IOException e)
				{
					retval = false;
					o.error(this.getClass().getName()+
						":processCommand:sendAcknowledge(DP):"+command+":"+e.toString());
					multRunDone.setErrorNum(OConstants.O_ERROR_CODE_BASE+1203);
					multRunDone.setErrorString(e.toString());
					multRunDone.setSuccessful(false);
					return multRunDone;
				}
				if(testAbort(multRunCommand,multRunDone) == true)
				{
					retval = false;
				}
				index++;
			}// end while on MULTRUN exposures
		}// end if Data Pipeline is to be called
		else
		{
		// no pipeline processing occured, set return value to something bland.
		// set filename to last filename exposed.
			multRunDone.setFilename(filename);
			multRunDone.setCounts(0.0f);
			multRunDone.setSeeing(0.0f);
			multRunDone.setXpix(0.0f);
			multRunDone.setYpix(0.0f);
			multRunDone.setPhotometricity(0.0f);
			multRunDone.setSkyBrightness(0.0f);
			multRunDone.setSaturation(false);
		}
	// if a failure occurs, return now
		if(!retval)
			return multRunDone;
	// setup return values.
	// setCounts,setFilename,setSeeing,setXpix,setYpix 
	// setPhotometricity, setSkyBrightness, setSaturation set by reduceExpose for last image reduced.
		multRunDone.setErrorNum(OConstants.O_ERROR_CODE_NO_ERROR);
		multRunDone.setErrorString("");
		multRunDone.setSuccessful(true);
	// return done object.
		return multRunDone;
	}
}
//
// $Log: not supported by cvs2svn $
//
