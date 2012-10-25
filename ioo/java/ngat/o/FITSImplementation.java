// FITSImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/FITSImplementation.java,v 1.11 2012-10-25 14:37:41 cjm Exp $
package ngat.o;

import java.lang.*;
import java.util.Date;
import java.util.List;
import java.util.Vector;

import ngat.message.base.*;
import ngat.message.ISS_INST.*;
import ngat.message.INST_BSS.*;
import ngat.message.RCS_BSS.*;
import ngat.fits.*;
import ngat.o.ccd.*;
import ngat.phase2.*;
import ngat.util.*;
import ngat.util.logging.*;

/**
 * This class provides the generic implementation of commands that write FITS files. It extends those that
 * use the hardware  libraries as this is needed to generate FITS files.
 * @see HardwareImplementation
 * @author Chris Mottram
 * @version $Revision: 1.11 $
 */
public class FITSImplementation extends HardwareImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: FITSImplementation.java,v 1.11 2012-10-25 14:37:41 cjm Exp $");
	/**
	 * Internal constant used when the order number offset defined in the property
	 * 'o.get_fits.order_number_offset' is not found or is not a valid number.
	 * @see #getFitsHeadersFromISS
	 */
	private final static int DEFAULT_ORDER_NUMBER_OFFSET = 255;
	/**
	 * A reference to the OStatus class instance that holds status information for O.
	 */
	protected OStatus status = null;
	/**
	 * A reference to the FitsFilename class instance used to generate unique FITS filenames.
	 */
	protected FitsFilename oFilename = null;
	/**
	 * A local reference to the FitsHeader object held in O. This is used for writing FITS headers to disk
	 * and setting the values of card images within the headers.
	 */
	protected FitsHeader oFitsHeader = null;
	/**
	 * A local reference to the FitsHeaderDefaults object held in O. 
	 * This is used to supply default values, 
	 * units and comments for FITS header card images.
	 */
	protected FitsHeaderDefaults oFitsHeaderDefaults = null;

	/**
	 * This method calls the super-classes method, and tries to fill in the reference to the
	 * FITS filename object, the FITS header object and the FITS default value object.
	 * @param command The command to be implemented.
	 * @see #status
	 * @see O#getStatus
	 * @see #oFilename
	 * @see O#getFitsFilename
	 * @see #oFitsHeader
	 * @see O#getFitsHeader
	 * @see #oFitsHeaderDefaults
	 * @see O#getFitsHeaderDefaults
	 */
	public void init(COMMAND command)
	{
		super.init(command);
		if(o != null)
		{
			status = o.getStatus();
			oFilename = o.getFitsFilename();
			oFitsHeader = o.getFitsHeader();
			oFitsHeaderDefaults = o.getFitsHeaderDefaults();
		}
	}

	/**
	 * This method is used to calculate how long an implementation of a command is going to take, so that the
	 * client has an idea of how long to wait before it can assume the server has died.
	 * @param command The command to be implemented.
	 * @return The time taken to implement this command, or the time taken before the next acknowledgement
	 * is to be sent.
	 */
	public ACK calculateAcknowledgeTime(COMMAND command)
	{
		return super.calculateAcknowledgeTime(command);
	}

	/**
	 * This routine performs the generic command implementation.
	 * @param command The command to be implemented.
	 * @return The results of the implementation of this command.
	 */
	public COMMAND_DONE processCommand(COMMAND command)
	{
		return super.processCommand(command);
	}

	/**
	 * This routine tries to move the mirror fold to a certain location, by issuing a MOVE_FOLD command
	 * to the ISS. The position to move the fold to is specified by the O property file.
	 * If an error occurs the done objects field's are set accordingly.
	 * @param command The command being implemented that made this call to the ISS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see OStatus#getPropertyInteger
	 * @see O#sendISSCommand
	 */
	public boolean moveFold(COMMAND command,COMMAND_DONE done)
	{
		INST_TO_ISS_DONE instToISSDone = null;
		MOVE_FOLD moveFold = null;
		int mirrorFoldPosition = 0;

		moveFold = new MOVE_FOLD(command.getId());
		try
		{
			mirrorFoldPosition = status.getPropertyInteger("o.mirror.fold.position");
		}
		catch(NumberFormatException e)
		{
			mirrorFoldPosition = 0;
			o.error(this.getClass().getName()+":moveFold:"+
				command.getClass().getName(),e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+300);
			done.setErrorString("moveFold:"+e);
			done.setSuccessful(false);
			return false;
		}
		moveFold.setMirror_position(mirrorFoldPosition);
		instToISSDone = o.sendISSCommand(moveFold,serverConnectionThread);
		if(instToISSDone.getSuccessful() == false)
		{
			o.error(this.getClass().getName()+":moveFold:"+
				command.getClass().getName()+":"+instToISSDone.getErrorString());
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+301);
			done.setErrorString(instToISSDone.getErrorString());
			done.setSuccessful(false);		
			return false;
		}
		return true;
	}

	/**
	 * This routine clears the current set of FITS headers. The FITS headers are held in the main O
	 * object. This is retrieved and the relevant method called.
	 * @see #oFitsHeader
	 * @see ngat.fits.FitsHeader#clearKeywordValueList
	 */
	public void clearFitsHeaders()
	{
		oFitsHeader.clearKeywordValueList();
	}

	/**
	 * This routine sets up the Fits Header objects with some keyword value pairs.
	 * It calls the more complicated method below, assuming exposureCount is 1.
	 * @param command The command being implemented that made this call to the ISS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @param obsTypeString The type of image taken by the camera. This string should be
	 * 	one of the OBSTYPE_VALUE_* defaults in ngat.fits.FitsHeaderDefaults.
	 * @param exposureTime The exposure time,in milliseconds, to put in the EXPTIME keyword. It
	 * 	is converted into decimal seconds (a double).
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see #setFitsHeaders(COMMAND,COMMAND_DONE,String,int,int)
	 */
	public boolean setFitsHeaders(COMMAND command,COMMAND_DONE done,String obsTypeString,int exposureTime)
	{
		return setFitsHeaders(command,done,obsTypeString,exposureTime,1);
	}

	/**
	 * This routine sets up the Fits Header objects with some keyword value pairs.
	 * <p>The following mandatory keywords are filled in: SIMPLE,BITPIX,NAXIS,NAXIS1,NAXIS2. Note NAXIS1 and
	 * NAXIS2 are retrieved from libccd, assuming the library has previously been setup with a 
	 * configuration.</p>
	 * <p> A complete list of keywords is constructed from the O FITS defaults file. Some of the values of
	 * these keywords are overwritten by real data obtained from the camera controller, 
	 * or internal O status.
	 * These are:
	 * OBSTYPE, RUNNUM, EXPNUM, EXPTOTAL, DATE, DATE-OBS, UTSTART, MJD, EXPTIME, 
	 * FILTER1, FILTERI1, FILTER2, FILTERI2, CONFIGID, CONFNAME, 
	 * PRESCAN, POSTSCAN, GAIN, READNOIS, EPERDN, CCDXIMSI, CCDYIMSI, CCDSCALE, CCDRDOUT,
	 * CCDXBIN, CCDYBIN, CCDSTEMP, CCDATEMP, CCDWMODE, CALBEFOR, CALAFTER, INSTDFOC, FILTDFOC, MYDFOCUS,
	 * SATWELL, SATADC, SATURATN.
	 * Windowing keywords CCDWXOFF, CCDWYOFF, CCDWXSIZ, CCDWYSIZ are not implemented at the moment.
	 * Note the DATE, DATE-OBS, UTSTART and MJD keywords are given the value of the current
	 * system time, this value is updated to the exposure start time when the image has been exposed. </p>
	 * @param command The command being implemented that made this call to the ISS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @param obsTypeString The type of image taken by the camera. This string should be
	 * 	one of the OBSTYPE_VALUE_* defaults in ngat.fits.FitsHeaderDefaults.
	 * @param exposureTime The exposure time,in milliseconds, to put in the EXPTIME keyword. It
	 * 	is converted into decimal seconds (a double).
	 * @param exposureCount The number of exposures to put in the EXPTOTAL keyword.
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see #status
	 * @see #ccd
	 * @see #oFitsHeader
	 * @see #oFitsHeaderDefaults
	 * @see #getCCDRDOUTValue
	 * @see HardwareImplementation#ccd
	 * @see OStatus#getPropertyBoolean
	 * @see OStatus#getPropertyDouble
	 * @see OStatus#getFilterTypeName
	 * @see OStatus#getFilterIdName
	 * @see OStatus#getFilterIdOpticalThickness
	 * @see OStatus#getBSSFocusOffset
	 * @see ngat.o.ccd.CCDLibrary#filterWheelGetPosition
	 * @see ngat.o.ccd.CCDLibrary#getBinnedNCols
	 * @see ngat.o.ccd.CCDLibrary#getBinnedNRows
	 * @see ngat.o.ccd.CCDLibrary#getTemperature
	 * @see ngat.o.ccd.CCDLibrary#getTemperatureHeaterADU
	 * @see ngat.o.ccd.CCDLibrary#getTemperatureHeaterPower
	 * @see ngat.o.ccd.CCDLibrary#getSetupWindowFlags
	 * @see ngat.fits.FitsHeaderDefaults#getCardImageList
	 */
	public boolean setFitsHeaders(COMMAND command,COMMAND_DONE done,String obsTypeString,
				      int exposureTime,int exposureCount)
	{
		double actualTemperature = 0.0;
		FitsHeaderCardImage cardImage = null;
		Date date = null;
		String filterWheelString = null;
		String filterWheelIdString = null;
		Vector defaultFitsHeaderList = null;
		int iValue,filterWheelPosition,xBin,yBin,windowFlags,heaterADU,preScan, postScan;
		double doubleValue = 0.0;
		double instDFoc,filtDFoc,myDFoc,bssFoc;
		boolean filterWheelEnable;

		// filter wheel and dfocus data
		try
		{
			// instrument defocus
			instDFoc = status.getPropertyDouble("o.focus.offset");
			// filter wheel enable
			filterWheelEnable = status.getPropertyBoolean("o.config.filter_wheel.enable");
			if(filterWheelEnable)
			{
			// type name
				filterWheelPosition = ccd.filterWheelGetPosition();
				filterWheelString = status.getFilterTypeName(filterWheelPosition);
			// filter id
				filterWheelIdString = status.getFilterIdName(filterWheelString);
			// filter defocus
				filtDFoc = status.getFilterIdOpticalThickness(filterWheelIdString);
			}
			else
			{
				filterWheelString = new String("UNKNOWN");
				filterWheelIdString = new String("UNKNOWN");
				filtDFoc = 0.0;
			}
			// defocus settings
			// get cached BSS offset. This is the offset returned from the last BSS GET_FOCUS_OFFSET
			// command issued by o. The current BSS offset may be different, but this cached one
			// is probably better as we want to use it to calculate myDFoc, which is the total
			// DFOCUS this instrument (configuration) would have liked, rather than the current
			// telescope DFOCUS which might be different (if this instrument does not have FOCUS_CONTROL).
			bssFoc = status.getBSSFocusOffset();
			myDFoc = instDFoc + filtDFoc + bssFoc;
		}
		// ngat.o.ccd.CCDNativeException thrown by ccd.filterWheelGetPosition
		// IllegalArgumentException thrown by OStatus.getFilterWheelName
		catch(Exception e)
		{
			String s = new String("Command "+command.getClass().getName()+
				":Setting Fits Headers failed:");
			o.error(s,e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+309);
			done.setErrorString(s+e);
			done.setSuccessful(false);
			return false;
		}
		try
		{
		// load all the FITS header defaults and put them into the oFitsHeader object
			defaultFitsHeaderList = oFitsHeaderDefaults.getCardImageList();
			oFitsHeader.addKeywordValueList(defaultFitsHeaderList,0);
		// get current binning for later
			xBin = ccd.getXBin();
			yBin = ccd.getYBin();
		// if the binning values are < 1, a problem has occured
			if((xBin < 1)||(yBin < 1))
			{
				String s = new String("Command "+command.getClass().getName()+
					":Setting Fits Headers failed:Illegal binning values:X Bin:"+xBin+
					":Y Bin:"+yBin);
				o.error(s);
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+308);
				done.setErrorString(s);
				done.setSuccessful(false);
				return false;
			}
		// NAXIS1
			cardImage = oFitsHeader.get("NAXIS1");
			cardImage.setValue(new Integer(ccd.getBinnedNCols()));
		// NAXIS2
			cardImage = oFitsHeader.get("NAXIS2");
			cardImage.setValue(new Integer(ccd.getBinnedNRows()));
		// OBSTYPE
			cardImage = oFitsHeader.get("OBSTYPE");
			cardImage.setValue(obsTypeString);
		// The current MULTRUN number and runNumber are used for these keywords at the moment.
		// They are updated in saveFitsHeaders, when the retrieved values are more likely 
		// to be correct.
		// RUNNUM
			cardImage = oFitsHeader.get("RUNNUM");
			cardImage.setValue(new Integer(oFilename.getMultRunNumber()));
		// EXPNUM
			cardImage = oFitsHeader.get("EXPNUM");
			cardImage.setValue(new Integer(oFilename.getRunNumber()));
		// EXPTOTAL
			cardImage = oFitsHeader.get("EXPTOTAL");
			cardImage.setValue(new Integer(exposureCount));
		// The DATE,DATE-OBS and UTSTART keywords are saved using the current date/time.
		// This is updated when the data is saved if CFITSIO is used.
			date = new Date();
		// DATE
			cardImage = oFitsHeader.get("DATE");
			cardImage.setValue(date);
		// DATE-OBS
			cardImage = oFitsHeader.get("DATE-OBS");
			cardImage.setValue(date);
		// UTSTART
			cardImage = oFitsHeader.get("UTSTART");
			cardImage.setValue(date);
		// MJD
			cardImage = oFitsHeader.get("MJD");
			cardImage.setValue(date);
		// EXPTIME
			cardImage = oFitsHeader.get("EXPTIME");
			cardImage.setValue(new Double(((double)exposureTime)/1000.0));
		// FILTER1
			cardImage = oFitsHeader.get("FILTER1");
			cardImage.setValue(filterWheelString);
		// FILTERI1
			cardImage = oFitsHeader.get("FILTERI1");
			cardImage.setValue(filterWheelIdString);
		// CONFIGID
			cardImage = oFitsHeader.get("CONFIGID");
			cardImage.setValue(new Integer(status.getConfigId()));
		// CONFNAME
			cardImage = oFitsHeader.get("CONFNAME");
			cardImage.setValue(status.getConfigName());
		// PRESCAN
			cardImage = oFitsHeader.get("PRESCAN");
			preScan = oFitsHeaderDefaults.getValueInteger("PRESCAN."+status.getNumberColumns(xBin)+"."+
								      getCCDRDOUTValue()+"."+xBin);
			cardImage.setValue(new Integer(preScan));
		// POSTSCAN
			cardImage = oFitsHeader.get("POSTSCAN");
			postScan = oFitsHeaderDefaults.getValueInteger("POSTSCAN."+status.getNumberColumns(xBin)+"."+
								       getCCDRDOUTValue()+"."+xBin);
			cardImage.setValue(new Integer(postScan));
		// GAIN
			cardImage = oFitsHeader.get("GAIN");
			doubleValue = oFitsHeaderDefaults.getValueDouble("GAIN."+getCCDRDOUTValue());
			cardImage.setValue(new Double(doubleValue));
		// READNOIS
			cardImage = oFitsHeader.get("READNOIS");
			doubleValue = oFitsHeaderDefaults.getValueDouble("READNOIS."+getCCDRDOUTValue());
			cardImage.setValue(new Double(doubleValue));
		// EPERDN
			cardImage = oFitsHeader.get("EPERDN");
			doubleValue = oFitsHeaderDefaults.getValueDouble("EPERDN."+getCCDRDOUTValue());
			cardImage.setValue(new Double(doubleValue));
		// CCDXIMSI
			cardImage = oFitsHeader.get("CCDXIMSI");
			cardImage.setValue(new Integer(oFitsHeaderDefaults.getValueInteger("CCDXIMSI")/xBin));
		// CCDYIMSI
			cardImage = oFitsHeader.get("CCDYIMSI");
			cardImage.setValue(new Integer(oFitsHeaderDefaults.getValueInteger("CCDYIMSI")/yBin));
		// CCDSCALE
			cardImage = oFitsHeader.get("CCDSCALE");
			// note this next line assumes xbin == ybin e.g. CCDSCALE is constant in both axes
			cardImage.setValue(new Double(oFitsHeaderDefaults.getValueDouble("CCDSCALE")*xBin));
		// CCDRDOUT
			cardImage = oFitsHeader.get("CCDRDOUT");
			cardImage.setValue(getCCDRDOUTValue());
		// CCDXBIN
			cardImage = oFitsHeader.get("CCDXBIN");
			cardImage.setValue(new Integer(xBin));
		// CCDYBIN
			cardImage = oFitsHeader.get("CCDYBIN");
			cardImage.setValue(new Integer(yBin));
		// CCDSTEMP
			doubleValue = status.getPropertyDouble("o.ccd.config.temperature.target")+CENTIGRADE_TO_KELVIN;
			cardImage = oFitsHeader.get("CCDSTEMP");
			cardImage.setValue(new Integer((int)doubleValue));
		// CCDATEMP
			actualTemperature = ccd.getTemperature();
			cardImage = oFitsHeader.get("CCDATEMP");
			cardImage.setValue(new Integer((int)(actualTemperature+CENTIGRADE_TO_KELVIN)));
		// CCDHEATA
			heaterADU = ccd.getTemperatureHeaterADU();
			cardImage = oFitsHeader.get("CCDHEATA");
			cardImage.setValue(new Integer((int)(heaterADU)));
		// CCDHEATP
			doubleValue = ccd.getTemperatureHeaterPower(heaterADU);
			cardImage = oFitsHeader.get("CCDHEATP");
			cardImage.setValue(new Double(doubleValue));
		// windowing keywords
		// CCDWMODE
			windowFlags = ccd.getSetupWindowFlags();
			cardImage = oFitsHeader.get("CCDWMODE");
			cardImage.setValue(new Boolean((boolean)(windowFlags>0)));
		// CALBEFOR
			cardImage = oFitsHeader.get("CALBEFOR");
			// diddly cardImage.setValue(new Boolean(status.getCachedConfigCalibrateBefore()));
		// CALAFTER
			cardImage = oFitsHeader.get("CALAFTER");
			// diddly cardImage.setValue(new Boolean(status.getCachedConfigCalibrateAfter()));
		// ROTCENTX
		// Value specified in config file is unbinned without bias offsets added
			cardImage = oFitsHeader.get("ROTCENTX");
			cardImage.setValue(new Integer((oFitsHeaderDefaults.getValueInteger("ROTCENTX")/xBin)+
					   preScan));
		// ROTCENTY
		// Value specified in config file is unbinned 
			cardImage = oFitsHeader.get("ROTCENTY");
			cardImage.setValue(new Integer(oFitsHeaderDefaults.getValueInteger("ROTCENTY")/yBin));
		// POICENTX
		// Value specified in config file is unbinned without bias offsets added
			cardImage = oFitsHeader.get("POICENTX");
			cardImage.setValue(new Integer((oFitsHeaderDefaults.getValueInteger("POICENTX")/xBin)+
					   preScan));
		// POICENTY
		// Value specified in config file is unbinned 
			cardImage = oFitsHeader.get("POICENTY");
			cardImage.setValue(new Integer(oFitsHeaderDefaults.getValueInteger("POICENTY")/yBin));
		// INSTDFOC
			cardImage = oFitsHeader.get("INSTDFOC");
			cardImage.setValue(new Double(instDFoc));
		// FILTDFOC
			cardImage = oFitsHeader.get("FILTDFOC");
			cardImage.setValue(new Double(filtDFoc));
		// MYDFOCUS
			cardImage = oFitsHeader.get("MYDFOCUS");
			cardImage.setValue(new Double(myDFoc));
		// SATWELL
			cardImage = oFitsHeader.get("SATWELL");
			iValue = oFitsHeaderDefaults.getValueInteger("SATWELL."+xBin);
			cardImage.setValue(new Integer(iValue));
		// SATADC
			cardImage = oFitsHeader.get("SATADC");
			iValue = oFitsHeaderDefaults.getValueInteger("SATADC."+xBin);
			cardImage.setValue(new Integer(iValue));
		// SATURATN
			cardImage = oFitsHeader.get("SATURATN");
			iValue = oFitsHeaderDefaults.getValueInteger("SATURATN."+xBin);
			cardImage.setValue(new Integer(iValue));
		}// end try
		// ngat.fits.FitsHeaderException thrown by oFitsHeaderDefaults.getValue
		// ngat.util.FileUtilitiesNativeException thrown by OStatus.getConfigId
		// IllegalArgumentException thrown by OStatus.getFilterWheelName
		// NumberFormatException thrown by OStatus.getFilterWheelName/OStatus.getConfigId
		// Exception thrown by OStatus.getConfigId
		catch(Exception e)
		{
			String s = new String("Command "+command.getClass().getName()+
				":Setting Fits Headers failed:");
			o.error(s,e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+304);
			done.setErrorString(s+e);
			done.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * This routine tries to get a set of FITS headers for an exposure, by issuing a GET_FITS command
	 * to the ISS. The results from this command are put into the O's FITS header object.
	 * If an error occurs the done objects field's can be set to record the error.
	 * The order numbers returned from the ISS are incremented by the order number offset
	 * defined in the O 'o.get_fits.iss.order_number_offset' property.
	 * @param command The command being implemented that made this call to the ISS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see O#sendISSCommand
	 * @see O#getStatus
	 * @see OStatus#getPropertyInteger
	 * @see #oFitsHeader
	 * @see #DEFAULT_ORDER_NUMBER_OFFSET
	 */
	public boolean getFitsHeadersFromISS(COMMAND command,COMMAND_DONE done)
	{
		INST_TO_ISS_DONE instToISSDone = null;
		ngat.message.ISS_INST.GET_FITS getFits = null;
		ngat.message.ISS_INST.GET_FITS_DONE getFitsDone = null;
		int orderNumberOffset;

		getFits = new ngat.message.ISS_INST.GET_FITS(command.getId());
		instToISSDone = o.sendISSCommand(getFits,serverConnectionThread);
		if(instToISSDone.getSuccessful() == false)
		{
			o.error(this.getClass().getName()+":getFitsHeadersFromISS:"+
				command.getClass().getName()+":"+instToISSDone.getErrorString());
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+305);
			done.setErrorString(instToISSDone.getErrorString());
			done.setSuccessful(false);
			return false;
		}
	// Get the returned FITS header information into the FitsHeader object.
		getFitsDone = (ngat.message.ISS_INST.GET_FITS_DONE)instToISSDone;
	// get the order number offset
		try
		{
			orderNumberOffset = status.getPropertyInteger("o.get_fits.iss.order_number_offset");
		}
		catch(NumberFormatException e)
		{
			orderNumberOffset = DEFAULT_ORDER_NUMBER_OFFSET;
			o.error(this.getClass().getName()+":getFitsHeadersFromISS:Getting order number offset failed.",
				e);
		}
		oFitsHeader.addKeywordValueList(getFitsDone.getFitsHeader(),orderNumberOffset);
		return true;
	}

	/**
	 * This routine tries to get a set of FITS headers for an exposure, by issuing a GET_FITS command
	 * to the BSS. The results from this command are put into the O's FITS header object.
	 * If an error occurs the done objects field's can be set to record the error.
	 * The order numbers returned from the BSS are incremented by the order number offset
	 * defined in the O 'o.get_fits.bss.order_number_offset' property.
	 * @param command The command being implemented that made this call to the BSS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see O#sendBSSCommand(INST_TO_BSS,OTCPServerConnectionThread)
	 * @see O#getStatus
	 * @see OStatus#getPropertyInteger
	 * @see #oFitsHeader
	 * @see #DEFAULT_ORDER_NUMBER_OFFSET
	 */
	public boolean getFitsHeadersFromBSS(COMMAND command,COMMAND_DONE done)
	{
		INST_TO_BSS_DONE instToBSSDone = null;
		ngat.message.INST_BSS.GET_FITS getFits = null;
		ngat.message.INST_BSS.GET_FITS_DONE getFitsDone = null;
		String instrumentName = null;
		int orderNumberOffset;
		boolean bssUse;

		bssUse = status.getPropertyBoolean("o.net.bss.use");
		if(bssUse)
		{
			instrumentName = status.getProperty("o.bss.instrument_name");
			getFits = new ngat.message.INST_BSS.GET_FITS(command.getId());
			getFits.setInstrumentName(instrumentName);
			instToBSSDone = o.sendBSSCommand(getFits,serverConnectionThread);
			if(instToBSSDone.getSuccessful() == false)
			{
				o.error(this.getClass().getName()+":getFitsHeadersFromBSS:"+
					command.getClass().getName()+":"+instToBSSDone.getErrorString());
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+317);
				done.setErrorString(instToBSSDone.getErrorString());
				done.setSuccessful(false);
				return false;
			}
			if((instToBSSDone instanceof ngat.message.INST_BSS.GET_FITS_DONE) == false)
			{
				o.error(this.getClass().getName()+":getFitsHeadersFromBSS:"+
					command.getClass().getName()+":DONE was not instance of GET_FITS_DONE:"+
					instToBSSDone.getClass().getName());
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+319);
				done.setErrorString("getFitsHeadersFromBSS:"+command.getClass().getName()+
						    ":DONE was not instance of GET_FITS_DONE:"+
						    instToBSSDone.getClass().getName());
				done.setSuccessful(false);
				return false;
			}
			// Get the returned FITS header information into the FitsHeader object.
			getFitsDone = (ngat.message.INST_BSS.GET_FITS_DONE)instToBSSDone;
			// get the order number offset
			try
			{
				orderNumberOffset = status.getPropertyInteger("o.get_fits.bss.order_number_offset");
			}
			catch(NumberFormatException e)
			{
				orderNumberOffset = DEFAULT_ORDER_NUMBER_OFFSET;
				o.error(this.getClass().getName()+
					":getFitsHeadersFromBSS:Getting order number offset failed.",e);
			}
			oFitsHeader.addKeywordValueList(getFitsDone.getFitsHeader(),orderNumberOffset);
		}
		else
		{
			o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
			      ":getFitsHeadersFromBSS:BSS not in use, no FITS headers added.");
		}
		return true;
	}

	/**
	 * This routine uses the Fits Header object, stored in the O object, to save the headers to disc.
	 * This method also updates the RUNNUM and EXPNUM keywords with the current multRun and runNumber values
	 * in the oFilename object, as they must be correct when the file is saved.
	 * A lock file is created before the FITS header is written, this allows synchronisation with the
	 * data transfer software.
	 * @param command The command being implemented that made this call to the ISS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @param filename The filename to save the headers to.
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see #oFitsHeader
	 * @see #oFilename
	 * @see ngat.fits.FitsFilename#getMultRunNumber
	 * @see ngat.fits.FitsFilename#getRunNumber
	 * @see ngat.util.LockFile2
	 */
	public boolean saveFitsHeaders(COMMAND command,COMMAND_DONE done,String filename)
	{
		LockFile2 lockFile = null;

		try
		{
			oFitsHeader.add("RUNNUM",new Integer(oFilename.getMultRunNumber()),
				oFitsHeaderDefaults.getComment("RUNNUM"),
				oFitsHeaderDefaults.getUnits("RUNNUM"),
				oFitsHeaderDefaults.getOrderNumber("RUNNUM"));
			oFitsHeader.add("EXPNUM",new Integer(oFilename.getRunNumber()),
				oFitsHeaderDefaults.getComment("EXPNUM"),
				oFitsHeaderDefaults.getUnits("EXPNUM"),
				oFitsHeaderDefaults.getOrderNumber("EXPNUM"));
		}
		// FitsHeaderException thrown by oFitsHeaderDefaults.getValue
		// IllegalAccessException thrown by oFitsHeaderDefaults.getValue
		// InvocationTargetException thrown by oFitsHeaderDefaults.getValue
		// NoSuchMethodException thrown by oFitsHeaderDefaults.getValue
		// InstantiationException thrown by oFitsHeaderDefaults.getValue
		// ClassNotFoundException thrown by oFitsHeaderDefaults.getValue
		catch(Exception e)
		{
			String s = new String("Command "+command.getClass().getName()+
				":Setting Fits Headers in saveFitsHeaders failed:"+e);
			o.error(s,e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+306);
			done.setErrorString(s);
			done.setSuccessful(false);
			return false;
		}
		// create lock file
		try
		{
			lockFile = new LockFile2(filename);
			lockFile.lock();
		}
		catch(Exception e)
		{
			String s = new String("Command "+command.getClass().getName()+
					":saveFitsHeaders:Creating lock file failed for file:"+filename+":"+e);
			o.error(s,e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+314);
			done.setErrorString(s);
			done.setSuccessful(false);
			return false;			
		}
		try
		{
			oFitsHeader.writeFitsHeader(filename);
		}
		catch(FitsHeaderException e)
		{
			String s = new String("Command "+command.getClass().getName()+
					":Saving Fits Headers failed for file:"+filename+":"+e);
			o.error(s,e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+307);
			done.setErrorString(s);
			done.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * This routine uses the Fits Header object, stored in the O object, to save the headers to disc.
	 * A lock file is created before the FITS header is written, this allows synchronisation with the
	 * data transfer software.
	 * This method also updates the RUNNUM and EXPNUM keywords with the current multRun and runNumber values
	 * in the oFilename object, as they must be correct when the file is saved. 
	 * A list of windows are defined from the setup's window flags, and a set of headers
	 * are saved to a window specific filename for each window defined.
	 * It changess the CCDWXOFF, CCDWYOFF, CCDWXSIZ and CCDWYSIZ keywords for each window defined. 
	 * @param command The command being implemented that made this call to the ISS. This is used
	 * 	for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @param filenameList An instance of a list. The filename's saved containing FITS headers are added
	 *        to the list.
	 * @return The routine returns a boolean to indicate whether the operation was completed
	 *  	successfully.
	 * @see #oFitsHeader
	 * @see #oFilename
	 * @see ngat.fits.FitsFilename#getMultRunNumber
	 * @see ngat.fits.FitsFilename#getRunNumber
	 * @see ngat.util.LockFile2
	 */
	public boolean saveFitsHeaders(COMMAND command,COMMAND_DONE done,List filenameList)
	{
		FitsHeaderCardImage cardImage = null;
		CCDLibrarySetupWindow window = null;
		LockFile2 lockFile = null;
		List windowIndexList = null;
		String filename = null;
		int windowIndex,windowFlags,ncols,nrows,xbin,ybin;

		windowFlags = ccd.getSetupWindowFlags();
		windowIndexList = new Vector();
		if(windowFlags > 0)
		{
			// if the relevant bit is set, add an Integer with the appopriate
			// window index to the windowIndexList. The window index is one less than
			// the window number.
			if((windowFlags&CCDLibrary.SETUP_WINDOW_ONE) > 0)
			{
				windowIndexList.add(new Integer(0));
			}
			if((windowFlags&CCDLibrary.SETUP_WINDOW_TWO) > 0)
			{
				windowIndexList.add(new Integer(1));
			}
			if((windowFlags&CCDLibrary.SETUP_WINDOW_THREE) > 0)
			{
				windowIndexList.add(new Integer(2));
			}
			if((windowFlags&CCDLibrary.SETUP_WINDOW_FOUR) > 0)
			{
				windowIndexList.add(new Integer(3));
			}
		}// end if windowFlags > 0
		else
		{
			windowIndexList.add(new Integer(-1));
		}
		for(int i = 0; i < windowIndexList.size(); i++)
		{
			try
			{
				windowIndex = ((Integer)windowIndexList.get(i)).intValue();
				if(windowIndex > -1)
				{
					window = ccd.getSetupWindow(windowIndex);
					// window number is 1 more than index
					oFilename.setWindowNumber(windowIndex+1);
					ncols = ccd.getWindowWidth(windowIndex);
					nrows = ccd.getWindowHeight(windowIndex);
					// only change PRESCAN and POSTSCAN if windowed
				        // PRESCAN
					cardImage = oFitsHeader.get("PRESCAN");
					cardImage.setValue(new Integer(0));
				        // POSTSCAN
					cardImage = oFitsHeader.get("POSTSCAN");
				        //diddly see ccd_setup.c : SETUP_WINDOW_BIAS_WIDTH
					cardImage.setValue(new Integer(53));
				}
				else
				{
					// get current binning for later
					xbin = ccd.getXBin();
					ybin = ccd.getYBin();
					o.log(Logging.VERBOSITY_VERY_VERBOSE,this.getClass().getName()+
					      ":saveFitsHeaders:Default window X size = "+
					      oFitsHeaderDefaults.getValueInteger("CCDWXSIZ")+" / "+xbin+" = "+
					      (oFitsHeaderDefaults.getValueInteger("CCDWXSIZ")/xbin)+".");
					o.log(Logging.VERBOSITY_VERY_VERBOSE,this.getClass().getName()+
					      ":saveFitsHeaders:Default window Y size = "+
					      oFitsHeaderDefaults.getValueInteger("CCDWYSIZ")+" / "+ybin+" = "+
					      (oFitsHeaderDefaults.getValueInteger("CCDWYSIZ")/ybin)+".");
					window = new CCDLibrarySetupWindow(0,0,
							  oFitsHeaderDefaults.getValueInteger("CCDWXSIZ")/xbin,
							  oFitsHeaderDefaults.getValueInteger("CCDWYSIZ")/ybin);
					o.log(Logging.VERBOSITY_VERY_VERBOSE,this.getClass().getName()+
						":saveFitsHeaders:Using default window : "+window+".");
					oFilename.setWindowNumber(1);
					ncols = ccd.getBinnedNCols();
					nrows = ccd.getBinnedNRows();
				}
				filename = oFilename.getFilename();
				// NAXIS1
				cardImage = oFitsHeader.get("NAXIS1");
				cardImage.setValue(new Integer(ncols));
				// NAXIS2
				cardImage = oFitsHeader.get("NAXIS2");
				cardImage.setValue(new Integer(nrows));
				// RUNNUM/EXPNUM
				oFitsHeader.add("RUNNUM",new Integer(oFilename.getMultRunNumber()),
						  oFitsHeaderDefaults.getComment("RUNNUM"),
						  oFitsHeaderDefaults.getUnits("RUNNUM"),
						  oFitsHeaderDefaults.getOrderNumber("RUNNUM"));
				oFitsHeader.add("EXPNUM",new Integer(oFilename.getRunNumber()),
						  oFitsHeaderDefaults.getComment("EXPNUM"),
						  oFitsHeaderDefaults.getUnits("EXPNUM"),
						  oFitsHeaderDefaults.getOrderNumber("EXPNUM"));
				// CCDWXOFF
				cardImage = oFitsHeader.get("CCDWXOFF");
				cardImage.setValue(new Integer(window.getXStart()));
				// CCDWYOFF
				cardImage = oFitsHeader.get("CCDWYOFF");
				cardImage.setValue(new Integer(window.getYStart()));
				// CCDWXSIZ
				cardImage = oFitsHeader.get("CCDWXSIZ");
				cardImage.setValue(new Integer(window.getXEnd()-window.getXStart()));
				// CCDWYSIZ
				cardImage = oFitsHeader.get("CCDWYSIZ");
				cardImage.setValue(new Integer(window.getYEnd()-window.getYStart()));
			}//end try
			// CCDLibraryNativeException thrown by CCDSetupGetWindow
			// FitsHeaderException thrown by oFitsHeaderDefaults.getValue
			// IllegalAccessException thrown by oFitsHeaderDefaults.getValue
			// InvocationTargetException thrown by oFitsHeaderDefaults.getValue
			// NoSuchMethodException thrown by oFitsHeaderDefaults.getValue
			// InstantiationException thrown by oFitsHeaderDefaults.getValue
			// ClassNotFoundException thrown by oFitsHeaderDefaults.getValue
			catch(Exception e)
			{
				String s = new String("Command "+command.getClass().getName()+
						      ":Setting Fits Headers in saveFitsHeaders failed:"+e);
				o.error(s,e);
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+311);
				done.setErrorString(s);
				done.setSuccessful(false);
				return false;
			}
			// create lock file
			try
			{
				lockFile = new LockFile2(filename);
				lockFile.lock();
			}
			catch(Exception e)
			{
				String s = new String("Command "+command.getClass().getName()+
						":saveFitsHeaders:Create lock file failed for file:"+filename+":"+e);
				o.error(s,e);
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+315);
				done.setErrorString(s);
				done.setSuccessful(false);
				return false;
			}
			// actually write FITS header
			try
			{
				oFitsHeader.writeFitsHeader(filename);
				filenameList.add(filename);
			}
			catch(FitsHeaderException e)
			{
				String s = new String("Command "+command.getClass().getName()+
						      ":Saving Fits Headers failed for file:"+filename+":"+e);
				o.error(s,e);
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+312);
				done.setErrorString(s);
				done.setSuccessful(false);
				return false;
			}
		}// end for
		return true;
	}

	/**
	 * This method tries to unlock the FITS filename.
	 * It first checks the lock file exists and only attempts to unlock locked files. This is because
	 * this method can be called after a partial failure, where the specified FITS file may or may not have been
	 * locked.
	 * @param command The command being implemented. This is used for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @param filename The FITS filename which should have an associated lock file.
	 * @return true if the method succeeds, false if a failure occurs.
	 * @see ngat.util.LockFile2
	 */
	public boolean unLockFile(COMMAND command,COMMAND_DONE done,String filename)
	{
		LockFile2 lockFile = null;

		try
		{
			lockFile = new LockFile2(filename);
			if(lockFile.isLocked())
				lockFile.unLock();
		}
		catch(Exception e)
		{
			String s = new String("Command "+command.getClass().getName()+
					      ":unLockFile:Unlocking lock file failed for file:"+filename+":"+e);
			o.error(s,e);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+318);
			done.setErrorString(s);
			done.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * This method tries to unlock files associated with the FITS filename in filenameList.
	 * It first checks the lock file exists and only attempts to unlocked locked files. This is because
	 * this method can be called after a partial failure, where only some of the specified FITS files had been
	 * locked.
	 * @param command The command being implemented. This is used for error logging.
	 * @param done A COMMAND_DONE subclass specific to the command being implemented. If an
	 * 	error occurs the relevant fields are filled in with the error.
	 * @param filenameList A list containing FITS filenames which should have associated lock files.
	 * @return true if the method succeeds, false if a failure occurs
	 * @see ngat.util.LockFile2
	 */
	public boolean unLockFiles(COMMAND command,COMMAND_DONE done,List filenameList)
	{
		LockFile2 lockFile = null;
		String filename = null;

		for(int i = 0; i < filenameList.size(); i++)
		{
			try
			{
				filename = (String)(filenameList.get(i));
				lockFile = new LockFile2(filename);
				if(lockFile.isLocked())
					lockFile.unLock();
			}
			catch(Exception e)
			{
				String s = new String("Command "+command.getClass().getName()+
						":unLockFiles:Unlocking lock file failed for file:"+filename+":"+e);
				o.error(s,e);
				done.setErrorNum(OConstants.O_ERROR_CODE_BASE+316);
				done.setErrorString(s);
				done.setSuccessful(false);
				return false;
			}
		}
		return true;
	}

	/**
	 * This method is called by command implementors that assume the CCD camera was configured to
	 * operate in non-windowed mode. The method checks CCDLibrary's setup window flags are zero.
	 * If they are non-zero, the done object is filled in with a suitable error message.
	 * @param done An instance of command done, the error string/number are set if the window flags are non-zero.
	 * @return The method returns true if the CCD is setup to be non-windowed, false if it is setup to be windowed.
	 * @see #ccd
	 * @see ngat.o.ccd.CCDLibrary#getSetupWindowFlags
	 */
	public boolean checkNonWindowedSetup(COMMAND_DONE done)
	{
		if(ccd.getSetupWindowFlags() > 0)
		{
			String s = new String(":Configured for Windowed Readout:Expecting non-windowed.");
			o.error(s);
			done.setErrorNum(OConstants.O_ERROR_CODE_BASE+310);
			done.setErrorString(s);
			done.setSuccessful(false);
			return false;
		}
		return true;
	}

	/**
	 * Method to get an integer representing a SDSU output Amplifier,that can be passed into the setupDimensions
	 * method of ngat.o.ccd.CCDLibrary. The amplifier to use depends on whether the exposure will be windowed
	 * or not, as windowed exposures sometimes have to use a different amplifier 
	 * (they can't use the DUAL/QUAD readout amplifier setting). 
	 * If windowed is true, the amplifier to use is got from the "o.ccd.config.window.amplifier"
	 * configuration property. If windowed is false, the amplifier to use is got from the "o.ccd.config.amplifier"
	 * configuration property.
	 * This implementation should agree with the eqivalent getDeInterlaceSetting method.
	 * @param windowed A boolean, should be true if we want to use the amplifier for windowing, false
	 *         if we want to use the default amplifier.
	 * @return An integer, representing a valid value to pass into setupDimensions to set the specified
	 *         amplifier.
	 * @exception NullPointerException Thrown if the property name, or it's value, are null.
	 * @exception CCDLibraryFormatException Thrown if the property's value, which is passed into
	 *            dspAmplifierFromString, does not contain a valid amplifier.
	 * @see #getAmplifier(java.lang.String)
	 */
	public int getAmplifier(boolean windowed) throws NullPointerException,CCDLibraryFormatException
	{
		int amplifier;

		if(windowed)
			amplifier = getAmplifier("o.ccd.config.window.amplifier");
		else
			amplifier = getAmplifier("o.ccd.config.amplifier");
		return amplifier;
	}

	/**
	 * Method to get an integer represeting a SDSU output Amplifier, that can be passed into the setupDimensions
	 * method of ngat.o.ccd.CCDLibrary. The amplifier to use is retrieved from the specified property.
	 * @param propertyName A string, of the property keyword, the value of which is used to specify the
	 *        amplifier.
	 * @return An integer, representing a valid value to pass into setupDimensions to set the specified
	 *         amplifier.
	 * @exception NullPointerException Thrown if the property name, or it's value, are null.
	 * @exception CCDLibraryFormatException Thrown if the property's value, which is passed into
	 *            dspAmplifierFromString, does not contain a valid amplifier.
	 * @see #status
	 * @see #ccd
	 * @see OStatus#getProperty
	 * @see ngat.o.ccd.CCDLibrary#dspAmplifierFromString
	 */
	public int getAmplifier(String propertyName) throws NullPointerException,CCDLibraryFormatException
	{
		String propertyValue = null;

		if(propertyName == null)
		{
			throw new NullPointerException(this.getClass().getName()+
						       ":getAmplifier:Property Name was null.");
		}
		propertyValue = status.getProperty(propertyName);
		if(propertyValue == null)
		{
			throw new NullPointerException(this.getClass().getName()+
						       ":getAmplifier:Property Value of keyword "+propertyName+
						       " was null.");
		}
		return ccd.dspAmplifierFromString(propertyValue);
	}

	/**
	 * Method to get an integer representing a SDSU output De-Interlace setting,
	 * that can be passed into the setupDimensions method of ngat.o.ccd.CCDLibrary. 
	 * The setting to use depends on whether the exposure will be windowed
	 * or not, as windowed exposures  sometimes have to use a different amplifier 
	 * (they can't use the DUAL/QUAD readout amplifier setting). 
	 * If windowed is true, the amplifier to use is got from the "o.ccd.config.window.amplifier"
	 * configuration property. If windowed is false, the amplifier to use is got from the "o.ccd.config.amplifier"
	 * configuration property. The chosen property name is passed to getDeInterlaceSetting to get the
	 * equivalent de-interlace setting.
	 * This implementation should agree with the eqivalent getAmplifier method.
	 * @param windowed A boolean, should be true if we want to use the de-interlace setting for windowing, false
	 *         if we want to use the default de-interlace setting.
	 * @return An integer, representing a valid value to pass into setupDimensions to set the specified
	 *         de-interlace setting.
	 * @exception NullPointerException Thrown if getDeInterlaceSetting fails.
	 * @exception CCDLibraryFormatException Thrown if getDeInterlaceSetting fails.
	 * @see #getDeInterlaceSetting(java.lang.String)
	 */
	public int getDeInterlaceSetting(boolean windowed) throws NullPointerException,CCDLibraryFormatException
	{
		int deInterlaceSetting;

		if(windowed)
			deInterlaceSetting = getDeInterlaceSetting("o.ccd.config.window.amplifier");
		else
			deInterlaceSetting = getDeInterlaceSetting("o.ccd.config.amplifier");
		return deInterlaceSetting;
	}

	/**
	 * Method to get an integer represeting a SDSU de-interlace setting,
	 * that can be passed into the setupDimensions method of ngat.o.ccd.CCDLibrary. 
	 * The amplifier to use is retrieved from the specified property, and the de-interlace setting determined 
	 * from this.
	 * @param amplifierPropertyName A string, of the property keyword, the value of which is used to specify the
	 *        amplifier.
	 * @return An integer, representing a valid value to pass into setupDimensions to set the specified
	 *         de-interlace setting.
	 * @exception NullPointerException Thrown if the property name, or it's value, are null.
	 * @exception IllegalArgumentException Thrown if the amplifier was not recognised by this method.
	 * @exception CCDLibraryFormatException Thrown if the derived de-interlace string, which is passed into
	 *            dspDeinterlaceFromString, does not contain a valid de-interlace setting.
	 * @see #getAmplifier
	 * @see ngat.o.ccd.CCDLibrary#dspDeinterlaceFromString
	 */
	public int getDeInterlaceSetting(String amplifierPropertyName) throws NullPointerException,
	                                 IllegalArgumentException,CCDLibraryFormatException
	{
		String deInterlaceString = null;
		int amplifier,deInterlaceSetting;

		amplifier = getAmplifier(amplifierPropertyName);
		// convert Amplifier to De-Interlace Setting string
		switch(amplifier)
		{
			case CCDLibrary.DSP_AMPLIFIER_TOP_LEFT:
				// If TOP_LEFT is the natural setting
				//deInterlaceString = "DSP_DEINTERLACE_SINGLE";
				// diddly to make this all agree with BOTH_RIGHT
				deInterlaceString = "DSP_DEINTERLACE_FLIP_XY";
				break;
			case CCDLibrary.DSP_AMPLIFIER_TOP_RIGHT:
				// If TOP_LEFT is the natural setting
				//deInterlaceString = "DSP_DEINTERLACE_FLIP_X";
				// diddly to make this all agree with BOTH_RIGHT
				deInterlaceString = "DSP_DEINTERLACE_FLIP_Y";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTTOM_LEFT:
				// If TOP_LEFT is the natural setting
				//deInterlaceString = "DSP_DEINTERLACE_FLIP_Y";
				// diddly to make this all agree with BOTH_RIGHT This one untested atm
				deInterlaceString = "DSP_DEINTERLACE_FLIP_X";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTTOM_RIGHT:
				// If TOP_LEFT is the natural setting
				//deInterlaceString = "DSP_DEINTERLACE_FLIP_XY";
				// diddly to make this all agree with BOTH_RIGHT
				deInterlaceString = "DSP_DEINTERLACE_SINGLE";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTH_LEFT:
				// If TOP_LEFT is the natural setting
				//deInterlaceString = "DSP_DEINTERLACE_SPLIT_PARALLEL";
				// diddly to make this all agree with BOTH_RIGHT
				// probably also a flip in X? So this is definately wrong atm
				deInterlaceString = "DSP_DEINTERLACE_SPLIT_PARALLEL";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTH_RIGHT:
				deInterlaceString = "DSP_DEINTERLACE_SPLIT_PARALLEL";
				break;
			case CCDLibrary.DSP_AMPLIFIER_ALL:
				deInterlaceString = "DSP_DEINTERLACE_SPLIT_QUAD";
				break;
			default:
				throw new IllegalArgumentException(this.getClass().getName()+
						       ":getDeInterlaceSetting:Amplifier String of keyword "+
						       amplifierPropertyName+" was illegal value "+amplifier+".");
		}
		// convert de-interlace string into value to pass to ccd.
		deInterlaceSetting = ccd.dspDeinterlaceFromString(deInterlaceString);
		return deInterlaceSetting;
	}

	/**
	 * This method retrieves the current Amplifier configuration used to configure the CCD controller.
	 * This determines which readout(s) the CCD uses. The numeric setting is then converted into a 
	 * valid string as specified by the LT FITS standard.
	 * @return A String is returned, either 'TOPLEFT', 'TOPRIGHT', 'BOTTOMLEFT', 'BOTTOMRIGHT', or 'ALL'. 
	 *         If the amplifier cannot be determined an exception is thrown.
	 * @exception IllegalArgumentException Thrown if the amplifier string cannot be determined.
	 * @see ngat.o.ccd.CCDLibrary#getAmplifier
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_TOP_LEFT
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_TOP_RIGHT
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_BOTTOM_LEFT
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_BOTTOM_RIGHT
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_BOTH_LEFT
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_BOTH_RIGHT
	 * @see ngat.o.ccd.CCDLibrary#DSP_AMPLIFIER_ALL
	 * @see #ccd
	 */
	private String getCCDRDOUTValue() throws IllegalArgumentException
	{
		String amplifierString = null;
		int amplifier;

		//get amplifier from ccd cached setting.
		amplifier = ccd.getAmplifier();
		switch(amplifier)
		{
			case CCDLibrary.DSP_AMPLIFIER_TOP_LEFT:
				amplifierString = "TOPLEFT";
				break;
			case CCDLibrary.DSP_AMPLIFIER_TOP_RIGHT:
				amplifierString = "TOPRIGHT";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTTOM_LEFT:
				amplifierString = "BOTTOMLEFT";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTTOM_RIGHT:
				amplifierString = "BOTTOMRIGHT";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTH_LEFT:
				amplifierString = "BOTHLEFT";
				break;
			case CCDLibrary.DSP_AMPLIFIER_BOTH_RIGHT:
				amplifierString = "BOTHRIGHT";
				break;
			case CCDLibrary.DSP_AMPLIFIER_ALL:
				amplifierString = "ALL";
				break;
			default:
				throw new IllegalArgumentException("getCCDRDOUTValue:amplifier:"+amplifier+
									" not known.");
		}
		return amplifierString;
	}

	/**
	 * Routine to set the telescope focus offset. The offset sent is based on:
	 * <ul>
	 * <li>The instrument's offset with respect to the telescope's natural offset (in the configuration
	 *     property 'o.focus.offset'.
	 * <li>An offset returned from the Beam Steering System, based on the position of it's mechanisms.
	 * <li>A filter specific offset, based on the filter's optical thickness.
	 * <ul>
	 * The BSS focus offset is queryed using a GET_FOCUS_OFFSET command to the BSS. The returned offset
	 * is cached in OStatus, to be used when writing FITS headers.
	 * This method sends a OFFSET_FOCUS_CONTROL command to
	 * the ISS. OFFSET_FOCUS_CONTROL means thats the offset focus will only be enacted if the BSS thinks this
	 * instrument is in control of the FOCUS at the time this command is sent.
	 * @param id The Id is used as the OFFSET_FOCUS_CONTROL and GET_FOCUS_OFFSET command's id.
	 * @param filterId The type of filter we are using.
	 * @exception Exception Thrown if the return value of the OFFSET_FOCUS_CONTROL ISS command is false.
	 *            Thrown if the return value of the GET_FOCUS_OFFSET BSS command is false.
	 * @see #o
	 * @see #status
	 * @see O#sendBSSCommand
	 * @see OStatus#setBSSFocusOffset
	 * @see OStatus#getFilterIdName
	 * @see OStatus#getFilterIdOpticalThickness
	 * @see ngat.message.INST_BSS.GET_FOCUS_OFFSET
	 * @see ngat.message.INST_BSS.GET_FOCUS_OFFSET_DONE
	 * @see ngat.message.INST_BSS.GET_FOCUS_OFFSET_DONE#getFocusOffset
	 */
	protected void setFocusOffset(String id,String filterId) throws Exception
	{
		GET_FOCUS_OFFSET getFocusOffset = null;
		INST_TO_BSS_DONE instToBSSDone = null;
		GET_FOCUS_OFFSET_DONE getFocusOffsetDone = null;
		OFFSET_FOCUS_CONTROL offsetFocusControlCommand = null;
		INST_TO_ISS_DONE instToISSDone = null;
		String instrumentName = null;
		String filterIdName = null;
		String filterTypeString = null;
		float focusOffset = 0.0f;
		boolean bssUse;

		o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+":setFocusOffset:Started.");
	// get instrument name to use for GET_FOCUS_OFFSET/OFFSET_FOCUS_CONTROL
		instrumentName = status.getProperty("o.bss.instrument_name");
		focusOffset = 0.0f;
	// get default focus offset
		focusOffset += status.getPropertyDouble("o.focus.offset");
		o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+":setFocusOffset:Master offset is "+
		      focusOffset+".");
	// filter wheel
		filterIdName = status.getFilterIdName(filterId);
		focusOffset += status.getFilterIdOpticalThickness(filterIdName);
	// get Beam Steering System Offset
		bssUse = status.getPropertyBoolean("o.net.bss.use");
		if(bssUse)
		{
			getFocusOffset = new GET_FOCUS_OFFSET(id);
			getFocusOffset.setInstrumentName(instrumentName);
			o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
			      ":setFocusOffset:Getting BSS focus offset for "+instrumentName+".");
			instToBSSDone = o.sendBSSCommand(getFocusOffset,serverConnectionThread);
			if(instToBSSDone.getSuccessful() == false)
			{
				throw new Exception(this.getClass().getName()+
						    ":setFocusOffset:BSS GET_FOCUS_OFFSET failed:"+
						    instrumentName+":"+instToBSSDone.getErrorString());
			}
			if((instToBSSDone instanceof GET_FOCUS_OFFSET_DONE) == false)
			{
				throw new Exception(this.getClass().getName()+":setFocusOffset:BSS GET_FOCUS_OFFSET("+
						    instrumentName+
						    ") did not return instance of GET_FOCUS_OFFSET_DONE:"+
						    instToBSSDone.getClass().getName());
			}
			getFocusOffsetDone = (GET_FOCUS_OFFSET_DONE)instToBSSDone;
			o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
			      ":setFocusOffset:BSS focus offset for "+instrumentName+" was "+
			      getFocusOffsetDone.getFocusOffset()+".");
			focusOffset += getFocusOffsetDone.getFocusOffset();
	// Cache the BSS focus offset for writing into the FITS headers
			status.setBSSFocusOffset(getFocusOffsetDone.getFocusOffset());
		}
		else
		{
			o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+
			      ":setFocusOffset:BSS not in use, faking BSS GET_FOCUS_OFFSET to 0.0.");
			status.setBSSFocusOffset(0.0f);
		}
	// send the overall focusOffset to the ISS using  OFFSET_FOCUS_CONTROL
		offsetFocusControlCommand = new OFFSET_FOCUS_CONTROL(id);
		offsetFocusControlCommand.setInstrumentName(instrumentName);
		offsetFocusControlCommand.setFocusOffset(focusOffset);
		o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+":setFocusOffset:Total offset for "+
		      instrumentName+" is "+focusOffset+".");
		instToISSDone = o.sendISSCommand(offsetFocusControlCommand,serverConnectionThread);
		if(instToISSDone.getSuccessful() == false)
		{
			throw new Exception(this.getClass().getName()+":focusOffset failed:"+focusOffset+":"+
					    instToISSDone.getErrorString());
		}
		o.log(Logging.VERBOSITY_VERY_TERSE,this.getClass().getName()+":setFocusOffset:Finished.");
	}

	/**
	 * Send a ngat.message.RCS_BSS.BEAM_STEER command to the BSS to move the dichroics to the correct position.
	 * @param id The Id is used as the BEAM_STEER command's id.
	 * @param lowerSlide The position the lower filter slide should be in.
	 * @param upperSlide The position the upper filter slide should be in.
	 * @return The method returns true if the BEAM_STEER command returned successfully, false if an error occured.
	 * @exception Exception Thrown if the beam steer command fails, or returns an error.
	 * @see O#sendBSSCommand(RCS_TO_BSS,OTCPServerConnectionThread,boolean)
	 */
	protected void beamSteer(String id,String lowerSlide,String upperSlide) throws Exception
	{
		BEAM_STEER beamSteer = null;
		RCS_TO_BSS_DONE rcsToBSSDone = null;
		XBeamSteeringConfig beamSteeringConfig = null;
		XOpticalSlideConfig opticalSlideConfig = null;
		boolean bssUse;

		o.log(Logging.VERBOSITY_VERBOSE,this.getClass().getName()+
		      ":beamSteer:Sending BEAM_STEER to BSS with lower slide "+lowerSlide+
		      "and upper slide "+upperSlide+".");
		bssUse = status.getPropertyBoolean("o.net.bss.use");
		if(bssUse)
		{
			beamSteer =  new ngat.message.RCS_BSS.BEAM_STEER(id);
			beamSteeringConfig = new XBeamSteeringConfig();
			beamSteer.setBeamConfig(beamSteeringConfig);
			// upper slide
			opticalSlideConfig = new XOpticalSlideConfig();
			opticalSlideConfig.setSlide(XOpticalSlideConfig.SLIDE_UPPER);
			opticalSlideConfig.setElementName(upperSlide);
			beamSteeringConfig.setUpperSlideConfig(opticalSlideConfig);
			// lower slide
			opticalSlideConfig = new XOpticalSlideConfig();
			opticalSlideConfig.setSlide(XOpticalSlideConfig.SLIDE_LOWER);
			opticalSlideConfig.setElementName(lowerSlide);
			beamSteeringConfig.setLowerSlideConfig(opticalSlideConfig);
			// log contents of command
			o.log(Logging.VERBOSITY_VERBOSE,this.getClass().getName()+
			      ":beamSteer:BEAM_STEER beam steering config now contains:"+
			      beamSteeringConfig.toString()+".");
			o.log(Logging.VERBOSITY_VERBOSE,this.getClass().getName()+
			      ":beamSteer:Sending BEAM_STEER to BSS.");
			// send command
			rcsToBSSDone = o.sendBSSCommand(beamSteer,serverConnectionThread,true);
			// check successful reply
			if(rcsToBSSDone.getSuccessful() == false)
			{
				o.log(Logging.VERBOSITY_VERBOSE,this.getClass().getName()+
				      ":beamSteer:BEAM_STEER returned an error:"+rcsToBSSDone.getErrorString());
				o.error(this.getClass().getName()+":beamSteer:"+id+":"+rcsToBSSDone.getErrorString());
				throw new Exception(this.getClass().getName()+":beamSteer failed:"+
						    rcsToBSSDone.getErrorString());
			}
			o.log(Logging.VERBOSITY_VERBOSE,this.getClass().getName()+
			      ":beamSteer:BEAM_STEER sent successfully.");
		}
		else
		{
			o.log(Logging.VERBOSITY_VERBOSE,this.getClass().getName()+
			      ":beamSteer:BSS not in use: BEAM_STEER command not sent.");
		}
	}
}

