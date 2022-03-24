// NDFilterArduinoMoveException.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ndfilter/NDFilterArduinoMoveException.java,v 1.1 2013-06-04 08:34:07 cjm Exp $
package ngat.o.ndfilter;

/**
 * This class extends NDFilterArduinoException. It is used to throw move errors associated with the 
 * IO:O neutral density filter Arduino controller.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 * @see ngat.o.ndfilter.NDFilterArduinoException
 */
public class NDFilterArduinoMoveException extends NDFilterArduinoException
{
	/**
	 * Revision Control System id string, showing the version of the Class
	 */
	public final static String RCSID = new String("$Id: NDFilterArduinoMoveException.java,v 1.1 2013-06-04 08:34:07 cjm Exp $");
	/**
	 * The error code returned by the move command.
	 */
	int errorCode;

	/**
	 * Constructor for the exception.
	 * @param errorString The error string.
	 */
	public NDFilterArduinoMoveException(String errorString)
	{
		super(errorString);
	}

	/**
	 * Constructor for the exception. makeErrorString is called to create a sensible error string 
	 * associated with the error code.
	 * @param errorCode An integer error number returned by the underlying Arduino.
	 * @see #errorCode
	 * @see #makeErrorString
	 */
	public NDFilterArduinoMoveException(int errorCode)
	{
		super(makeErrorString(errorCode));
		this.errorCode = errorCode;
	}

	/**
	 * Return the error code associated with this exception.
	 * @return An integer error code.
	 * @see #errorCode
	 */
	public int getErrorCode()
	{
		return errorCode;
	}

	/**
	 * Class method called during superclass construction to generate a sensible error message based on
	 * the error code. These error messages are based on the Arduino sketch in repository: svn://ltdevsrv/ioo
	 * , sketch ioo/NDstageControl/NDstageControl.ino.
	 * @param errorCode The error code to generate an error message for.
	 * @return A string describing the error message.
	 */
	protected static String makeErrorString(int errorCode)
	{
		switch(errorCode)
		{
			case 1:
				return new String ("messageReady:ERROR 1:Unknown filter in get command.");
			case 2:
				return new String ("messageReady:ERROR 2:Unknown get command.");
			case 3:
				return new String ("messageReady:ERROR 3:Unknown filter in move command.");
			case 4:
				return new String ("messageReady:ERROR 4:Unknown move command.");
			case 5:
				return new String ("messageReady:ERROR 5:Unknown command.");
			case 6:
				return new String ("moveStow:ERROR 6:Stow timeout.");
			case 7:
				return new String ("moveStow:ERROR 7:Filter NOT in stowed position.");
			case 8:
				return new String ("moveStow:ERROR 8:Filter in deployed position.");
			case 9:
				return new String ("moveDeploy:ERROR 9:Deploy timeout.");
			case 10:
				return new String ("moveDeploy:ERROR 10:Filter is NOT in deployed position.");
			case 11:
				return new String ("moveDeploy:ERROR 11:Filter is in stowed position.");
			default:
				return new String ("NDFilterArduinoMoveException: move returned error code:"+
						   errorCode);
		}
	}
}

//
// $Log: not supported by cvs2svn $
//
