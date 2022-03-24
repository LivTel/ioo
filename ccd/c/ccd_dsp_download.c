/* ccd_dsp_download.c
** ccd library
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ccd_dsp_download.c,v 1.1 2011-11-23 10:59:52 cjm Exp $
*/
/**
 * ccd_dsp_download.c contains the code to download DSP code to the SDSU controller.
 * @author SDSU, Chris Mottram
 * @version $Revision: 1.1 $
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for clock_gettime.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for clock_gettime.
 */
#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "log_udp.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_dsp.h"
#include "ccd_global.h"

/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ccd_dsp_download.c,v 1.1 2011-11-23 10:59:52 cjm Exp $";

/* defines */
/**
 * The maximum memory address of code that isn't boot code. This is used in 
 * <a href="#CCD_DSP_Download">CCD_DSP_Download</a> so that only application code is downloaded
 * and not the boot code bundled with it.
 * @see #CCD_DSP_Download
 */
#define	DSP_DOWNLOAD_ADDR_MAX			(0x4000) /* maximum address of code that isn't boot code */
/**
 * Bits 8 and 9 in the HCTR (host interface control register) on the PCI interface board are used to
 * put the PCI DSP into slave mode, to download DSP code to the PCI interface board. Bit 8 is cleared
 * when putting the PCI DSP into slave mode.
 */
#define DSP_DOWNLOAD_HCTR_HTF_BIT8		(1<<8)
/**
 * Bits 8 and 9 in the HCTR (host interface control register) on the PCI interface board are used to
 * put the PCI DSP into slave mode, to download DSP code to the PCI interface board. If bit 9 is set,
 * the PCI DSP is in slave mode.
 */
#define DSP_DOWNLOAD_HCTR_HTF_BIT9		(1<<9)
/**
 * Used to reset the HCTR after a SDSU PCI board download.
 */
#define DSP_DOWNLOAD_HCTR_HTF_BIT11		(1<<11)
/**
 * Bit 3 in the HCTR (host interface control register) on the PCI interface board is set when an image
 * buffer is being transferred to user space.
 */
#define DSP_DOWNLOAD_HCTR_IMAGE_BUFFER_BIT	(1<<3)
/**
 * Magic constant value sent to the PCI DSP as argument one, at the commencement of downloading PCI interface
 * DSP code. The PCI DSP decides to allow the download once this value has been sent.
 */
#define DSP_DOWNLOAD_PCI_BOOT_LOAD		(0x00555AAA)
/**
 * DSP SECTION name for the DSP code used to control the PCI interface board.
 * Used in DSP_Download_PCI_Interface to verify DSP code is for the PCI interface.
 * @see #DSP_Download_PCI_Interface
 */
#define DSP_DOWNLOAD_PCI_BOOT_STRING		("PCIBOOT")
/**
 * String used in a PCI interface DSP code file to indicate the start of a program segment of code.
 * @see #DSP_Download_PCI_Interface
 */
#define DSP_DOWNLOAD_PCI_DATA_PROGRAM_STRING	("_DATA P")

/* internal variables */
/**
 * Variable holding error code of last operation performed by ccd_dsp_download.
 */
static int DSP_Download_Error_Number = 0;
/**
 * Internal  variable holding description of the last error that occured.
 */
static char DSP_Download_Error_String[CCD_GLOBAL_ERROR_STRING_LENGTH] = "";

/* internal functions */
static int DSP_Download_Timing_Utility(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,char *filename);
static int DSP_Download_PCI_Interface(CCD_Interface_Handle_T* handle,char *filename);
static int DSP_Download_PCI_Finish(CCD_Interface_Handle_T* handle);
static int DSP_Download_Read_Line(FILE *fp, char *buff);
static int DSP_Download_Get_Type(FILE *fp);
static int DSP_Download_Address_Char_To_Mem_Space(char ch,enum CCD_DSP_MEM_SPACE *mem_space);
static int DSP_Download_Process_Data(CCD_Interface_Handle_T* handle,FILE *download_fp,enum CCD_DSP_BOARD_ID board_id,
	enum CCD_DSP_MEM_SPACE mem_space,int addr);