//
// $Log: not supported by cvs2svn $
// Revision 1.10  2012/07/17 17:15:08  cjm
// Added BOTHLEFT.
//
// Revision 1.9  2012/07/10 13:41:31  cjm
// Changed calls to CCDLibrary: getNCols/getNRows to getBinnedNCols/getBinnedNRows
// which are more explicit.
//
// Revision 1.8  2012/04/19 13:12:40  cjm
// getFitsHeadersFromBSS/setFocusOffset/beamSteer now retrieve bssUse
// from the "o.net.bss.use" config value, and only invoke sendBSSCommand
// if it is true. sendBSSCommand also checks this config, but adding
// checks here means getFitsHeadersFromBSS will not fail when it gets the
// wrong return class from sendBSSCommand in fake mode.
//
// Revision 1.7  2012/02/08 10:46:10  cjm
// Added beamSteer method, so it can be called from both TWILIGHTCalibrate and ACQUIRE implementations.
//
// Revision 1.6  2012/01/11 14:55:18  cjm
// setFitsHeaders: ROTCENT[XY] POICENT[XY] now scaled by binning/ prescan etc.
// BOTH_RIGHT amplifier setting supported for FITS headers / parsing.
//
// Revision 1.5  2012/01/04 12:03:06  cjm
// Fixed GAIN/EPERDN/READNOIS getValue type.
//
// Revision 1.4  2012/01/04 10:28:31  cjm
// Fixes comment.
//
// Revision 1.3  2012/01/04 10:27:36  cjm
// Added GAIN, READNOIS, and EPERDN code to setFitsHeaders as they all vary by readout amplifier.
//
// Revision 1.2  2011/12/20 11:38:02  cjm
// PRESCAN and POSTSCAN now modified by ncols/amplifier/binning setup.
//
// Revision 1.1  2011/11/23 10:55:24  cjm
// Initial revision
//
//
