// CCDLibraryNativeException.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ccd/CCDLibraryNativeException.java,v 1.1 2011-11-23 10:59:30 cjm Exp $
package ngat.o.ccd;

/**
 * This class extends Exception. Objects of this class are thrown when the underlying C code in CCDLibrary produces an
 * error. The JNI interface itself can also generate these exceptions.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class CCDLibraryNativeException extends Exception
{
	/**
	 * Revision Control System id string, showing the version of the Class
	 */
	public final static String RCSID = new String("$Id: CCDLibraryNativeException.java,v 1.1 2011-11-23 10:59:30 cjm Exp $");
	/**
	 * The current value of the error number in the DSP module.
	 */
	protected int dspErrorNumber = 0;
	/**
	 * The current value of the error number in the Exposure module.
	 */
	protected int exposureErrorNumber = 0;
	/**
	 * The current value of the error number in the filter wheel module.
	 */
	protected int filterWheelErrorNumber = 0;
	/**
	 * The current value of the error number in the Interface module.
	 */
	protected int interfaceErrorNumber = 0;
	/**
	 * The current value of the error number in the PCI module.
	 */
	protected int pciErrorNumber = 0;
	/**
	 * The current value of the error number in the Setup module.
	 */
	protected int setupErrorNumber = 0;
	/**
	 * The current value of the error number in the Temperature module.
	 */
	protected int temperatureErrorNumber = 0;
	/**
	 * The current value of the error number in the Text module.
	 */
	protected int textErrorNumber = 0;

	/**
	 * Constructor for the exception.
	 * @param errorString The error string.
	 */
	public CCDLibraryNativeException(String errorString)
	{
		super(errorString);
	}

	/**
	 * Constructor for the exception. Used from C JNI interface.
	 * The error number fields of the created exception are filled with error numbers retrieved using
	 * JNI calls to the libccd instance passed in.
	 * @param errorString The error string.
	 * @param libo_ccd The libo_ccd instance that caused this excecption.
	 * @see #dspErrorNumber
	 * @see #exposureErrorNumber
	 * @see #filterWheelErrorNumber
	 * @see #interfaceErrorNumber
	 * @see #pciErrorNumber
	 * @see #setupErrorNumber
	 * @see #temperatureErrorNumber
	 * @see #textErrorNumber
	 * @see CCDLibrary#getDSPErrorNumber
	 * @see CCDLibrary#getExposureErrorNumber
	 * @see CCDLibrary#getInterfaceErrorNumber
	 * @see CCDLibrary#getPCIErrorNumber
	 * @see CCDLibrary#getSetupErrorNumber
	 * @see CCDLibrary#getTemperatureErrorNumber
	 * @see CCDLibrary#getTextErrorNumber
	 */
	public CCDLibraryNativeException(String errorString,CCDLibrary libo_ccd)
	{
		super(errorString);
		this.dspErrorNumber = libo_ccd.getDSPErrorNumber();
		this.exposureErrorNumber = libo_ccd.getExposureErrorNumber();
		//this.filterWheelErrorNumber = libo_ccd.getFilterWheelErrorNumber();
		this.interfaceErrorNumber = libo_ccd.getInterfaceErrorNumber();
		this.pciErrorNumber = libo_ccd.getPCIErrorNumber();
		this.setupErrorNumber = libo_ccd.getSetupErrorNumber();
		this.temperatureErrorNumber = libo_ccd.getTemperatureErrorNumber();
		this.textErrorNumber = libo_ccd.getTextErrorNumber();
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #dspErrorNumber
	 */
	public int getDSPErrorNumber()
	{
		return dspErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #exposureErrorNumber
	 */
	public int getExposureErrorNumber()
	{
		return exposureErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #filterWheelErrorNumber
	 */
	public int getFilterWheelErrorNumber()
	{
		return filterWheelErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #interfaceErrorNumber
	 */
	public int getInterfaceErrorNumber()
	{
		return interfaceErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #pciErrorNumber
	 */
	public int getPCIErrorNumber()
	{
		return pciErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #setupErrorNumber
	 */
	public int getSetupErrorNumber()
	{
		return setupErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #temperatureErrorNumber
	 */
	public int getTemperatureErrorNumber()
	{
		return temperatureErrorNumber;
	}

	/**
	 * Retrieve routine for the error number for the relevant C module.
	 * @return Returns the error number supplied for this exception, 
	 * 	if the number was supplied in a constructor.
	 * @see #textErrorNumber
	 */
	public int getTextErrorNumber()
	{
		return textErrorNumber;
	}
}

//
// $Log: not supported by cvs2svn $
//