/* external functions */
/**
 * This routine sets up ccd_dsp_download internal variables.
 * It should be called at startup.
 * @return Return TRUE if initialisation is successful, FALSE if it wasn't.
 */
int CCD_DSP_Download_Initialise(void)
{
	DSP_Download_Error_Number = 0;
	strcpy(DSP_Download_Error_String,"");
/* print some compile time information to stdout */
	fprintf(stdout,"CCD_DSP_Download_Initialise:%s.\n",rcsid);
	return TRUE;
}

/**
 * Downloads some DSP code to one of the boards from filename.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param board The board to send the command to.
 * @param filename The filename of compiled DSP commends to send to the board.
 * 	This is usually a .lod file.
 * @return Returns TRUE if the operation succeeds, FALSE if it fails.
 * @see #DSP_Download_PCI_Interface
 * @see #DSP_Download_Timing_Utility
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
int CCD_DSP_Download(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,char *filename)
{
	int retval;

	DSP_Download_Error_Number = 0;
	if(!CCD_DSP_IS_BOARD_ID(board_id))
	{
		DSP_Download_Error_Number = 1;
		sprintf(DSP_Download_Error_String,"CCD_DSP_Download:Illegal board ID '%d'.",board_id);
		return FALSE;
	}
	if(filename == NULL)
	{
		DSP_Download_Error_Number = 2;
		sprintf(DSP_Download_Error_String,"CCD_DSP_Download:Filename for board '%d' is NULL.",board_id);
		return FALSE;
	}
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download(%#x,%s) started.",
		board_id,filename);
#endif
/* depending on the board type, call a sub-routine */
	switch(board_id)
	{
		case CCD_DSP_HOST_BOARD_ID:
			DSP_Download_Error_Number = 3;
			sprintf(DSP_Download_Error_String,
				"CCD_DSP_Download:Can't download DSP code to Host computer.");
			return FALSE;
		case CCD_DSP_INTERFACE_BOARD_ID:
			retval = DSP_Download_PCI_Interface(handle,filename);
			break;
		case CCD_DSP_TIM_BOARD_ID:
		case CCD_DSP_UTIL_BOARD_ID:
			retval = DSP_Download_Timing_Utility(handle,board_id,filename);
			break;
		default:
			DSP_Download_Error_Number = 4;
			sprintf(DSP_Download_Error_String,"CCD_DSP_Download:Unknown board ID '%d'.",board_id);
			return FALSE;
	}
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download(%#x,%s) returned %#x.",
		board_id,filename,retval);
#endif
	return retval;
}

/**
 * Get the current value of ccd_dsp_download's error number.
 * @return The current value of ccd_dsp_download's error number.
 * @see #DSP_Download_Error_Number
 */
int CCD_DSP_Download_Get_Error_Number(void)
{
	return DSP_Download_Error_Number;
}

/**
 * The error routine that reports any errors occuring in ccd_dsp_download in a standard way.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 * @see #DSP_Download_Error_Number
 * @see #DSP_Download_Error_String
 */
void CCD_DSP_Download_Error(void)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(DSP_Download_Error_Number == 0)
		sprintf(DSP_Download_Error_String,"Logic Error:No Error defined");
	fprintf(stderr,"%s CCD_DSP_Download:Error(%d) : %s\n",time_string,DSP_Download_Error_Number,
		DSP_Download_Error_String);
}

/**
 * The error routine that reports any errors occuring in ccd_dsp_download in a standard way. This routine places the
 * generated error string at the end of a passed in string argument.
 * @param error_string A string to put the generated error in. This string should be initialised before
 * being passed to this routine. The routine will try to concatenate it's error string onto the end
 * of any string already in existance.
 * @see ccd_global.html#CCD_Global_Get_Current_Time_String
 */
void CCD_DSP_Download_Error_String(char *error_string)
{
	char time_string[32];

	CCD_Global_Get_Current_Time_String(time_string,32);
	/* if the error number is zero an error message has not been set up
	** This is in itself an error as we should not be calling this routine
	** without there being an error to display */
	if(DSP_Download_Error_Number == 0)
		sprintf(DSP_Download_Error_String,"Logic Error:No Error defined");
	sprintf(error_string+strlen(error_string),"%s CCD_DSP_Download:Error(%d) : %s\n",time_string,
		DSP_Download_Error_Number,DSP_Download_Error_String);
}

