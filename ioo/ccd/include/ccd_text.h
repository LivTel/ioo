/* ccd_text.h
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/include/ccd_text.h,v 1.1 2011-11-23 11:02:52 cjm Exp $
*/

#ifndef CCD_TEXT_H
#define CCD_TEXT_H

#include <sys/types.h>

/* These enum definitions should match with those in CCDLibrary.java */
/**
 * Level number passed to CCD_Text_Set_Print_Level to set the amount of information to print out. One of:
 * <ul>
 * <li>CCD_TEXT_PRINT_LEVEL_COMMANDS - Print out commands only.
 * <li>CCD_TEXT_PRINT_LEVEL_REPLIES - Print out commands' replies as well.
 * <li>CCD_TEXT_PRINT_LEVEL_VALUES - Print out parameter values as well.
 * <li>CCD_TEXT_PRINT_LEVEL_ALL - Print out everything.
 * </ul>
 * @see #CCD_Text_Set_Print_Level
 */
enum CCD_TEXT_PRINT_LEVEL
{
	CCD_TEXT_PRINT_LEVEL_COMMANDS=0,CCD_TEXT_PRINT_LEVEL_REPLIES=1,CCD_TEXT_PRINT_LEVEL_VALUES=2,
	CCD_TEXT_PRINT_LEVEL_ALL=3
};

/**
 * Macro to check whether the level is a legal level of print level.
 */
#define CCD_TEXT_IS_TEXT_PRINT_LEVEL(level)	(((level) == CCD_TEXT_PRINT_LEVEL_COMMANDS)|| \
	((level) == CCD_TEXT_PRINT_LEVEL_REPLIES)||((level) == CCD_TEXT_PRINT_LEVEL_VALUES)|| \
	((level) == CCD_TEXT_PRINT_LEVEL_ALL))

/**
 * Typedef for the text handle pointer, which is an instance of CCD_Text_Handle_Struct.
 * @see #CCD_Text_Handle_Struct
 */
typedef struct CCD_Text_Handle_Struct CCD_Text_Handle_T;


/* configuration of this device interface */
extern void CCD_Text_Set_Print_Level(enum CCD_TEXT_PRINT_LEVEL level);

/* implementation of device interface */
extern void CCD_Text_Initialise(void);
extern int CCD_Text_Open(char *device_pathname,CCD_Interface_Handle_T *handle);
extern int CCD_Text_Memory_Map(CCD_Interface_Handle_T *handle,int buffer_size);
extern int CCD_Text_Memory_UnMap(CCD_Interface_Handle_T *handle);
extern int CCD_Text_Command(CCD_Interface_Handle_T *handle,int request,int *argument);
extern int CCD_Text_Command_List(CCD_Interface_Handle_T *handle,int request,int *argument_list,int argument_count);
extern int CCD_Text_Get_Reply_Data(CCD_Interface_Handle_T *handle,unsigned short **data);
extern int CCD_Text_Close(CCD_Interface_Handle_T *handle);
extern int CCD_Text_Get_Error_Number(void);
extern void CCD_Text_Error(void);
extern void CCD_Text_Error_String(char *error_string);
extern void CCD_Text_Warning(void);

#endif
