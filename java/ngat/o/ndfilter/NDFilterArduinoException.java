// NDFilterArduinoException.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ndfilter/NDFilterArduinoException.java,v 1.1 2013-06-04 08:34:07 cjm Exp $
package ngat.o.ndfilter;


/**
 * This class extends Exception. It is used to throw errors associated with the IO:O neutral density filters Arduino.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class NDFilterArduinoException extends Exception
{
	/**
	 * Revision Control System id string, showing the version of the Class
	 */
	public final static String RCSID = new String("$Id: NDFilterArduinoException.java,v 1.1 2013-06-04 08:34:07 cjm Exp $");

	/**
	 * Constructor for the exception.
	 * @param errorString The error string.
	 */
	public NDFilterArduinoException(String errorString)
	{
		super(errorString);
	}
}

//
// $Log: not supported by cvs2svn $
//