/* ----------------------------------------------------------------
**	Internal routines
** ---------------------------------------------------------------- */
/**
 * Downloads some DSP code to either the timing or utility board from filename.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param board The board to send the command to.
 * @param filename The filename of compiled DSP commends to send to the board.
 * 	This is usually a .lod file.
 * @return Returns TRUE if the operation succeeds, FALSE if it fails.
 * @see #DSP_Download_Get_Type
 * @see #DSP_Download_Read_Line
 * @see #DSP_Download_Address_Char_To_Mem_Space
 * @see #DSP_Download_Process_Data
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int DSP_Download_Timing_Utility(CCD_Interface_Handle_T* handle,enum CCD_DSP_BOARD_ID board_id,char *filename)
{
	FILE *download_fp;
	enum CCD_DSP_MEM_SPACE mem_space;
	int finished,download_board_id,addr;
	char buff[255],addr_type;

/* try to open the file */
	if((download_fp = fopen(filename,"r")) == NULL)
	{
		DSP_Download_Error_Number = 5;
		sprintf(DSP_Download_Error_String,"CCD_DSP_Download_Timing_Utility:Could not open filename(%s).",
			filename);
		return FALSE;
	}
/* ensure the file is for the same board as the one we are trying to send a program to */
	if((download_board_id = DSP_Download_Get_Type(download_fp)) == FALSE)
	{
		fclose(download_fp);
		DSP_Download_Error_Number = 6;
		sprintf(DSP_Download_Error_String,"CCD_DSP_Download_Timing_Utility:Could not get filename type(%s).",
			filename);
		return FALSE;
	}

	if (download_board_id != board_id)
	{
		fclose(download_fp);
		DSP_Download_Error_Number = 7;
		sprintf(DSP_Download_Error_String,"CCD_DSP_Download_Timing_Utility:Boards do not match(%s,%d,%d).",
			filename,download_board_id,board_id);
		return FALSE;
	}
	finished = FALSE;
/* send the data to the board until the end of the file is reached 
** or the operation is aborted */
	while((!finished)&&(!CCD_DSP_Get_Abort()))
	{
		DSP_Download_Read_Line(download_fp,buff);
		if(strncmp(buff,"_END",4) == 0)
			finished = TRUE;
		else if(sscanf(buff,"_DATA %c %x",&addr_type,(unsigned int *)&addr) == 2)
		{
			if(!DSP_Download_Address_Char_To_Mem_Space(addr_type,&mem_space))
			{
				fclose(download_fp);
				return FALSE;
			}
			if (addr < DSP_DOWNLOAD_ADDR_MAX)
			{
				if(!DSP_Download_Process_Data(handle,download_fp,board_id,mem_space,addr))
				{
					fclose(download_fp);
					return FALSE;
				}
			}
		}
	}
	fclose(download_fp);
	return(TRUE);
}

/**
 * Downloads some DSP code to the PCI interface board from filename. This is done by setting the PCI interface
 * DSP chip to slave mode, and using HCVR_DATA calls to download program data. The PCI interface board must
 * be set back to master mode on completion.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param board The board to download the program to.
 * @param filename The filename of compiled DSP commends to send to the board.
 * 	This is usually a .lod file.
 * @return Returns TRUE if the operation succeeds, FALSE if it fails.
 * @see #DSP_Download_Read_Line
 * @see #DSP_Download_PCI_Finish
 * @see #DSP_DOWNLOAD_HCTR_HTF_BIT8
 * @see #DSP_DOWNLOAD_HCTR_HTF_BIT9
 * @see #DSP_DOWNLOAD_HCTR_IMAGE_BUFFER_BIT
 * @see #DSP_DOWNLOAD_PCI_BOOT_STRING
 * @see #DSP_DOWNLOAD_PCI_DATA_PROGRAM_STRING
 * @see #DSP_DOWNLOAD_PCI_BOOT_LOAD
 * @see ccd_dsp.html#CCD_DSP_Command_PCI_Download
 * @see ccd_dsp.html#CCD_DSP_Command_PCI_Download_Get_Reply
 * @see ccd_pci.html#CCD_Interface_Command
 * @see ccd_pci.html#CCD_PCI_IOCTL_GET_HCTR
 * @see ccd_pci.html#CCD_PCI_IOCTL_SET_HCTR
 * @see ccd_pci.html#CCD_PCI_IOCTL_HCVR_DATA
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int DSP_Download_PCI_Interface(CCD_Interface_Handle_T* handle,char *filename)
{
	FILE *download_fp = NULL;
	char buff[81];
	char *value_string = NULL;
	int host_control_reg,argument,done,word_count,address,retval;
	int word_number = 0;

/* try to open the file */
	if((download_fp = fopen(filename,"r")) == NULL)
	{
		DSP_Download_Error_Number = 8;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Could not open filename(%s).",
			filename);
		return FALSE;
	}
