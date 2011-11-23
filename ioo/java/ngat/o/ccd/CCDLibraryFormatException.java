// CCDLibraryFormatException.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ccd/CCDLibraryFormatException.java,v 1.1 2011-11-23 10:59:30 cjm Exp $
package ngat.o.ccd;

/**
 * This class extends java.lang.IllegalArgumentException. Objects of this class are thrown when an illegal
 * format argument is passed into various parse routines in CCDLibrary.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class CCDLibraryFormatException extends IllegalArgumentException
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: CCDLibraryFormatException.java,v 1.1 2011-11-23 10:59:30 cjm Exp $");

	/**
	 * Constructor for the exception.
	 * @param fromClassName The name of the class the exception occured in.
	 * @param methodName The name of the method the exception occured in.
	 * @param illegalParameter The illegal string that could not be parsed by the method.
	 */
	public CCDLibraryFormatException(String fromClassName,String methodName,String illegalParameter)
	{
		super("CCDLibraryFormatException:"+fromClassName+":"+methodName+":Illegal Parameter:"+
			illegalParameter);
	}
}

//
// $Log: not supported by cvs2svn $
//
