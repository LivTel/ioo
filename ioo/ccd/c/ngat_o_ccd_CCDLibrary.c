/* ngat_o_ccd_CCDLibrary.c
** implementation of Java Class ngat.o.ccd.CCDLibrary native interfaces
** $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/ngat_o_ccd_CCDLibrary.c,v 1.1 2011-11-23 10:59:52 cjm Exp $
*/
/**
 * ngat_o_ccd_CCDLibrary.c is the 'glue' between libo_ccd, the C library version of the SDSU CCD Controller
 * software, and CCDLibrary.java, a Java Class to drive the controller. CCDLibrary specifically
 * contains all the native C routines corresponding to native methods in Java.
 * @author Chris Mottram LJMU
 * @version $Revision: 1.1 $
 */
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for time.
 */
#define _POSIX_SOURCE 1
/**
 * This hash define is needed before including source files give us POSIX.4/IEEE1003.1b-1993 prototypes
 * for time.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <time.h>
#include "ccd_global.h"
#include "ccd_dsp.h"
#include "ccd_exposure.h"
#include "ccd_filter_wheel.h"
#include "ccd_interface.h"
#include "ccd_pci.h"
#include "ccd_setup.h"
#include "ccd_temperature.h"
#include "ccd_text.h"
#include "ngat_o_ccd_CCDLibrary.h"

/* hash definitions */
/**
 * Constant value for the size of buffer to use when getting the libo_ccd error string using 
 * CCD_Global_Error_String.
 * @see ccd_global.html#CCD_Global_Error_String
 * @see ccd_global.html#CCD_GLOBAL_ERROR_STRING_LENGTH
 */
#define CCD_ERROR_LENGTH	1024

/**
 * Hash define for the size of the array holding CCDLibrary Instance (jobject) maps to CCD_Interface_Handle_T.
 * Set to 5.
 */
#define HANDLE_MAP_SIZE         (5)

/* internal structures */
/**
 * Structure holding mapping between CCDLibrary Instances (jobject) to CCD_Interface_Handle_T.
 * This means each CCDLibrary object talks to one SDSU controller.
 * <dl>
 * <dt>CCDLibrary_Instance_Handle</dt> <dd>jobject reference for the CCDLibrary instance.</dd>
 * <dt>Interface_Handle</dt> <dd>Pointer to the CCD_Interface_Handle_T for that CCDLibrary instance.</dd>
 * </dl>
 */
struct Handle_Map_Struct
{
	jobject CCDLibrary_Instance_Handle;
	CCD_Interface_Handle_T* Interface_Handle;
};

/* internal variables */
/**
 * Revision Control System identifier.
 */
static char rcsid[] = "$Id: ngat_o_ccd_CCDLibrary.c,v 1.1 2011-11-23 10:59:52 cjm Exp $";

/**
 * Copy of the java virtual machine pointer, used for logging back up to the Java layer from C.
 */
static JavaVM *java_vm = NULL;
/**
 * Cached global reference to the "ngat.ccd.CCDLibrary" logger, used to log back to the Java layer from
 * C routines.
 */
static jobject logger = NULL;
/**
 * Cached reference to the "ngat.util.logging.Logger" class's log(int level,String message) method.
 * Used to log C layer log messages, in conjunction with the logger's object reference logger.
 * @see #logger
 */
static jmethodID log_method_id = NULL;
/**
 * Internal list of maps between CCDLibrary jobject's (i.e. CCDLibrary references), and
 * CCD_Interface_Handle_T handles (which control which /dev/astropci port we talk to).
 * @see #Handle_Map_Struct
 * @see #HANDLE_MAP_SIZE
 */
static struct Handle_Map_Struct Handle_Map_List[HANDLE_MAP_SIZE] = 
{
	{NULL,NULL},
	{NULL,NULL},
	{NULL,NULL},
	{NULL,NULL},
	{NULL,NULL}
};

/* internal routines */
static void CCDLibrary_Throw_Exception(JNIEnv *env,jobject obj,char *function_name);
static void CCDLibrary_Throw_Exception_String(JNIEnv *env,jobject obj,char *function_name,char *error_string);
static void CCDLibrary_Log_Handler(int level,char *string);
static int CCDLibrary_Java_String_List_To_C_List(JNIEnv *env,jobject obj,jobject java_list,
						 jstring **jni_jstring_list,int *jni_jstring_count,
						 char ***c_list,int *c_list_count);
static int CCDLibrary_Java_String_List_Free(JNIEnv *env,jobject obj,
					    jstring *jni_jstring_list,int jni_jstring_count,
					    char **c_list,int c_list_count);
static int CCDLibrary_Handle_Map_Add(JNIEnv *env,jobject instance,CCD_Interface_Handle_T* interface_handle);
static int CCDLibrary_Handle_Map_Delete(JNIEnv *env,jobject instance);
static int CCDLibrary_Handle_Map_Find(JNIEnv *env,jobject instance,CCD_Interface_Handle_T** interface_handle);

/* ------------------------------------------------------------------------------
** 		External routines
** ------------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------------
** 		CCDLibrary C layer initialisation
** ------------------------------------------------------------------------------ */
/**
 * This routine gets called when the native library is loaded. We use this routine
 * to get a copy of the JavaVM pointer of the JVM we are running in. This is used to
 * get the correct per-thread JNIEnv context pointer in CCDLibrary_Log_Handler.
 * @see #java_vm
 * @see #CCDLibrary_Log_Handler
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	java_vm = vm;
	return JNI_VERSION_1_2;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    initialiseLoggerReference<br>
 * Signature: (Lngat/util/logging/Logger;)V<br>
 * Java Native Interface implementation CCDLibrary's initialiseLoggerReference.
 * This takes the supplied logger object reference and stores it in the logger variable as a global reference.
 * The log method ID is also retrieved and stored.
 * The libo_ccd's log handler is set to the JNI routine CCDLibrary_Log_Handler.
 * The libo_ccd's log filter function is set absolute.
 * @param l The CCDLibrary's "ngat.o.ccd.CCDLibrary" logger.
 * @see #CCDLibrary_Log_Handler
 * @see #logger
 * @see #log_method_id
 * @see ccd_global.html#CCD_Global_Log_Filter_Level_Absolute
 * @see ccd_global.html#CCD_Global_Set_Log_Handler_Function
 * @see ccd_global.html#CCD_Global_Set_Log_Filter_Function
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_initialiseLoggerReference(JNIEnv *env,jobject obj,jobject l)
{
	jclass cls = NULL;

/* save logger instance */
	logger = (*env)->NewGlobalRef(env,l);
/* get the ngat.util.logging.Logger class */
	cls = (*env)->FindClass(env,"ngat/util/logging/Logger");
	/* if the class is null, one of the following exceptions occured:
	** ClassFormatError,ClassCircularityError,NoClassDefFoundError,OutOfMemoryError */
	if(cls == NULL)
		return;