/* get host interface control register */
	if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_GET_HCTR,&host_control_reg))
	{
		fclose(download_fp);
		DSP_Download_Error_Number = 9;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Getting Host Control Register failed.");
		return FALSE;
	}
/* clear HTF bits (8 and 9) and image buffer bit (3) */
 	host_control_reg = host_control_reg & (~(DSP_DOWNLOAD_HCTR_HTF_BIT8|DSP_DOWNLOAD_HCTR_HTF_BIT9|
		DSP_DOWNLOAD_HCTR_IMAGE_BUFFER_BIT));
/* set HTF bit 9 */
	host_control_reg = host_control_reg|DSP_DOWNLOAD_HCTR_HTF_BIT9;
/* set host interface control register */
	if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_SET_HCTR,&host_control_reg))
	{
		fclose(download_fp);
		DSP_Download_Error_Number = 10;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Setting Host Control Register failed.");
		return FALSE;
	}
/* Inform the DSP that new pci boot code will be downloaded.*/
	if(!CCD_DSP_Command_PCI_Download(handle))
	{
		DSP_Download_PCI_Finish(handle);
		fclose(download_fp);
		DSP_Download_Error_Number = 11;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Sending PCI download command failed.");
		return FALSE;
	}
/* send the magic number that says this is a pci boot load. */
	argument = DSP_DOWNLOAD_PCI_BOOT_LOAD;
	if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_HCVR_DATA,&argument))
	{
		DSP_Download_PCI_Finish(handle);
		fclose(download_fp);
		DSP_Download_Error_Number = 12;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Sending PCI magic number failed.");
		return FALSE;
	}
/* get first line from input file, check file is a PCIBOOT DSP code file */
	if(!DSP_Download_Read_Line(download_fp,buff))
	{
		DSP_Download_PCI_Finish(handle);
		fclose(download_fp);
		DSP_Download_Error_Number = 13;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Reading first line from '%s failed.",
			filename);
		return FALSE;
	}
#if LOGGING > 4
	CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download:First Line:%s.",buff);
#endif
	if(strstr(buff,DSP_DOWNLOAD_PCI_BOOT_STRING) == NULL)
	{
		DSP_Download_PCI_Finish(handle);
		fclose(download_fp);
		DSP_Download_Error_Number = 14;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:"
			"First line in filename '%s' does not include '%s'.",filename,DSP_DOWNLOAD_PCI_BOOT_STRING);
		return FALSE;
	}
/* while reading data, get a line and process it */
	done = FALSE;
	while(done == FALSE)
	{
		if(!DSP_Download_Read_Line(download_fp,buff))
		{
			DSP_Download_PCI_Finish(handle);
			fclose(download_fp);
			DSP_Download_Error_Number = 15;
			sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:Reading line from '%s failed.",
				filename);
			return FALSE;
		}
#if LOGGING > 4
		CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download:Read Line:%s.",buff);
