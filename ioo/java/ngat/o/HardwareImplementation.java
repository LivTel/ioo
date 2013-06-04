// HardwareImplementation.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/HardwareImplementation.java,v 1.2 2013-06-04 08:26:15 cjm Exp $
package ngat.o;

import ngat.message.base.*;
import ngat.o.ccd.*;
import ngat.o.ndfilter.*;

/**
 * This class provides the generic implementation of commands that use hardware to control a mechanism.
 * This is the SDSU CCD controller, and the neutral density filter slides' Arduino.
 * @version $Revision: 1.2 $
 */
public class HardwareImplementation extends CommandImplementation implements JMSCommandImplementation
{
	/**
	 * Revision Control System id string, showing the version of the Class.
	 */
	public final static String RCSID = new String("$Id: HardwareImplementation.java,v 1.2 2013-06-04 08:26:15 cjm Exp $");
	/**
	 * Internal constant used when converting temperatures in centigrade (from the CCD controller/O
	 * configuration file) to Kelvin. Used for FITS headers and GET_STATUS.
	 */
	public final static double CENTIGRADE_TO_KELVIN = 273.15;
	/**
	 * A reference to the CCDLibrary class instance used to communicate with the SDSU CCD Controller.
	 */
	protected CCDLibrary ccd = null;
	/**
	 * A reference to the instance of NDFilterArduino used to communicate with the Arduino used to control
	 * the neutral density filters.
	 */
	protected NDFilterArduino ndFilterArduino = null;

	/**
	 * This method calls the super-classes method. It then tries to fill in the reference to the hardware
	 * objects.
	 * @param command The command to be implemented.
	 * @see #o
	 * @see #ccd
	 * @see #ndFilterArduino
	 * @see O#getCCD
	 * @see O#getNDFilterArduino
	 */
	public void init(COMMAND command)
	{
		super.init(command);
		if(o != null)
		{
			ccd = o.getCCD();
			ndFilterArduino = o.getNDFilterArduino();
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
}

//
// $Log: not supported by cvs2svn $
// Revision 1.1  2011/11/23 10:55:24  cjm
// Initial revision
//
//
