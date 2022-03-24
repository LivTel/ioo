// CCDLibrarySetupWindow.java
// $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/ccd/CCDLibrarySetupWindow.java,v 1.1 2011-11-23 10:59:30 cjm Exp $
package ngat.o.ccd;

/**
 * This class allows us to specify the location of a window on the CCD chip. It is used as a parameter to
 * the setup method that sets the windows on the controller.
 * @author Chris Mottram
 * @version $Revision: 1.1 $
 */
public class CCDLibrarySetupWindow
{
	/**
	 * Revision Control System id string, showing the version of the Class
	 */
	public final static String RCSID = new String("$Id: CCDLibrarySetupWindow.java,v 1.1 2011-11-23 10:59:30 cjm Exp $");
	/**
	 * The pixel number of the X start position of the window (upper left corner).
	 */
	private int xStart;
	/**
	 * The pixel number of the Y start position of the window (upper left corner).
	 */
	private int yStart;
	/**
	 * The pixel number of the X end position of the window (lower right corner).
	 */
	private int xEnd;
	/**
	 * The pixel number of the Y end position of the window (lower right corner).
	 */
	private int yEnd;

	/**
	 * Default constructor. Sets all the positions to -1.
	 */
	public CCDLibrarySetupWindow()
	{
		xStart = -1;
		yStart = -1;
		xEnd = -1;
		yEnd = -1;
	}

	/**
	 * Constructor. Sets the window to the specified position.
	 * @param xs The X Start position.
	 * @param ys The Y Start position.
	 * @param xe The X End position.
	 * @param ye The Y End position.
	 */
	public CCDLibrarySetupWindow(int xs,int ys,int xe,int ye)
	{
		xStart = xs;
		yStart = ys;
		xEnd = xe;
		yEnd = ye;
	}

	/**
	 * This method sets the value of X Start position of the window.
	 * @param value The pixel position.
	 */
	public void setXStart(int value)
	{
		xStart = value;
	}

	/**
	 * This method gets the value of X Start position of the window.
	 * @return The current value of the position.
	 */
	public int getXStart()
	{
		return xStart;
	}

	/**
	 * This method sets the value of Y Start position of the window.
	 * @param value The pixel position.
	 */
	public void setYStart(int value)
	{
		yStart = value;
	}

	/**
	 * This method gets the value of Y Start position of the window.
	 * @return The current value of the position.
	 */
	public int getYStart()
	{
		return yStart;
	}

	/**
	 * This method sets the value of X End position of the window.
	 * @param value The pixel position.
	 */
	public void setXEnd(int value)
	{
		xEnd = value;
	}

	/**
	 * This method gets the value of X End position of the window.
	 * @return The current value of the position.
	 */
	public int getXEnd()
	{
		return xEnd;
	}

	/**
	 * This method sets the value of Y End position of the window.
	 * @param value The pixel position.
	 */
	public void setYEnd(int value)
	{
		yEnd = value;
	}

	/**
	 * This method gets the value of Y End position of the window.
	 * @return The current value of the position.
	 */
	public int getYEnd()
	{
		return yEnd;
	}
}
 
//
// $Log: not supported by cvs2svn $
//