#endif
		if(strstr(buff,DSP_DOWNLOAD_PCI_DATA_PROGRAM_STRING) != NULL)
		{
			if(!DSP_Download_Read_Line(download_fp,buff))
			{
				DSP_Download_PCI_Finish(handle);
				fclose(download_fp);
				DSP_Download_Error_Number = 16;
				sprintf(DSP_Download_Error_String,
					"DSP_Download_PCI_Interface:Reading line from '%s failed.",
					filename);
				return FALSE;
			}
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download:Read _DATA P Line:%s.",buff);
#endif
			retval = sscanf(buff,"%x %x",(unsigned int *)&word_count,(unsigned int *)&address);
			if(retval != 2)
			{
				DSP_Download_PCI_Finish(handle);
				fclose(download_fp);
				DSP_Download_Error_Number = 17;
				sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:"
					"Reading line '%s' from '%s' failed.",buff,filename);
				return FALSE;
			}
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download:Word Count %d:Address:%#x.",
				word_count,address);
#endif
		/* send word count */
			if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_HCVR_DATA,&word_count))
			{
				DSP_Download_PCI_Finish(handle);
				fclose(download_fp);
				DSP_Download_Error_Number = 18;
				sprintf(DSP_Download_Error_String,
					"DSP_Download_PCI_Interface:Sending word count %#x failed.",
					word_count);
				return FALSE;
			}
		/* send address */
			if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_HCVR_DATA,&address))
			{
				DSP_Download_PCI_Finish(handle);
				fclose(download_fp);
				DSP_Download_Error_Number = 19;
				sprintf(DSP_Download_Error_String,
					"DSP_Download_PCI_Interface:Sending address %#x failed.",
					address);
				return FALSE;
			}
		/* throw away next line (e.g. _DATA P 000002) - this does not make sense to me. */
			if(!DSP_Download_Read_Line(download_fp,buff))
			{
				DSP_Download_PCI_Finish(handle);
				fclose(download_fp);
				DSP_Download_Error_Number = 20;
				sprintf(DSP_Download_Error_String,
					"DSP_Download_PCI_Interface:Reading line from '%s failed.",
					filename);
				return FALSE;
			}
#if LOGGING > 4
			CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download:Throw away Line:%s.",buff);
#endif
			while(word_number < word_count)
			{
				if(!DSP_Download_Read_Line(download_fp,buff))
				{
					DSP_Download_PCI_Finish(handle);
					fclose(download_fp);
					DSP_Download_Error_Number = 21;
					sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:"
						"Reading line from '%s' failed.",filename);
					return FALSE;
				}
#if LOGGING > 4
				CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,"CCD_DSP_Download:Read data Line:%s.",
					buff);
#endif
			/* if line does not contain "_DATA P", it must contain program data */
				if(strstr(buff,DSP_DOWNLOAD_PCI_DATA_PROGRAM_STRING) == NULL)
				{
					value_string = strtok(buff," \t\n");
					while(value_string != NULL)
					{
					/* Check the word number. */
						if (word_number >= word_count)
							break;
#if LOGGING > 4
						CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
							"CCD_DSP_Download:Memory Value:%s.",value_string);
#endif
						retval = sscanf(value_string,"%x",(unsigned int *)&argument);
						if(retval != 1)
						{
							DSP_Download_PCI_Finish(handle);
							fclose(download_fp);
							DSP_Download_Error_Number = 22;
							sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:"
								"Scanning program word '%s' (%d of %d) failed.",
								value_string,word_number,word_count);
							return FALSE;
						}
#if LOGGING > 4
						CCD_Global_Log_Format(LOG_VERBOSITY_VERY_VERBOSE,
							"CCD_DSP_Download:Memory Value:%#x Word (%d of %d).",
							argument,word_number,word_count);
#endif
					/* send program word. */
						if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_HCVR_DATA,&argument))
						{
							DSP_Download_PCI_Finish(handle);
							fclose(download_fp);
							DSP_Download_Error_Number = 23;
							sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Interface:"
								"Sending program word %#x (%d of %d) failed.",
								argument,word_number,word_count);
							return FALSE;
						}
						word_number++;
					/* get next token */
						value_string = strtok(NULL," \t\n");
					}/* while next token in buff */
				}/*end if line did not contain "_DATA P" */
			}/* end while number of words is less than word_count-2 */
			done = TRUE;
		}/* end if string is DATA _P */
	}/* end while on done */
/* return PCI interface DSP from slave mode */
	if(!DSP_Download_PCI_Finish(handle))
	{
		fclose(download_fp);
		return FALSE;
	}