/* get relevant method id to call */
/* log(int level,java/lang/String message) */
	log_method_id = (*env)->GetMethodID(env,cls,"log","(ILjava/lang/String;)V");
	if(log_method_id == NULL)
	{
		/* One of the following exceptions has been thrown:
		** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
		return;
	}
	/* Make the C layer log back to the Java logger, using CCDLibrary_Log_Handler JNI routine. */
	CCD_Global_Set_Log_Handler_Function(CCDLibrary_Log_Handler);
	/* Make the filtering absolute, as expected by the C layer */
	CCD_Global_Set_Log_Filter_Function(CCD_Global_Log_Filter_Level_Absolute);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    finaliseLoggerReference<br>
 * Signature: ()V<br>
 * This native method is called from CCDLibrary's finaliser method. It removes the global reference to
 * logger.
 * @see #logger
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_finaliseLoggerReference(JNIEnv *env, jobject obj)
{
	(*env)->DeleteGlobalRef(env,logger);
}

/* ------------------------------------------------------------------------------
** 		ccd_dsp.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_DSP_Command_RET<br>
 * Signature: ()I<br>
 * Java Native Interface routine to read the length of time an exposure is underway.
 * This is done by calling CCD_DSP_Command_RET.
 * If an error occurs a CCDLibraryNativeException is thrown.
 * If an exposure is not underway (the shutter is closed) zero is returned.
 * @see ccd_dsp.html#CCD_DSP_Command_RET
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1DSP_1Command_1RET(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* real elapsed time */
	retval = CCD_DSP_Command_RET(handle);
	if((retval == 0)&&(CCD_DSP_Get_Error_Number()))
		CCDLibrary_Throw_Exception(env,obj,"CCD_DSP_Command_Read_Exposure_Time");
	return retval;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_DSP_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for the ccd_dsp part of the library.
 * @return The current error number of ccd_dsp. A zero error number means an error has not occured.
 * @see ccd_dsp.html#CCD_DSP_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1DSP_1Get_1Error_1Number(JNIEnv *env, jobject obj)
{
	return CCD_DSP_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		ccd_exposure.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Expose<br>
 * Signature: (ZJILjava/util/List;)V<br>
 * Java Native Interface routine to do an exposure.
 * @see ccd_exposure.html#CCD_Exposure_Expose
 * @see #CCDLibrary_Throw_Exception
 * @see #CCDLibrary_Java_String_List_To_C_List
 * @see #CCDLibrary_Java_String_List_Free
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Expose(JNIEnv *env,jobject obj,jboolean open_shutter,
			     jlong start_time_long,jint exposure_length,jobject filename_list_object)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval,jni_filename_count,filename_count;
	struct timespec start_time;
	jstring *jni_filename_list = NULL;
	char **filename_list = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* convert filenameList to filename_list (Java to C) */
	retval = CCDLibrary_Java_String_List_To_C_List(env,obj,filename_list_object,
						 &jni_filename_list,&jni_filename_count,
						 &filename_list,&filename_count);
	if(retval == FALSE)
		return; /* CCDLibrary_Java_String_List_To_C_List throws exception */
	/* convert start_time_long to start_time */
	if(start_time_long > -1)
	{
		start_time.tv_sec = (time_t)(start_time_long/((jlong)1000L));
		start_time.tv_nsec = (long)((start_time_long%((jlong)1000L))*1000000L);
	}
	else
	{
		start_time.tv_sec = 0;
		start_time.tv_nsec = 0;
	}
	/* do exposure */
	retval = CCD_Exposure_Expose(handle,TRUE,open_shutter,start_time,exposure_length,filename_list,filename_count);
	/* Free the generated C filename_list, and associated jstring list */
	CCDLibrary_Java_String_List_Free(env,obj,jni_filename_list,jni_filename_count,
					    filename_list,filename_count);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Exposure_Expose");
	}
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Bias<br>
 * Signature: (Ljava/lang/String;)V<br>
 * Java Native Interface routine to take a bias frame.
 * @see ccd_exposure.html#CCD_Exposure_Bias
 * @see #CCDLibrary_Throw_Exception
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Bias(JNIEnv *env,jobject obj,jstring filename)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval;
	const char *cfilename = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* Change the java strings to a c null terminated string
	** If the java String is null the C string should be null as well */
	if(filename != NULL)
		cfilename = (*env)->GetStringUTFChars(env,filename,0);
	/* do bias */
	retval = CCD_Exposure_Bias(handle,(char*)cfilename);
	/* If we created the C strings we need to free the memory it uses */
	if(filename != NULL)
		(*env)->ReleaseStringUTFChars(env,filename,cfilename);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Exposure_Bias");

}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Abort<br>
 * Signature: ()V<br>
 * Java Native Interface implementation to abort an exposure. 
 * @see ccd_exposure.html#CCD_Exposure_Abort
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Abort(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* abort exposure */
	retval = CCD_Exposure_Abort(handle);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Exposure_Abort");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Get_Exposure_Status<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the current status of an exposure.
 * @return The current status of exposure, whether the ccd is not exposing,exposing or reading out.
 * @see ccd_exposure.html#CCD_Exposure_Get_Exposure_Status
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Get_1Exposure_1Status(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return CCD_Exposure_Get_Exposure_Status(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Get_Exposure_Length<br>
 * Signature: ()I<br>
 * Java native method to return the exposure length.
 * @return The exposure length in milliseconds.
 * @see ccd_exposure.html#CCD_Exposure_Get_Exposure_Length
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Get_1Exposure_1Length(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return CCD_Exposure_Get_Exposure_Length(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Get_Exposure_Start_Time<br>
 * Signature: ()J<br>
 * Java Native method to get the exposure start time.
 * @return A long, the time in milliseconds the exposure was started, since the EPOCH.
 * @see ccd_exposure.html#CCD_Exposure_Get_Exposure_Start_Time
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jlong JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Get_1Exposure_1Start_1Time(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;
	struct timespec start_time;
	jlong retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	start_time = CCD_Exposure_Get_Exposure_Start_Time(handle);
	retval = ((jlong)start_time.tv_sec)*((jlong)1000L);
	retval += ((jlong)start_time.tv_nsec)/((jlong)1000000L);
	return retval;
}
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Shutter_Trigger_Delay_Set<br>
 * Signature: (I)V<br>
 * @see ccd_exposure.html#CCD_Exposure_Shutter_Trigger_Delay_Set
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Shutter_1Trigger_1Delay_1Set(JNIEnv *env, 
									      jobject obj, jint delay_ms)
{
	CCD_Exposure_Shutter_Trigger_Delay_Set((int)delay_ms);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Shutter_Trigger_Delay_Get<br>
 * Signature: ()I<br>
 * @see ccd_exposure.html#CCD_Exposure_Shutter_Trigger_Delay_Get
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Shutter_1Trigger_1Delay_1Get(JNIEnv *env, jobject obj)
{
	return (jint)CCD_Exposure_Shutter_Trigger_Delay_Get();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Shutter_Open_Delay_Set<br>
 * Signature: (I)V<br>
 * @see ccd_exposure.html#CCD_Exposure_Shutter_Open_Delay_Set
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Shutter_1Open_1Delay_1Set(JNIEnv *env, jobject obj,
											   jint delay_ms)
{
	CCD_Exposure_Shutter_Open_Delay_Set((int)delay_ms);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Shutter_Open_Delay_Get<br>
 * Signature: ()I<br>
 * @see ccd_exposure.html#CCD_Exposure_Shutter_Open_Delay_Get
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Shutter_1Open_1Delay_1Get(JNIEnv *env, jobject obj)
{
	return (jint)CCD_Exposure_Shutter_Open_Delay_Get();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Shutter_Close_Delay_Set<br>
 * Signature: (I)V<br>
 * @see ccd_exposure.html#CCD_Exposure_Shutter_Close_Delay_Set
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Shutter_1Close_1Delay_1Set(JNIEnv *env, jobject obj, 
											    jint delay_ms)
{
	CCD_Exposure_Shutter_Close_Delay_Set((int)delay_ms);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Shutter_Close_Delay_Get<br>
 * Signature: ()I<br>
 * @see ccd_exposure.html#CCD_Exposure_Shutter_Close_Delay_Get
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Shutter_1Close_1Delay_1Get(JNIEnv *env, jobject obj)
{
	return (jint)CCD_Exposure_Shutter_Close_Delay_Get();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Readout_Delay_Set<br>
 * Signature: (I)V<br>
 * @see ccd_exposure.html#CCD_Exposure_Readout_Delay_Set
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Readout_1Delay_1Set(JNIEnv *env, jobject obj, 
										     jint delay_ms)
{
	CCD_Exposure_Readout_Delay_Set((int)delay_ms);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Readout_Delay_Get<br>
 * Signature: ()I<br>
 * @see ccd_exposure.html#CCD_Exposure_Readout_Delay_Get
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Readout_1Delay_1Get(JNIEnv *env, jobject obj)
{
	return (jint)CCD_Exposure_Readout_Delay_Get();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Exposure_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for the ccd_exposure part of the library.
 * @return The current error number of ccd_exposure. A zero error number means an error has not occured.
 * @see ccd_exposure.html#CCD_Exposure_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Exposure_1Get_1Error_1Number(JNIEnv *env,jobject obj)
{
	return CCD_Exposure_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		ccd_filter_wheel.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Position_Count_Set<br>
 * Signature: (I)V<br>
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Set_Position_Count
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Position_1Count_1Set(JNIEnv *env,jobject obj,
											   jint position_count)
{
	CCD_Filter_Wheel_Position_Count_Set((int)position_count);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Reset<br>
 * Signature: ()V<br>
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Reset
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Reset(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	retval = CCD_Filter_Wheel_Reset(handle);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Filter_Wheel_Reset");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Move<br>
 * Signature: (I)V<br>
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Move
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Move(JNIEnv *env,jobject obj,jint position)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	retval = CCD_Filter_Wheel_Move(handle,(int)position);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Filter_Wheel_Move");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Abort<br>
 * Signature: ()V<br>
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Abort
 * @see ccd_interface.html#CCD_Interface_Handle_T
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Abort(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T* handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	retval = CCD_Filter_Wheel_Abort(handle);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Filter_Wheel_Abort");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Get_Error_Number<br>
 * Signature: ()I<br>
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Get_1Error_1Number(JNIEnv *env, jobject obj)
{
	return (jint)CCD_Filter_Wheel_Get_Error_Number();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Get_Status<br>
 * Signature: ()I<br>
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Get_Status
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Get_1Status(JNIEnv *env, jobject obj)
{
	return (jint)CCD_Filter_Wheel_Get_Status();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Filter_Wheel_Get_Position<br>
 * Signature: ()I<br>
 * @see ccd_filter_wheel.html#CCD_Filter_Wheel_Get_Position
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Filter_1Wheel_1Get_1Position(JNIEnv *env, jobject obj)
{
	int position;

	if(!CCD_Filter_Wheel_Get_Position(&position))
		return -1;
	return position;
}

/* ------------------------------------------------------------------------------
** 		ccd_global.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Global_Initialise<br>
 * Signature: ()V<br>
 * Java Native Interface implementation of <a href="ccd_global.html#CCD_Global_Initialise">CCD_Global_Initialise</a>,
 * which calls initialisation routines for various parts of the library.
 * It also sets the log handler to CCDLibrary_Log_Handler by calling CCD_Global_Set_Log_Handler_Function. This means
 * all logging messages get sent to the CCDLibrary's logger using a JNI call.
 * @see ccd_global.html#CCD_Global_Initialise
 * @see ccd_global.html#CCD_Global_Set_Log_Handler_Function
 * @see #CCDLibrary_Log_Handler
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Global_1Initialise(JNIEnv *env, jobject obj)
{
	CCD_Global_Initialise();
/* print some compile time information to stdout */
	fprintf(stdout,"ngat.frodospec.ccd.CCDLibrary.CCD_Global_Initialise:%s.\n",rcsid);
	CCD_Global_Set_Log_Handler_Function(CCDLibrary_Log_Handler);
	fflush(stdout);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Global_Set_Log_Filter_Level<br>
 * Signature: (I)V<br>
 * JNI interface to set the log level.
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Global_1Set_1Log_1Filter_1Level(JNIEnv *env, jobject obj, 
										       jint level)
{
	CCD_Global_Set_Log_Filter_Level(level);
}

/* ------------------------------------------------------------------------------
** 		CCD_Interface routines
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Interface_Open<br>
 * Signature: (ILjava/lang/String;)V<br>
 * Java Native Interface implementation of <a href="ccd_interface.html#CCD_Interface_Open">CCD_Interface_Open</a>,
 * which opens the specified interface.
 * If an error occurs a CCDLibraryNativeException is thrown.
 * CCDLibrary_Handle_Map_Add is used to add a mapping from the CCDLIbrary instance (obj) to the opened handle.
 * @see ccd_global.html#CCD_Global_Initialise
 * @see ccd_interface.html#CCD_Interface_Open
 * @see #CCDLibrary_Handle_Map_Add
 * @see #CCDLibrary_Throw_Exception
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Interface_1Open(JNIEnv *env,jobject obj,jint device_number, 
								       jstring filename)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;
	const char *cfilename = NULL;

	/* Change the java strings to a c null terminated string
	** If the java String is null the C string should be null as well */
	if(filename != NULL)
		cfilename = (*env)->GetStringUTFChars(env,filename,0);
	/* create handle to interface */
	retval = CCD_Interface_Open((enum CCD_INTERFACE_DEVICE_ID)device_number,(char*)cfilename,&handle);
	if(filename != NULL)
		(*env)->ReleaseStringUTFChars(env,filename,cfilename);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Interface_Open");
		return;
	}
	/* map this (obj) to handle */
	retval = CCDLibrary_Handle_Map_Add(env,obj,handle);
	if(retval == FALSE)
	{
		/* An error should have been thrown by CCDLibrary_Handle_Map_Add */
		return;
	}
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Interface_Memory_Map<br>
 * Signature: ()V<br>
 * JNI interface to create a mmap memory map to the SDSU controller.
 * This is normally done internally to to CCD_Setup_Startup, but is exposed in the Java interface for command line
 * Java programs that don't call the setup mathod, but still want to readout the CCD.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_interface.html#CCD_Interface_Memory_Map
 * @see ccd_setup.html#CCD_SETUP_MEMORY_BUFFER_SIZE
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Interface_1Memory_1Map(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* create mmap */
	retval = CCD_Interface_Memory_Map(handle,CCD_SETUP_MEMORY_BUFFER_SIZE);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Interface_Memory_Map");
		return;
	}
}


/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Interface_Close<br>
 * Signature: ()V<br>
 * JNI interface to close a connection to the controller.
 * The library handle map entry is also released using CCDLibrary_Handle_Map_Delete.
 * @see ccd_interface.html#CCD_Interface_Close
 * @see #CCDLibrary_Throw_Exception
 * @see #CCDLibrary_Handle_Map_Delete
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Interface_1Close(JNIEnv *env, jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* close handle */
	retval = CCD_Interface_Close(&handle);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Interface_Close");
		return;
	}
	/* remove mapping from CCDLibrary instance to interface handle */
	retval = CCDLibrary_Handle_Map_Delete(env,obj);
	if(retval == FALSE)
	{
		/* CCDLibrary_Handle_Map_Delete should have thrown an error if it fails */
		return;
	}
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Interface_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for this module.
 * @return The current value of the error number for this module. A zero error number means an error has not occured.
 * @see ccd_interface.html#CCD_Interface_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Interface_1Get_1Error_1Number(JNIEnv *env,jobject obj)
{
	return CCD_Interface_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		ccd_pci.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_PCI_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for this module.
 * @return The current value of the error number for this module. A zero error number means an error has not occured.
 * @see ccd_interface.html#CCD_PCI_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1PCI_1Get_1Error_1Number(JNIEnv *env,jobject obj)
{
	return CCD_PCI_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		ccd_setup.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Startup<br>
 * Signature: (ILjava/lang/String;IILjava/lang/String;IILjava/lang/String;DIZZ)V<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Startup">CCD_Setup_Startup</a>,
 * a routine to setup the SDSU CCD Controller for exposures. This routine translates the pci_filename_string, 
 * timing_filename_string and utility_filename_string parameters from Java Strings to C Strings.
 * If an error occurs a CCDLibraryNativeException is thrown.
 * @see ccd_setup.html#CCD_Setup_Startup
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Startup(JNIEnv *env,jobject obj,
			   jint pci_load_type,jstring pci_filename_string,
			   jint timing_load_type,jint timing_application_number,jstring timing_filename_string,
			   jint utility_load_type,jint utility_application_number,jstring utility_filename_string,
			   jdouble target_temperature,jint gain,jboolean gain_speed,jboolean idle)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;
	const char *pci_filename = NULL;
	const char *timing_filename = NULL;
	const char *utility_filename = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* Change the java strings to a c null terminated string
	** If the java String is null the C string should be null as well */
	if(pci_filename_string != NULL)
		pci_filename = (*env)->GetStringUTFChars(env,pci_filename_string,0);
	if(timing_filename_string != NULL)
		timing_filename = (*env)->GetStringUTFChars(env,timing_filename_string,0);
	if(utility_filename_string != NULL)
		utility_filename = (*env)->GetStringUTFChars(env,utility_filename_string,0);
	/* do setup */
	retval = CCD_Setup_Startup(handle,pci_load_type,(char*)pci_filename,
		timing_load_type,timing_application_number,(char*)timing_filename,
		utility_load_type,utility_application_number,(char*)utility_filename,
		target_temperature,gain,gain_speed,idle);
	/* If we created the C strings we need to free the memory it uses */
	if(pci_filename_string != NULL)
		(*env)->ReleaseStringUTFChars(env,pci_filename_string,pci_filename);
	if(timing_filename_string != NULL)
		(*env)->ReleaseStringUTFChars(env,timing_filename_string,timing_filename);
	if(utility_filename_string != NULL)
		(*env)->ReleaseStringUTFChars(env,utility_filename_string,utility_filename);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Startup");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Shutdown<br>
 * Signature: ()V<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Shutdown">CCD_Setup_Shutdown</a>,
 * a routine to shutdown the connection to a SDSU CCD Controller.
 * If an error occurs a CCDLibraryNativeException is thrown.
 * @see ccd_setup.html#CCD_Setup_Shutdown
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Shutdown(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* shutdown */
	retval = CCD_Setup_Shutdown(handle);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Shutdown");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Dimensions<br>
 * Signature: (IIIIIII[Lngat/o/ccd/CCDLibrarySetupWindow;)V<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Dimensions">CCD_Setup_Dimensions</a>,
 * a routine to setup the SDSU CCD Controller dimensions.
 * If an error occurs a CCDLibraryNativeException is thrown.
 * If the class cannot be found one of the following exception is thrown: 
 * ClassFormatError, ClassCircularityError, NoClassDefFoundError, OutOfMemoryError.
 * @see ccd_setup.html#CCD_Setup_Dimensions
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Dimensions(JNIEnv *env,jobject obj,
				 jint ncols,jint nrows,jint nsbin,jint npbin,jint amplifier, jint deinterlace_setting,
				 jint window_flags,jobjectArray window_object_list)
{
	CCD_Interface_Handle_T *handle = NULL;
	struct CCD_Setup_Window_Struct window_list[CCD_SETUP_WINDOW_COUNT];
	char error_string[256];
	int retval,i;
	jclass cls = NULL;
	jobject window_object = NULL;
	jmethodID get_x_start_method_id,get_y_start_method_id,get_x_end_method_id,get_y_end_method_id;
	jsize window_count;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
/* convert window_object_list to window_list */
	/* check array is not NULL */
	if(window_object_list == NULL)
	{
		/* N.B. This error occured in the JNI interface, not the libfrodospec_ccd - no error string set */
		sprintf(error_string,"CCD_Setup_Dimensions:window list was NULL.");
		CCDLibrary_Throw_Exception_String(env,obj,"CCD_Setup_Dimensions",error_string);
		return;
	}
	/* check size of array */
	window_count = (*env)->GetArrayLength(env,(jarray)window_object_list);
	if(window_count != CCD_SETUP_WINDOW_COUNT)
	{
		/* N.B. This error occured in the JNI interface, not the libfrodospec_ccd - no error string set */
		sprintf(error_string,"CCD_Setup_Dimensions:window list has wrong number of elements(%d,%d).",
			window_count,CCD_SETUP_WINDOW_COUNT);
		CCDLibrary_Throw_Exception_String(env,obj,"CCD_Setup_Dimensions",error_string);
		return;
	}
/* get the class of CCDLibrarySetupWindow */
	cls = (*env)->FindClass(env,"ngat/o/ccd/CCDLibrarySetupWindow");
	/* if the class is null, one of the following exceptions occured:
	** ClassFormatError,ClassCircularityError,NoClassDefFoundError,OutOfMemoryError */
	if(cls == NULL)
		return;
/* get relevant method ids to call */
/* getXStart */
	get_x_start_method_id = (*env)->GetMethodID(env,cls,"getXStart","()I");
	if(get_x_start_method_id == NULL)
	{
		/* One of the following exceptions has been thrown:
		** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
		return;
	}
/* getYStart */
	get_y_start_method_id = (*env)->GetMethodID(env,cls,"getYStart","()I");
	if(get_y_start_method_id == 0)
	{
		/* One of the following exceptions has been thrown:
		** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
		return;
	}
/* getXEnd */
	get_x_end_method_id = (*env)->GetMethodID(env,cls,"getXEnd","()I");
	if(get_x_end_method_id == 0)
	{
		/* One of the following exceptions has been thrown:
		** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
		return;
	}
/* getYEnd */
	get_y_end_method_id = (*env)->GetMethodID(env,cls,"getYEnd","()I");
	if(get_y_end_method_id == 0)
	{
		/* One of the following exceptions has been thrown:
		** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
		return;
	}
/* for each window, get each position from the window_object_list into the window_list */
	for(i=0;i<CCD_SETUP_WINDOW_COUNT;i++)
	{
		/* Get the object at index i into window_object
		** throws ArrayIndexOutOfBoundsException: if index does not specify a valid index in the array. 
		** However, this should never occur, we have checked the size of the array earlier. */
		window_object = (*env)->GetObjectArrayElement(env,window_object_list,i);

		/* call the get method and put it into the list of window structures. */
		window_list[i].X_Start = (int)((*env)->CallIntMethod(env,window_object,get_x_start_method_id));
		window_list[i].Y_Start = (int)((*env)->CallIntMethod(env,window_object,get_y_start_method_id));
		window_list[i].X_End = (int)((*env)->CallIntMethod(env,window_object,get_x_end_method_id));
		window_list[i].Y_End = (int)((*env)->CallIntMethod(env,window_object,get_y_end_method_id));
	}
/* call dimension setup routine */
	retval = CCD_Setup_Dimensions(handle,ncols,nrows,nsbin,npbin,amplifier,deinterlace_setting,
				      window_flags,window_list);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Dimensions");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Hardware_Test<br>
 * Signature: (I)V<br>
 * Java Native Interface implementation of 
 * <a href="ccd_setup.html#CCD_Setup_Hardware_Test">CCD_Setup_Hardware_Test</a>,
 * which tests the hardware data links to the controller boards.
 * @see ccd_setup.html#CCD_Setup_Hardware_Test
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 * @see #CCDLibrary_Throw_Exception
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Hardware_1Test(JNIEnv *env,jobject obj,jint test_count)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* hardware test */
	retval = CCD_Setup_Hardware_Test(handle,test_count);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Hardware_Test");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Abort<br>
 * Signature: ()V<br>
 * Abort a setup underway.
 * @see ccd_setup.html#CCD_Setup_Abort
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Abort(JNIEnv *env,jobject obj)
{
	/* abort setup */
	CCD_Setup_Abort();
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_NCols<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Get_NCols">CCD_Setup_Get_NCols</a>,
 * which gets the number of columns in the CCD chip.
 * @see ccd_setup.html#CCD_Setup_Get_NCols
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1NCols(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_NCols(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_NRows<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Get_NRows">CCD_Setup_Get_NRows</a>,
 * which gets the number of rows in the CCD chip.
 * @see ccd_setup.html#CCD_Setup_Get_NRows
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1NRows(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_NRows(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_NSBin<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Get_NSBin">CCD_Setup_Get_NSBin</a>,
 * which gets the serial/column/X binning.
 * @see ccd_setup.html#CCD_Setup_Get_NSBin
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1NSBin(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_NSBin(handle);
}


/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_NPBin<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Get_NPBin">CCD_Setup_Get_NPBin</a>,
 * which gets the parallel/row/Y binning.
 * @see ccd_setup.html#CCD_Setup_Get_NPBin
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1NPBin(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_NPBin(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Amplifier<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of 
 * <a href="ccd_setup.html#CCD_Setup_Get_Amplifier">CCD_Setup_Get_Amplifier</a>,
 * which gets the amplifier.
 * @see ccd_setup.html#CCD_Setup_Get_Amplifier
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Amplifier(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_Amplifier(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_DeInterlace_Type<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of 
 * <a href="ccd_setup.html#CCD_Setup_Get_DeInterlace_Type">CCD_Setup_Get_DeInterlace_Type</a>,
 * which gets the de-interlace type.
 * @see ccd_setup.html#CCD_Setup_Get_DeInterlace_Type
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1DeInterlace_1Type(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_DeInterlace_Type(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Setup_Complete<br>
 * Signature: ()Z<br>
 * Java Native Interface routine to get whether a setup operation has been completed since the last reset.
 * The setup is complete when the DSP application have been loaded, the power is on and the dimension information has
 * been set.
 * @return True if the controller is setup for exposures, false if it is not.
 * @see ccd_setup.html#CCD_Setup_Get_Setup_Complete
 * @see ccd_setup.html#CCD_Setup_Startup
 * @see ccd_setup.html#CCD_Setup_Dimensions
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jboolean JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Setup_1Complete(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jboolean)CCD_Setup_Get_Setup_Complete(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Setup_In_Progress<br>
 * Signature: ()Z<br>
 * Java Native Interface routine to get whether a setup operation is currently underway. A setup
 * operation is currently underway if the setup routine is executing.
 * @return True if a setup operation is underway, false if it is not.
 * @see ccd_setup.html#CCD_Setup_Get_Setup_In_Progress
 * @see ccd_setup.html#CCD_Setup_Startup
 * @see ccd_setup.html#CCD_Setup_Dimensions
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jboolean JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Setup_1In_1Progress(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jboolean)CCD_Setup_Get_Setup_In_Progress(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Window_Flags<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of 
 * <a href="ccd_setup.html#CCD_Setup_Get_Window_Flags">CCD_Setup_Get_Window_Flags</a>,
 * which gets the de-interlace type.
 * @see ccd_setup.html#CCD_Setup_Get_Window_Flags
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Window_1Flags(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_Window_Flags(handle);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Window<br>
 * Signature: (I)Lngat/o/ccd/CCDLibrarySetupWindow;<br>
 * Java Native Interface implementation of <a href="ccd_setup.html#CCD_Setup_Get_Window">CCD_Setup_Get_Window</a>,
 * which gets the specified window parameters.
 * @see ccd_setup.html#CCD_Setup_Get_Window
 */
JNIEXPORT jobject JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Window(JNIEnv *env,jobject obj,jint index)
{
	CCD_Interface_Handle_T *handle = NULL;
	struct CCD_Setup_Window_Struct window;
	jclass cls;
	jmethodID mid;
	jobject windowInstance;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return NULL; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	retval = CCD_Setup_Get_Window(handle,index,&window);
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Get_Window");
		return NULL;
	}
/* get the class of CCDLibrarySetupWindow */
	cls = (*env)->FindClass(env,"ngat/o/ccd/CCDLibrarySetupWindow");
	/* if the class is null, one of the following exceptions occured:
	** ClassFormatError,ClassCircularityError,NoClassDefFoundError,OutOfMemoryError */
	if(cls == NULL)
		return NULL;
/* get CCDLibrarySetupWindow constructor */
	mid = (*env)->GetMethodID(env,cls,"<init>","(IIII)V");	
	if(mid == 0)
	{
		/* One of the following exceptions has been thrown:
		** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
		return NULL;
	}
/* call constructor */
	windowInstance = (*env)->NewObject(env,cls,mid,(jint)window.X_Start,(jint)window.Y_Start,
		(jint)window.X_End,(jint)window.Y_End);
	if(windowInstance == NULL)
	{
		/* One of the following exceptions has been thrown:
		** InstantiationException, OutOfMemoryError */
		return NULL;
	}
	return windowInstance;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Window_Width<br>
 * Signature: (I)I<br>
 * @see ccd_setup.html#CCD_Setup_Get_Window_Width
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Window_1Width(JNIEnv *env,jobject obj,
										 jint window_index)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_Window_Width(handle,(int)window_index);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Window_Height<br>
 * Signature: (I)I<br>
 * @see ccd_setup.html#CCD_Setup_Get_Window_Height
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Window_1Height(JNIEnv *env,jobject obj,
										  jint window_index)
{
	CCD_Interface_Handle_T *handle = NULL;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	return (jint)CCD_Setup_Get_Window_Height(handle,(int)window_index);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_High_Voltage_Analogue_ADU<br>
 * Signature: ()I<br>
 * @see ccd_setup.html#CCD_Setup_Get_High_Voltage_Analogue_ADU
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1High_1Voltage_1Analogue_1ADU(JNIEnv *env,
												jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval,adu;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* get high voltage ADU */
	retval = CCD_Setup_Get_High_Voltage_Analogue_ADU(handle,&adu);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Get_High_Voltage_Analogue_ADU");
	return adu;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Low_Voltage_Analogue_ADU<br>
 * Signature: ()I<br>
 * @see ccd_setup.html#CCD_Setup_Get_Low_Voltage_Analogue_ADU
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Low_1Voltage_1Analogue_1ADU(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval,adu;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* get low voltage ADU */
	retval = CCD_Setup_Get_Low_Voltage_Analogue_ADU(handle,&adu);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Get_Low_Voltage_Analogue_ADU");
	return adu;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU<br>
 * Signature: ()I<br>
 * @see ccd_setup.html#CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Minus_1Low_1Voltage_1Analogue_1ADU(JNIEnv *env,
												      jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval,adu;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* get voltage ADU */
	retval = CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU(handle,&adu);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Setup_Get_Minus_Low_Voltage_Analogue_ADU");
	return adu;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Setup_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for the ccd_setup part of the library.
 * @param env The JNI environment pointer.
 * @param obj The instance of CCDLibrary that called this routine.
 * @return The current error number of ccd_setup. A zero error number means an error has not occured.
 * @see ccd_setup.html#CCD_Setup_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Setup_1Get_1Error_1Number(JNIEnv *env,jobject obj)
{
	return CCD_Setup_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		ccd_temperature.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Temperature_Get<br>
 * Signature: ()D<br>
 * Java Native Interface implementation of <a href="ccd_temperature.html#CCD_Temperature_Get">CCD_Temperature_Get</a>,
 * which gets the current temperature of the CCD. 
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_temperature.html#CCD_Temperature_Get
 * @see #CCDLibrary_Throw_Exception
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jdouble JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Temperature_1Get(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	double dvalue;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1.0; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* get temperature */
	retval = CCD_Temperature_Get(handle,&dvalue);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Temperature_Get");
		return ((jdouble)dvalue);
	}
	return ((jdouble)dvalue);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Temperature_Set<br>
 * Signature: (D)V<br>
 * Java Native Interface implementation of <a href="ccd_temperature.html#CCD_Temperature_Set">CCD_Temperature_Set</a>,
 * which sets the temperature of the CCD. 
 * If an error occurs a CCDLibraryNativeException is thrown.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_temperature.html#CCD_Temperature_Set
 * @see #CCDLibrary_Throw_Exception
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Temperature_1Set(JNIEnv *env,jobject obj,
									jdouble target_temperature)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* set temperature */
	retval = CCD_Temperature_Set(handle,target_temperature);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
		CCDLibrary_Throw_Exception(env,obj,"CCD_Temperature_Set");
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Temperature_Get_Utility_Board_ADU<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of CCD_Temperature_Get_Utility_Board_ADU,
 * which gets the Analogue to Digital counts from the utility board mounted temperature sensor. 
 * If an error occurs a CCDLibraryNativeException is thrown.
 * @return The current ADU count is returned. If an error occurs an exception is thrown, and this routine
 * 	returns -1.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_temperature.html#CCD_Temperature_Get_Utility_Board_ADU
 * @see #CCDLibrary_Throw_Exception
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Temperature_1Get_1Utility_1Board_1ADU(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval,adu = -1;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* get utility board ADU */
	retval = CCD_Temperature_Get_Utility_Board_ADU(handle,&adu);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Temperature_Get_Utility_Board_ADU");
		return ((jint)adu);
	}
	return ((jint)adu);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Temperature_Get_Heater_ADU<br>
 * Signature: ()I<br>
 * Java Native Interface implementation of CCD_Temperature_Get_Heater_ADU,
 * which gets the current heater Analogue to Digital counts from the utility board. 
 * If an error occurs a CCDLibraryNativeException is thrown.
 * @return The current ADU count is returned. If an error occurs an exception is thrown, and this routine
 * 	returns -1.
 * @see ccd_interface.html#CCD_Interface_Handle_T
 * @see ccd_temperature.html#CCD_Temperature_Get_Heater_ADU
 * @see #CCDLibrary_Throw_Exception
 * @see #CCDLibrary_Handle_Map_Find
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Temperature_1Get_1Heater_1ADU(JNIEnv *env,jobject obj)
{
	CCD_Interface_Handle_T *handle = NULL;
	int retval,adu = -1;

	/* get interface handle from CCDLibrary instance map */
	if(!CCDLibrary_Handle_Map_Find(env,obj,&handle))
		return -1; /* CCDLibrary_Handle_Map_Find throws an exception on failure */
	/* get heater ADU */
	retval = CCD_Temperature_Get_Heater_ADU(handle,&adu);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Temperature_Get_Heater_ADU");
		return ((jint)adu);
	}
	return ((jint)adu);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Temperature_Heater_ADU_To_Power<br>
 * Signature: (I)D<br>
 * Java Native Interface implementation of CCD_Temperature_Heater_ADU_To_Power,
 * which converts a heater adu into a measure of power (in Watts) put into the dewar.
 * If an error occurs a CCDLibraryNativeException is thrown.
 * @return The heater power in Watts is returned.
 * @see ccd_temperature.html#CCD_Temperature_Heater_ADU_To_Power
 * @see #CCDLibrary_Throw_Exception
 */
JNIEXPORT jdouble JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Temperature_1Heater_1ADU_1To_1Power(JNIEnv *env, jobject obj,
											      jint heater_adu)
{
	int retval;
	double power;

	retval = CCD_Temperature_Heater_ADU_To_Power(heater_adu,&power);
	/* if an error occured throw an exception. */
	if(retval == FALSE)
	{
		CCDLibrary_Throw_Exception(env,obj,"CCD_Temperature_Heater_ADU_To_Power");
		return ((jdouble)power);
	}
	return (jdouble)power;
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Temperature_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for the ccd_temperature part of the library.
 * @return The current error number of ccd_temperature. A zero error number means an error has not occured.
 * @see ccd_temperature.html#CCD_Temperature_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Temperature_1Get_1Error_1Number(JNIEnv *env,jobject obj)
{
	return CCD_Temperature_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		ccd_text.c
** ------------------------------------------------------------------------------ */
/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Text_Set_Print_Level<br>
 * Signature: (I)V<br>
 * Java Native Interface implementation of 
 * <a href="ccd_text.html#CCD_Text_Set_Print_Level">CCD_Text_Set_Print_Level</a>,
 * which sets the amount of information displayed when the text interface device is enabled. 
 * @see ccd_text.html#CCD_Text_Set_Print_Level
 */
JNIEXPORT void JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Text_1Set_1Print_1Level(JNIEnv *env,jobject obj,jint level)
{
	CCD_Text_Set_Print_Level(level);
}

/**
 * Class:     ngat_o_ccd_CCDLibrary<br>
 * Method:    CCD_Text_Get_Error_Number<br>
 * Signature: ()I<br>
 * Java Native Interface routine to get the error number for this module.
 * @return The current value of the error number for this module. A zero error number means an error has not occured.
 * @see ccd_interface.html#CCD_Text_Get_Error_Number
 */
JNIEXPORT jint JNICALL Java_ngat_o_ccd_CCDLibrary_CCD_1Text_1Get_1Error_1Number(JNIEnv *env,jobject obj)
{
	return CCD_Text_Get_Error_Number();
}

/* ------------------------------------------------------------------------------
** 		Internal routines
** ------------------------------------------------------------------------------ */
/**
 * This routine throws an exception. The error generated is from the error codes in libo_ccd, 
 * it assumes another routine has generated an error and this routine packs this error into an exception to return
 * to the Java code, using CCDLibrary_Throw_Exception_String. The total length of the error string should
 * not be longer than CCD_ERROR_LENGTH. A new line is added to the start of the error string,
 * so that the error string returned from libo_ccd is formatted properly.
 * @param env The JNI environment pointer.
 * @param function_name The name of the function in which this exception is being generated for.
 * @param obj The instance of CCDLibrary that threw the error.
 * @see ccd_global.html#CCD_Error_To_String
 * @see #CCDLibrary_Throw_Exception_String
 * @see #CCD_ERROR_LENGTH
 */
static void CCDLibrary_Throw_Exception(JNIEnv *env,jobject obj,char *function_name)
{
	char error_string[CCD_ERROR_LENGTH];

	strcpy(error_string,"\n");
	CCD_Global_Error_String(error_string+strlen(error_string));
	CCDLibrary_Throw_Exception_String(env,obj,function_name,error_string);
}

/**
 * This routine throws an exception of class ngat/o/ccd/CCDLibraryNativeException. 
 * This is used to report all libo_ccd error messages back to the Java layer.
 * @param env The JNI environment pointer.
 * @param obj The instance of CCDLibrary that threw the error.
 * @param function_name The name of the function in which this exception is being generated for.
 * @param error_string The string to pass to the constructor of the exception.
 */
static void CCDLibrary_Throw_Exception_String(JNIEnv *env,jobject obj,char *function_name,char *error_string)
{
	jclass exception_class = NULL;
	jobject exception_instance = NULL;
	jstring error_jstring = NULL;
	jmethodID mid;
	int retval;

	exception_class = (*env)->FindClass(env,"ngat/o/ccd/CCDLibraryNativeException");
	if(exception_class != NULL)
	{
	/* get CCDLibraryNativeException constructor */
		mid = (*env)->GetMethodID(env,exception_class,"<init>",
					  "(Ljava/lang/String;Lngat/o/ccd/CCDLibrary;)V");
		if(mid == 0)
		{
			/* One of the following exceptions has been thrown:
			** NoSuchMethodError, ExceptionInInitializerError, OutOfMemoryError */
			fprintf(stderr,"CCDLibrary_Throw_Exception_String:GetMethodID failed:%s:%s\n",function_name,
				error_string);
			return;
		}
	/* convert error_string to JString */
		error_jstring = (*env)->NewStringUTF(env,error_string);
	/* call constructor */
		exception_instance = (*env)->NewObject(env,exception_class,mid,error_jstring,obj);
		if(exception_instance == NULL)
		{
			/* One of the following exceptions has been thrown:
			** InstantiationException, OutOfMemoryError */
			fprintf(stderr,"CCDLibrary_Throw_Exception_String:NewObject failed %s:%s\n",
				function_name,error_string);
			return;
		}
	/* throw instance */
		retval = (*env)->Throw(env,(jthrowable)exception_instance);
		if(retval !=0)
		{
			fprintf(stderr,"CCDLibrary_Throw_Exception_String:Throw failed %d:%s:%s\n",retval,
				function_name,error_string);
		}
	}
	else
	{
		fprintf(stderr,"CCDLibrary_Throw_Exception_String:FindClass failed:%s:%s\n",function_name,
			error_string);
	}
}

/**
 * libo_ccd Log Handler for the Java layer interface. This calls the ngat.o.ccd.CCDLibrary logger's 
 * log(int level,String message) method with the parameters supplied to this routine.
 * If the logger instance is NULL, or the log_method_id is NULL the call is not made.
 * Otherwise, A java.lang.String instance is constructed from the string parameter,
 * and the JNI CallVoidMEthod routine called to call log().
 * @param level The log level of the message.
 * @param string The message to log.
 * @see #java_vm
 * @see #logger
 * @see #log_method_id
 */
static void CCDLibrary_Log_Handler(int level,char *string)
{
	JNIEnv *env = NULL;
	jstring java_string = NULL;

	if(logger == NULL)
	{
		fprintf(stderr,"CCDLibrary_Log_Handler:logger was NULL (%d,%s).\n",level,string);
		return;
	}
	if(log_method_id == NULL)
	{
		fprintf(stderr,"CCDLibrary_Log_Handler:log_method_id was NULL (%d,%s).\n",level,string);
		return;
	}
	if(java_vm == NULL)
	{
		fprintf(stderr,"CCDLibrary_Log_Handler:java_vm was NULL (%d,%s).\n",level,string);
		return;
	}
/* get java env for this thread */
	(*java_vm)->AttachCurrentThread(java_vm,(void**)&env,NULL);
	if(env == NULL)
	{
		fprintf(stderr,"CCDLibrary_Log_Handler:env was NULL (%d,%s).\n",level,string);
		return;
	}
	if(string == NULL)
	{
		fprintf(stderr,"CCDLibrary_Log_Handler:string (%d) was NULL.\n",level);
		return;
	}
/* convert C to Java String */
	java_string = (*env)->NewStringUTF(env,string);
/* call log method on logger instance */
	(*env)->CallVoidMethod(env,logger,log_method_id,(jint)level,java_string);
}

/**
 * This routine creates a re-allocatable c list of strings, from a jobject of class java.util.List
 * containing java.lang.String s. Note the c list of strings will need freeing in the same JNI routine.
 * @param env The JNI environment pointer.
 * @param obj The instance of CCDLibrary that called this routine.
 * @param java_list A jobject which implements the interface java.util.List. This list should contain
 *        jobjects that are jstrings. The string's characters are extracted and put into c_list
 * @param jni_jstring_list A pointer to a list a jstrings. This list is allocated by this routine,
 *        and filled with jstring's got from java_list. Each jstring in the returned list has an equivalent 
 *        C string in the c_list. This list is created to allow us to free the c_list correctly.
 * @param jni_jstring_count The address of an integer which, on exit, contains the number of jstrings in the 
 *       jni_jstring_list.
 * @param c_list The address of a reallocatable list of character pointers (char *). This will contain
 * 	a list a c strings on exit.
 * @param c_list_count The address of an integer to fill with the number of strings in the list.
 * @return True if the routine is successfull, false if it fails. If FALSE is returned, a JNI exception is thrown.
 * @see #CCDLibrary_Throw_Exception_String
 * @see #CCDLibrary_Java_String_List_Free
 */
static int CCDLibrary_Java_String_List_To_C_List(JNIEnv *env,jobject obj,jobject java_list,
						 jstring **jni_jstring_list,int *jni_jstring_count,
						 char ***c_list,int *c_list_count)
{
	jclass java_list_class = NULL;
	jmethodID list_size_mid = NULL;
	jmethodID list_get_mid = NULL;
	jstring java_string = NULL;
	int index;

	if(c_list == NULL)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List","C List was NULL.");
		return FALSE;
	}
	if(c_list_count == NULL)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
						  "C List count was NULL.");
		return FALSE;
	}
	/* get class and method id's */
	java_list_class = (*env)->GetObjectClass(env,java_list);
	list_size_mid = (*env)->GetMethodID(env,java_list_class, "size","()I");
	if(list_size_mid == 0)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
						  "List size Method ID was 0.");
		return FALSE;
	}
	list_get_mid = (*env)->GetMethodID(env,java_list_class, "get","(I)Ljava/lang/Object;");
	if(list_get_mid == 0)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
						  "List get Method ID was 0.");
		return FALSE;
	}
	/* get count of elements in java_list */
	(*c_list_count) = (int)((*env)->CallIntMethod(env,java_list,list_size_mid));
	(*jni_jstring_count) = (*c_list_count);
	/* allocate c_list to match number of elements in java_list */
	(*c_list) = (char **)malloc((*c_list_count)*sizeof(char*));
	if((*c_list) == NULL)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
					   "Memory Allocation Error(c_list).");
		return FALSE;
	}
	(*jni_jstring_list) = (jstring *)malloc((*jni_jstring_count)*sizeof(char*));
	if((*jni_jstring_list) == NULL)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
					   "Memory Allocation Error(jni_jstring_list).");
		return FALSE;
	}
	/* enter loop over elements in java_list */
	for(index = 0; index < (*c_list_count); index ++)
	{
		/* get the jstring at this index in the list */
		java_string = (jstring)((*env)->CallObjectMethod(env,java_list,list_get_mid,index));
		if(java_string == NULL)
		{
			/* free data elements already allocated. */
			CCDLibrary_Java_String_List_Free(env,obj,(*jni_jstring_list),(*jni_jstring_count),
							 (*c_list),(*c_list_count));
			(*jni_jstring_list) = NULL;
			(*jni_jstring_count) = 0;
			(*c_list) = NULL;
			(*c_list_count) = 0;
			CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
						   "Java String was NULL.");
			return FALSE;
		}
		(*jni_jstring_list)[index] = java_string;
		/* Get the filename from a java string to a c null terminated string */
		(*c_list)[index] = (char*)((*env)->GetStringUTFChars(env,java_string,0));
		if((*c_list)[index] == NULL)
		{
			/* free data elements already allocated. */
			CCDLibrary_Java_String_List_Free(env,obj,(*jni_jstring_list),(*jni_jstring_count),
							 (*c_list),(*c_list_count));
			(*jni_jstring_list) = NULL;
			(*jni_jstring_count) = 0;
			(*c_list) = NULL;
			(*c_list_count) = 0;
			CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_To_C_List",
						   "C String was NULL.");
			return FALSE;
		}
	}/* end for on list */
	return TRUE;
}

/**
 * Routine to free data created by CCDLibrary_Java_String_List_To_C_List. This routine copes with
 * list in which half the elements have not been converted, i.e. it can be used to free during error recovery.
 * The passed in lists should have the same number of elements.
 * @param env The JNI environment pointer.
 * @param obj The instance of CCDLibrary that called this routine.
 * @param jni_jstring_list A list of jstrings. Each jstring in the list has an equivalent 
 *        C string in the c_list.
 * @param jni_jstring_count The number of jstrings in the jni_jstring_list.
 * @param c_list The list of character pointers (char *). This contains a list a c strings to free.
 * @param c_list_count The number of strings in the list.
 * @return True if the routine is successfull, false if it fails. If FALSE is returned, a JNI exception is thrown.
 * @see #CCDLibrary_Java_String_List_To_C_List
 * @see #CCDLibrary_Throw_Exception_String
 */
static int CCDLibrary_Java_String_List_Free(JNIEnv *env,jobject obj,
					    jstring *jni_jstring_list,int jni_jstring_count,
					    char **c_list,int c_list_count)
{
	int index;

	if(jni_jstring_count != c_list_count)
	{
		CCDLibrary_Throw_Exception_String(env,obj,"CCDLibrary_Java_String_List_Free",
					   "jni_jstring_count and c_list_count not equal.");
		return FALSE;
	}
	for(index = 0;index < jni_jstring_count;index ++)
	{
		/* This if test allows this routine to free half completed java string lists,
		** where an error occured during convertion and we only want to free what was converted.*/
		if((jni_jstring_list[index] != NULL) && (c_list[index] != NULL))
		{
			(*env)->ReleaseStringUTFChars(env,jni_jstring_list[index],c_list[index]);
		}
	}/* end for */
	if(jni_jstring_list != NULL)
		free(jni_jstring_list);
	if(c_list != NULL)
		free(c_list);
	return TRUE;
}

/**
 * Routine to add a mapping from the CCDLibrary instance instance to the opened CCD Interface Handle
 * interface_handle, in the Handle_Map_List.
 * @param instance The CCDLibrary instance.
 * @param interface_handle The interface handle.
 * @return The routine returns TRUE if the map is added (or updated), FALSE if there was no room left
 *         in the mapping list. 
 *         CCDLibrary_Throw_Exception_String is used to throw a Java exception if the routine returns FALSE.
 * @see #HANDLE_MAP_SIZE
 * @see #Handle_Map_List
 * @see #CCDLibrary_Throw_Exception_String
 */
static int CCDLibrary_Handle_Map_Add(JNIEnv *env,jobject instance,CCD_Interface_Handle_T* interface_handle)
{
	int i,done;
	jobject global_instance = NULL;

	/* does the map already exist? */
	i = 0;
	done = FALSE;
	while((i < HANDLE_MAP_SIZE)&&(done == FALSE))
	{
		if((*env)->IsSameObject(env,Handle_Map_List[i].CCDLibrary_Instance_Handle,instance))
			done = TRUE;
		else
			i++;
	}
	if(done == TRUE)/* found an existing interface handle for this CCDLIbrary instance */
	{
		/* update handle */
		Handle_Map_List[i].Interface_Handle = interface_handle;
	}
	else
	{
		/* look for a blank index to put the map */
		i = 0;
		done = FALSE;
		while((i < HANDLE_MAP_SIZE)&&(done == FALSE))
		{
			if(Handle_Map_List[i].CCDLibrary_Instance_Handle == NULL)
				done = TRUE;
			else
				i++;
		}
		if(done == FALSE)
		{
			CCDLibrary_Throw_Exception_String(env,instance,"CCDLibrary_Handle_Map_Add",
							  "No empty slots in handle map.");
			return FALSE;
		}
		/* index i is free, add handle map here */
		global_instance = (*env)->NewGlobalRef(env,instance);
		if(global_instance == NULL)
		{
			CCDLibrary_Throw_Exception_String(env,instance,"CCDLibrary_Handle_Map_Add",
							  "Failed to create Global reference of instance.");
			return FALSE;
		}
		fprintf(stdout,"CCDLibrary_Handle_Map_Add:Adding instance %p with handle %p at map index %d.\n",
			(void*)global_instance,(void*)interface_handle,i);
		Handle_Map_List[i].CCDLibrary_Instance_Handle = global_instance;
		Handle_Map_List[i].Interface_Handle = interface_handle;
	}
	return TRUE;
}

/**
 * Routine to delete a mapping from the CCDLibrary instance instance to the opened CCD Interface Handle
 * interface_handle, in the Handle_Map_List.
 * @param instance The CCDLibrary instance to remove from the list.
 * @return The routine returns TRUE if the map is deleted (or updated), FALSE if the mapping could not be found
 *         in the mapping list.
 *         CCDLibrary_Throw_Exception_String is used to throw a Java exception if the routine returns FALSE.
 * @see #HANDLE_MAP_SIZE
 * @see #Handle_Map_List
 * @see #CCDLibrary_Throw_Exception_String
 */
static int CCDLibrary_Handle_Map_Delete(JNIEnv *env,jobject instance)
{
	int i,done;

  	/* does the map already exist? */
	i = 0;
	done = FALSE;
	while((i < HANDLE_MAP_SIZE)&&(done == FALSE))
	{
		if((*env)->IsSameObject(env,Handle_Map_List[i].CCDLibrary_Instance_Handle,instance))
			done = TRUE;
		else
			i++;
	}
	if(done == FALSE)
	{
		CCDLibrary_Throw_Exception_String(env,instance,"CCDLibrary_Handle_Map_Delete",
						  "Failed to find CCDLibrary instance in handle map.");
		return FALSE;
	}
	/* found an existing interface handle for this CCDLIbrary instance at index i */
	/* delete this map at index i */
	fprintf(stdout,"CCDLibrary_Handle_Map_Delete:Deleting instance %p with handle %p at map index %d.\n",
		(void*)Handle_Map_List[i].CCDLibrary_Instance_Handle,(void*)Handle_Map_List[i].Interface_Handle,i);
	(*env)->DeleteGlobalRef(env,Handle_Map_List[i].CCDLibrary_Instance_Handle);
	Handle_Map_List[i].CCDLibrary_Instance_Handle = NULL;
	Handle_Map_List[i].Interface_Handle = NULL;
	return TRUE;
}

/**
 * Routine to find a mapping from the CCDLibrary instance instance to the opened CCD Interface Handle
 * interface_handle, in the Handle_Map_List.
 * @param instance The CCDLibrary instance.
 * @param interface_handle The address of an interface handle, to fill with the interface handle for
 *        this CCDLibrary instance, if one is successfully found.
 * @return The routine returns TRUE if the mapping is found and returned,, FALSE if there was no mapping
 *         for this CCDLibrary instance, or the interface_handle pointer was NULL.
 *         CCDLibrary_Throw_Exception_String is used to throw a Java exception if the routine returns FALSE.
 * @see #HANDLE_MAP_SIZE
 * @see #Handle_Map_List
 * @see #CCDLibrary_Throw_Exception_String
 */
static int CCDLibrary_Handle_Map_Find(JNIEnv *env,jobject instance,CCD_Interface_Handle_T** interface_handle)
{
	int i,done;

	if(interface_handle == NULL)
	{
		CCDLibrary_Throw_Exception_String(env,instance,"CCDLibrary_Handle_Map_Find",
						  "interface handle was NULL.");
		return FALSE;
	}
	i = 0;
	done = FALSE;
	while((i < HANDLE_MAP_SIZE)&&(done == FALSE))
	{
		if((*env)->IsSameObject(env,Handle_Map_List[i].CCDLibrary_Instance_Handle,instance))
			done = TRUE;
		else
			i++;
	}
	if(done == FALSE)
	{
		fprintf(stdout,"CCDLibrary_Handle_Map_Find:Failed to find instance %p.\n",(void*)instance);
		CCDLibrary_Throw_Exception_String(env,instance,"CCDLibrary_Handle_Map_Find",
						  "CCDLibrary instance handle was not found.");
		return FALSE;
	}
	(*interface_handle) = Handle_Map_List[i].Interface_Handle;
	return TRUE;
}

/*
** $Log: not supported by cvs2svn $
*/