/* close file */
	fclose(download_fp);
/* return */
	return TRUE;
}

/**
 * Routine to finish a DSP program download to the PCI interface board. This involves clearing
 * the HTF bits so the PCI DSP returns from slave mode, and setting bit 3.
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @return The routine returns TRUE if the reset succeeded, FALSE if it failed (an error is set).
 * @see #CCD_DSP_Command_PCI_Download_Wait
 * @see ccd_interface.html#CCD_Interface_Command
 * @see ccd_pci.html#CCD_PCI_IOCTL_GET_HCTR
 * @see ccd_pci.html#CCD_PCI_IOCTL_SET_HCTR
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int DSP_Download_PCI_Finish(CCD_Interface_Handle_T* handle)
{
	int host_control_reg;

/* get host interface control register */
	if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_GET_HCTR,&host_control_reg))
	{
		DSP_Download_Error_Number = 25;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Finish:Getting Host Control Register failed.");
		return FALSE;
	}
/* clear HTF bits (8 and 9) */
 	host_control_reg = host_control_reg & (~(DSP_DOWNLOAD_HCTR_HTF_BIT8|DSP_DOWNLOAD_HCTR_HTF_BIT9));
/* set bits (1<<8) and (1<<11) */
	host_control_reg = host_control_reg | (DSP_DOWNLOAD_HCTR_HTF_BIT8|DSP_DOWNLOAD_HCTR_HTF_BIT11);
/* set host interface control register */
	if(!CCD_Interface_Command(handle,CCD_PCI_IOCTL_SET_HCTR,&host_control_reg))
	{
		DSP_Download_Error_Number = 26;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Finish:Setting Host Control Register(1) failed.");
		return FALSE;
	}
/* wait for PCI to complete download */
	if(CCD_DSP_Command_PCI_Download_Wait(handle) != CCD_DSP_DON)
	{
		DSP_Download_Error_Number = 24;
		sprintf(DSP_Download_Error_String,"DSP_Download_PCI_Finish:Waiting for PCI download failed.");
		return FALSE;
	}
	return TRUE;
}

/**
 * This routine gets a line of data from a file pointer and puts it into a buffer. It is used when
 * downloading a DSP application program from a .lod file.
 * @param fp The file pointer to get input from.
 * @param buff The buffer to put the input into. The buffer must have room for at least 80 characters.
 * @return Returns TRUE if getting the line succeeded, FALSE otherwise.
 */
static int DSP_Download_Read_Line(FILE *fp, char *buff)
{
	if (fgets(buff,80,fp) == NULL)
	{
		perror("error on input");
		strcpy(buff,"");
		return FALSE;
	}
	return TRUE;
}

/**
 * This routine gets which board the .lod file opened using fp is meant for. The routine is called
 * from <a href="#CCD_DSP_Download">CCD_DSP_Download</a>. Note this routine won't work for PCI DSP
 * downloads at the moment.
 * @param fp The file pointer to get the input from.
 * @return Returns which board the .lod file should be sent to (either
 * 	CCD_DSP_TIM_BOARD_ID(timing board) or CCD_DSP_UTIL_BOARD_ID(utility board)), 
 * 	otherwise returns FALSE.
 */
static int DSP_Download_Get_Type(FILE *fp)
{
	char    *cp, buff[255];
	int     id;

	DSP_Download_Read_Line(fp,buff);
	if (strncmp(buff,"_START",6) != 0)
		return FALSE;

	cp = buff+7;
	switch(*cp)
	{
		case 'T':
			id = CCD_DSP_TIM_BOARD_ID;
			break;
		case 'U':
			id = CCD_DSP_UTIL_BOARD_ID;
			break;
		default:
			return FALSE;
	}
	return(id);
}

/**
 * Routine to convert a address type character, read from a '_DATA P 4000' type statement in a .lod file,
 * into a memory space enum value, suitable for passing to a PCI write memory command.
 * @param ch The address type character.
 * @param mem_space The address of a variable to store the converted memory space.
 * @return Returns TRUE if the conversion succeeded, FALSE if it failed.
 */
static int DSP_Download_Address_Char_To_Mem_Space(char ch,enum CCD_DSP_MEM_SPACE *mem_space)
{
	switch(ch)
	{
		case 'R':
			(*mem_space) = CCD_DSP_MEM_SPACE_R;
			break;
		case 'P':
			(*mem_space) = CCD_DSP_MEM_SPACE_P;
			break;
		case 'X':
			(*mem_space) = CCD_DSP_MEM_SPACE_X;
			break;
		case 'Y':
			(*mem_space) = CCD_DSP_MEM_SPACE_Y;
			break;
		default:
			(*mem_space) = 0;
			DSP_Download_Error_Number = 27;
			sprintf(DSP_Download_Error_String,
				"DSP_Download_Address_Char_To_Mem_Space:Illegal value '%c'.",ch);
			break;
	}
	return ((*mem_space) != 0);
}

/**
 * This routine reads DSP program code from file download_fp and writes it to DSP memory
 * using the <a href="#CCD_DSP_WRM">WRM</a> command to board board_id, memory space type address addr
 * @param handle The address of a CCD_Interface_Handle_T that holds the device connection specific information.
 * @param download_fp The file pointer of the .lod file we are loading the program from.
 * @param board_id The board to send the command to. One of
 * 	CCD_DSP_TIM_BOARD_ID(timing board) or CCD_DSP_UTIL_BOARD_ID(utility board).
 * @param mem_space The memory space to put the data into, of type 
 * 	<a href="#CCD_DSP_MEM_SPACE">CCD_DSP_MEM_SPACE</a>. One of:
 * 	CCD_DSP_MEM_SPACE_P(program),
 * 	CCD_DSP_MEM_SPACE_X(X data),
 * 	CCD_DSP_MEM_SPACE_Y(Y data)
 * 	or CCD_DSP_MEM_SPACE_R(ROM).
 * @param addr The address within the memory space.
 * @return Returns TRUE if the operation succeeds, FALSE if it fails.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
static int DSP_Download_Process_Data(CCD_Interface_Handle_T* handle,FILE *download_fp,enum CCD_DSP_BOARD_ID board_id,
	enum CCD_DSP_MEM_SPACE mem_space,int addr)
{ 
	int finished, value;
	char c;

	finished = FALSE;
	/* while theres data to download and we've not aborted the operation */
	while ((!finished) && (!CCD_DSP_Get_Abort()))
	{
		/* ignore spaces */
		while ((c = getc(download_fp)) == 32);
		/* if we get an underscore it's probably the start of an _END or _DATA - hence stop */
		if(c == '_')
		{
			ungetc(c, download_fp);
			finished = TRUE;
		}
		/* it it's not a newline it must be actual data */
		else if((c != 10)&&(c != 13))
		{
			/* put the byte back */
			ungetc(c, download_fp);
			/* read the whole word of hexadecimal data */
			fscanf(download_fp, "%x", (unsigned int *)&value);
			/* try writing it to a board */
			if(!CCD_DSP_Command_WRM(handle,board_id,mem_space,addr,value))
			{
				DSP_Download_Error_Number = 28;
				sprintf(DSP_Download_Error_String,
					"DSP_Download_Process_Data:Failed to WRM(%#x,%#x,%#x,%#x).",
					board_id,mem_space,addr,value);
				return FALSE;
			}
			addr++;
		}
	}
	return(TRUE);
}

/*
** $Log: not supported by cvs2svn $
** Revision 1.6  2009/02/05 11:40:27  cjm
** Swapped Bitwise for Absolute logging levels.
**
** Revision 1.5  2008/11/20 11:34:46  cjm
** *** empty log message ***
**
** Revision 1.4  2006/05/17 17:58:56  cjm
** Fixed %x requires (unsigned int *) warnings.
** Fixed mismatched number of parameters in fprintf.
**
** Revision 1.3  2006/05/16 14:14:01  cjm
** gnuify: Added GNU General Public License.
**
** Revision 1.2  2002/12/16 16:49:36  cjm
** Removed Error routines resetting error number to zero.
**
** Revision 1.1  2002/11/07 19:13:39  cjm
** Initial revision
**
*/
