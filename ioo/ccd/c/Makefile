# $Header: /space/home/eng/cjm/cvs/ioo/ccd/c/Makefile,v 1.1 2012-07-19 14:06:23 cjm Exp $

include			../../../Makefile.common
include			../../Makefile.common
include			../Makefile.common

INCDIR = $(O_CCD_SRC_HOME)/include
DOCSDIR = $(O_CCD_DOC_HOME)/cdocs

#Do we want to byte swap the ccd data read from the camera.
# Previous to v1.3 - does NOT byte swap in the device driver - we do it in software, this flag is set
# v1.4 byte swapped in the device driver - this flag was unset
# v1.7 does NOT byte swap in the device driver - we do it in software, this flag is set
# Note also depends on whether machine has same byte ordering as PCI interface, this might be different
# for SOlaris and Linux
# So for v1.7, byte swap should be set for Solaris, but not for Linux
#BYTESWAPCFLAGS = -DCCD_EXPOSURE_BYTE_SWAP=1
BYTESWAPCFLAGS = 

# Do we wish to link with CFITSIO routines?
FITSCFLAGS = -DCFITSIO=1 -I$(CFITSIOINCDIR) 
CFITSIOLIB = -lcfitsio

# FITSCFLAGS =
# CFITSIOLIB =

# Do we want to enable logging?
# no logging
#LOGGINGCFLAGS = -DLOGGING=0
# some logging
#LOGGINGCFLAGS = -DLOGGING=1
# some logging
#LOGGINGCFLAGS = -DLOGGING=4
#lots of logging. 
#LOGGINGCFLAGS = -DLOGGING=5
#LOGGINGCFLAGS = -DLOGGING=10
LOGGINGCFLAGS = -DLOGGING=10

# SLALIB link information - used only for calculating MJD at the moment
#MJDCFLAGS 	= -DSLALIB=1 -I${LT_SRC_HOME}/astrometry/include
#MJDLIB		= -lsla
# NGAT Astro MJD link informatiom
MJDCFLAGS 	= -DNGATASTRO=1 -I${LT_SRC_HOME}/ngatastro/include
MJDLIB		= -lngatastro

# when reading out it could help if we lock down the memory we are reading out into to prevent page swaps
# 1. No memory locking
MLOCKCFLAGS	= 
# 2. Memory locking
#MLOCKCFLAGS	= -DCCD_GLOBAL_READOUT_MLOCK

#Do we want MUTEX protected locking around controller command communication?
MUTEXCFLAGS = -DCCD_DSP_MUTEXED=1
#MUTEXCFLAGS = 

# when reading out it helps if we are running at a better priority
# This can be done by using priority change code and running as root.
# - No priority changes in library, code will run as user.
PRIORITYCFLAGS		= -DCCD_GLOBAL_READOUT_PRIORITY=0
# - Priority changes in library, using POSIX.4 scheduler, code must run as root.
#PRIORITYCFLAGS		= -DCCD_GLOBAL_READOUT_PRIORITY=1
# - Priority changes in library, using BSD/SVr4 process management, code must run as root.
#PRIORITYCFLAGS		= -DCCD_GLOBAL_READOUT_PRIORITY=2

# Do we want to stop the timing board Idling mode whilst downloading a DSP program to it?
# It is not clear whether we should do this.
# Don't stop timing board Idling whilst downloading DSP code. (voodoo v1.4 did this)
#TIMINGDOWNLOADIDLECFLAGS =
# Stop timing board Idling whilst downloading DSP code. (ccdtool did this/voodoo v1.7 does this)
TIMINGDOWNLOADIDLECFLAGS = -DCCD_SETUP_TIMING_DOWNLOAD_IDLE=1

# In some versions of the DSP code the utility board used to lock the controller up,
# if we sent an RDM/WRM/Read temperature during an exposure.
# In v1.7, the controller locks up if a RDM is sent during a readout.
# This flag enables us to conditionally compile some code to test for this:
# Return an error if RDM/WRM/temperature get called when we are exposing.
#UTILEXPOSURECHECKFLAGS = -DCCD_DSP_UTIL_EXPOSURE_CHECK=1
# Return an error if RDM/WRM/temperature get called when we are reading out.
#UTILEXPOSURECHECKFLAGS = -DCCD_DSP_UTIL_EXPOSURE_CHECK=2
# New FrodoSpec version
# Allow RET during exposure (sent to timing board)
# but don't allow RDM/WRM/TDL to UTIL board during exposure/readout
UTILEXPOSURECHECKFLAGS = -DCCD_DSP_UTIL_EXPOSURE_CHECK=3
# Allow RDM/WRM/temperature get calls when we are exposing.
#UTILEXPOSURECHECKFLAGS = 

#CCD_FILTER_WHEEL_FAKE
# Add -DCCD_FILTER_WHEEL_FAKE to fake filter wheel movement
CFLAGS = -g $(CCHECKFLAG) -I$(INCDIR) -I$(JNIINCDIR) -I$(JNIMDINCDIR) -L$(LT_LIB_HOME) \
	$(FITSCFLAGS) $(MJDCFLAGS) $(MUTEXCFLAGS) $(TIMINGDOWNLOADIDLECFLAGS) \
	$(UTILEXPOSURECHECKFLAGS) $(BYTESWAPCFLAGS) $(PRIORITYCFLAGS) $(MLOCKCFLAGS) $(LOGGINGCFLAGS) $(LOG_UDP_CFLAGS)

LINTFLAGS = -I$(INCDIR) -I$(JNIINCDIR) -I$(JNIMDINCDIR)
DOCFLAGS = -static
SRCS 		= 	ccd_interface.c ccd_pci.c ccd_text.c ccd_global.c ccd_dsp.c ccd_dsp_download.c \
			ccd_temperature.c ccd_setup.c ccd_exposure.c ccd_filter_wheel.c
HEADERS		=	$(SRCS:%.c=%.h) ccd_interface_private.h ccd_exposure_private.h ccd_setup_private.h
OBJS		=	$(SRCS:%.c=%.o)
DOCS 		= 	$(SRCS:%.c=$(DOCSDIR)/%.html)
JAVASRCS 	= 	$(SRCS) ngat_o_ccd_CCDLibrary.c
JAVAOBJS	=	$(JAVASRCS:%.c=%.o)
JAVADOCS 	= 	$(JAVASRCS:%.c=$(DOCSDIR)/%.html)
LIBNAME		=	lib$(O_HOME)_$(O_CCD_HOME)

top:  $(INCDIR)/ngat_o_ccd_CCDLibrary.h shared docs 

shared: $(LT_LIB_HOME)/$(LIBNAME).so

$(LT_LIB_HOME)/$(LIBNAME).so: $(JAVAOBJS)
	cc $(CCSHAREDFLAG) $(CFLAGS) $(JAVAOBJS) -o $@ $(TIMELIB) $(CFITSIOLIB) $(MJDLIB)

static: $(LT_LIB_HOME)/$(LIBNAME).a

$(LT_LIB_HOME)/$(LIBNAME).a: $(OBJS)
	ar rcv $@ $?

# special case for ngat_o_ccd_CCDLibrary - depending on whether ngat.o.ccd.CCDLibrary has changed
# recreate the machine generated JNI header
# touch it, javah does not update the timestamp correctly
# $(LT_JAVALIB_HOME)/ngat_o.jar
$(INCDIR)/ngat_o_ccd_CCDLibrary.h: $(O_SRC_HOME)/java/ngat/o/ccd/CCDLibrary.java
	$(JAVAH) -classpath $(O_BIN_HOME)/java:$(CLASSPATH) -jni -d $(INCDIR) ngat.o.ccd.CCDLibrary
	$(TOUCH) $@

docs: $(JAVADOCS)

$(JAVADOCS): $(JAVASRCS)
	-$(CDOC) -d $(DOCSDIR) -h $(INCDIR) $(DOCFLAGS) $(JAVASRCS)

$(JAVADOCS) : $(JAVASRCS)

checkout:
	-$(CO) $(CO_OPTIONS) $(JAVASRCS)
	-(cd $(INCDIR); $(CO) $(CO_OPTIONS) $(HEADERS);)

checkin:
	-$(CI) $(CI_OPTIONS) $(JAVASRCS)
	-(cd $(INCDIR); $(CI) $(CI_OPTIONS) $(HEADERS);)

staticdepend:
	makedepend $(MAKEDEPENDFLAGS) -- $(CFLAGS) -- $(SRCS)

depend:
	makedepend $(MAKEDEPENDFLAGS) -- $(CFLAGS) -- $(JAVASRCS)

lint:
	$(LINT)	$(LINTFLAGS) $(JAVASRCS)

clean:
	-$(RM) $(RM_OPTIONS) $(JAVAOBJS) $(INCDIR)/ngat_o_ccd_CCDLibrary.h $(LT_LIB_HOME)/$(LIBNAME).so $(LT_LIB_HOME)/$(LIBNAME)_g.so $(LT_LIB_HOME)/$(LIBNAME).a $(TIDY_OPTIONS)

tidy:
	-$(RM) $(RM_OPTIONS) $(TIDY_OPTIONS)
	-(cd $(INCDIR); $(RM) $(RM_OPTIONS) $(TIDY_OPTIONS);)

backup: tidy
	-$(RM) $(RM_OPTIONS) $(JAVAOBJS) $(INCDIR)/ngat_o_ccd_CCDLibrary.h

#
# $Log: not supported by cvs2svn $
# Revision 1.21  2008/12/11 14:40:59  cjm
# Added private headers.
#
# Revision 1.20  2008/07/22 13:37:34  cjm
# Added MAKEDEPENDFLAGS.
#
#

# DO NOT DELETE

ccd_interface.o: /usr/include/errno.h /usr/include/features.h
ccd_interface.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_interface.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_interface.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_interface.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_interface.o: /usr/include/asm-generic/errno-base.h /usr/include/fcntl.h
ccd_interface.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
ccd_interface.o: /usr/include/bits/types.h
ccd_interface.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_interface.o: /usr/include/bits/typesizes.h /usr/include/time.h
ccd_interface.o: /usr/include/endian.h /usr/include/bits/endian.h
ccd_interface.o: /usr/include/sys/select.h /usr/include/bits/select.h
ccd_interface.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ccd_interface.o: /usr/include/sys/sysmacros.h
ccd_interface.o: /usr/include/bits/pthreadtypes.h /usr/include/stdio.h
ccd_interface.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_interface.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_interface.o: /usr/include/gconv.h
ccd_interface.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_interface.o: /usr/include/bits/stdio_lim.h
ccd_interface.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
ccd_interface.o: /usr/include/alloca.h /usr/include/string.h
ccd_interface.o: /usr/include/signal.h /usr/include/bits/signum.h
ccd_interface.o: /usr/include/bits/siginfo.h /usr/include/bits/sigaction.h
ccd_interface.o: /usr/include/bits/sigcontext.h /usr/include/asm/sigcontext.h
ccd_interface.o: /usr/include/bits/sigstack.h /usr/include/bits/sigthread.h
ccd_interface.o: /usr/include/sys/time.h
ccd_interface.o: /home/dev/src/log_udp/include/log_udp.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_interface_private.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_exposure_private.h
ccd_interface.o: /home/dev/src/o/ccd/include/ccd_setup_private.h
ccd_pci.o: /usr/include/errno.h /usr/include/features.h
ccd_pci.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_pci.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_pci.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_pci.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_pci.o: /usr/include/asm-generic/errno-base.h /usr/include/fcntl.h
ccd_pci.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
ccd_pci.o: /usr/include/bits/types.h
ccd_pci.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_pci.o: /usr/include/bits/typesizes.h /usr/include/time.h
ccd_pci.o: /usr/include/endian.h /usr/include/bits/endian.h
ccd_pci.o: /usr/include/sys/select.h /usr/include/bits/select.h
ccd_pci.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ccd_pci.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
ccd_pci.o: /usr/include/signal.h /usr/include/bits/signum.h
ccd_pci.o: /usr/include/bits/siginfo.h /usr/include/bits/sigaction.h
ccd_pci.o: /usr/include/bits/sigcontext.h /usr/include/asm/sigcontext.h
ccd_pci.o: /usr/include/bits/sigstack.h /usr/include/bits/sigthread.h
ccd_pci.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
ccd_pci.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_pci.o: /usr/include/gconv.h
ccd_pci.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_pci.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ccd_pci.o: /usr/include/stdlib.h /usr/include/alloca.h /usr/include/string.h
ccd_pci.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
ccd_pci.o: /usr/include/bits/confname.h /usr/include/getopt.h
ccd_pci.o: /usr/include/sys/time.h /usr/include/sys/mman.h
ccd_pci.o: /usr/include/bits/mman.h /home/dev/src/o/ccd/include/ccd_global.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_interface_private.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_exposure_private.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_setup_private.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_pci.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_text.o: /usr/include/stdio.h /usr/include/features.h
ccd_text.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_text.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_text.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_text.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ccd_text.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_text.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_text.o: /usr/include/gconv.h
ccd_text.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_text.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ccd_text.o: /usr/include/stdlib.h /usr/include/sys/types.h
ccd_text.o: /usr/include/time.h /usr/include/endian.h
ccd_text.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ccd_text.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ccd_text.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
ccd_text.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
ccd_text.o: /usr/include/string.h /usr/include/errno.h
ccd_text.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_text.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_text.o: /usr/include/asm-generic/errno-base.h /usr/include/unistd.h
ccd_text.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
ccd_text.o: /usr/include/getopt.h /home/dev/src/o/ccd/include/ccd_global.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_interface_private.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_exposure_private.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_setup_private.h
ccd_text.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_global.o: /usr/include/stdio.h /usr/include/features.h
ccd_global.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_global.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_global.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_global.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ccd_global.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_global.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_global.o: /usr/include/gconv.h
ccd_global.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_global.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ccd_global.o: /usr/include/errno.h /usr/include/bits/errno.h
ccd_global.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
ccd_global.o: /usr/include/asm-generic/errno.h
ccd_global.o: /usr/include/asm-generic/errno-base.h
ccd_global.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/limits.h
ccd_global.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/syslimits.h
ccd_global.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
ccd_global.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
ccd_global.o: /usr/include/bits/posix2_lim.h /usr/include/sys/types.h
ccd_global.o: /usr/include/time.h /usr/include/endian.h
ccd_global.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ccd_global.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ccd_global.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
ccd_global.o: /usr/include/bits/pthreadtypes.h /usr/include/string.h
ccd_global.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
ccd_global.o: /usr/include/bits/confname.h /usr/include/getopt.h
ccd_global.o: /home/dev/src/log_udp/include/log_udp.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_filter_wheel.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_temperature.h
ccd_global.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_dsp.o: /usr/include/errno.h /usr/include/features.h
ccd_dsp.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_dsp.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_dsp.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_dsp.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_dsp.o: /usr/include/asm-generic/errno-base.h /usr/include/fcntl.h
ccd_dsp.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
ccd_dsp.o: /usr/include/bits/types.h
ccd_dsp.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_dsp.o: /usr/include/bits/typesizes.h /usr/include/time.h
ccd_dsp.o: /usr/include/endian.h /usr/include/bits/endian.h
ccd_dsp.o: /usr/include/sys/select.h /usr/include/bits/select.h
ccd_dsp.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ccd_dsp.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
ccd_dsp.o: /usr/include/signal.h /usr/include/bits/signum.h
ccd_dsp.o: /usr/include/bits/siginfo.h /usr/include/bits/sigaction.h
ccd_dsp.o: /usr/include/bits/sigcontext.h /usr/include/asm/sigcontext.h
ccd_dsp.o: /usr/include/bits/sigstack.h /usr/include/bits/sigthread.h
ccd_dsp.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
ccd_dsp.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_dsp.o: /usr/include/gconv.h
ccd_dsp.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_dsp.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ccd_dsp.o: /usr/include/stdlib.h /usr/include/alloca.h /usr/include/string.h
ccd_dsp.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
ccd_dsp.o: /usr/include/bits/confname.h /usr/include/getopt.h
ccd_dsp.o: /usr/include/pthread.h /usr/include/sched.h
ccd_dsp.o: /usr/include/bits/sched.h /usr/include/bits/setjmp.h
ccd_dsp.o: /home/dev/src/log_udp/include/log_udp.h
ccd_dsp.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_dsp.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_dsp.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_dsp.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_dsp.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_dsp.o: /home/dev/src/o/ccd/include/ccd_filter_wheel.h
ccd_dsp_download.o: /usr/include/errno.h /usr/include/features.h
ccd_dsp_download.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_dsp_download.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_dsp_download.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_dsp_download.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_dsp_download.o: /usr/include/asm-generic/errno-base.h
ccd_dsp_download.o: /usr/include/fcntl.h /usr/include/bits/fcntl.h
ccd_dsp_download.o: /usr/include/sys/types.h /usr/include/bits/types.h
ccd_dsp_download.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_dsp_download.o: /usr/include/bits/typesizes.h /usr/include/time.h
ccd_dsp_download.o: /usr/include/endian.h /usr/include/bits/endian.h
ccd_dsp_download.o: /usr/include/sys/select.h /usr/include/bits/select.h
ccd_dsp_download.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ccd_dsp_download.o: /usr/include/sys/sysmacros.h
ccd_dsp_download.o: /usr/include/bits/pthreadtypes.h /usr/include/signal.h
ccd_dsp_download.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
ccd_dsp_download.o: /usr/include/bits/sigaction.h
ccd_dsp_download.o: /usr/include/bits/sigcontext.h
ccd_dsp_download.o: /usr/include/asm/sigcontext.h
ccd_dsp_download.o: /usr/include/bits/sigstack.h
ccd_dsp_download.o: /usr/include/bits/sigthread.h /usr/include/stdio.h
ccd_dsp_download.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_dsp_download.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_dsp_download.o: /usr/include/gconv.h
ccd_dsp_download.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_dsp_download.o: /usr/include/bits/stdio_lim.h
ccd_dsp_download.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
ccd_dsp_download.o: /usr/include/alloca.h /usr/include/string.h
ccd_dsp_download.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
ccd_dsp_download.o: /usr/include/bits/confname.h /usr/include/getopt.h
ccd_dsp_download.o: /home/dev/src/log_udp/include/log_udp.h
ccd_dsp_download.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_dsp_download.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_dsp_download.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_dsp_download.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_temperature.o: /usr/include/stdio.h /usr/include/features.h
ccd_temperature.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_temperature.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_temperature.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_temperature.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ccd_temperature.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_temperature.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_temperature.o: /usr/include/gconv.h
ccd_temperature.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_temperature.o: /usr/include/bits/stdio_lim.h
ccd_temperature.o: /usr/include/bits/sys_errlist.h /usr/include/string.h
ccd_temperature.o: /usr/include/math.h /usr/include/bits/huge_val.h
ccd_temperature.o: /usr/include/bits/mathdef.h /usr/include/bits/mathcalls.h
ccd_temperature.o: /usr/include/time.h
ccd_temperature.o: /home/dev/src/log_udp/include/log_udp.h
ccd_temperature.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_temperature.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_temperature.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_temperature.o: /home/dev/src/o/ccd/include/ccd_temperature.h
ccd_setup.o: /usr/include/stdio.h /usr/include/features.h
ccd_setup.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_setup.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_setup.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_setup.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ccd_setup.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_setup.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_setup.o: /usr/include/gconv.h
ccd_setup.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_setup.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ccd_setup.o: /usr/include/string.h /usr/include/fcntl.h
ccd_setup.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
ccd_setup.o: /usr/include/time.h /usr/include/endian.h
ccd_setup.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ccd_setup.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ccd_setup.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
ccd_setup.o: /usr/include/bits/pthreadtypes.h /usr/include/signal.h
ccd_setup.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
ccd_setup.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
ccd_setup.o: /usr/include/asm/sigcontext.h /usr/include/bits/sigstack.h
ccd_setup.o: /usr/include/bits/sigthread.h /usr/include/errno.h
ccd_setup.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_setup.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_setup.o: /usr/include/asm-generic/errno-base.h /usr/include/math.h
ccd_setup.o: /usr/include/bits/huge_val.h /usr/include/bits/mathdef.h
ccd_setup.o: /usr/include/bits/mathcalls.h /usr/include/unistd.h
ccd_setup.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
ccd_setup.o: /usr/include/getopt.h /home/dev/src/log_udp/include/log_udp.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_interface_private.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_exposure_private.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_setup_private.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_setup.o: /home/dev/src/o/ccd/include/ccd_temperature.h
ccd_exposure.o: /usr/include/stdio.h /usr/include/features.h
ccd_exposure.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_exposure.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_exposure.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_exposure.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ccd_exposure.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_exposure.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_exposure.o: /usr/include/gconv.h
ccd_exposure.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_exposure.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
ccd_exposure.o: /usr/include/stdlib.h /usr/include/sys/types.h
ccd_exposure.o: /usr/include/time.h /usr/include/endian.h
ccd_exposure.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ccd_exposure.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ccd_exposure.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
ccd_exposure.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
ccd_exposure.o: /usr/include/string.h /usr/include/fcntl.h
ccd_exposure.o: /usr/include/bits/fcntl.h /usr/include/signal.h
ccd_exposure.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
ccd_exposure.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
ccd_exposure.o: /usr/include/asm/sigcontext.h /usr/include/bits/sigstack.h
ccd_exposure.o: /usr/include/bits/sigthread.h /usr/include/errno.h
ccd_exposure.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
ccd_exposure.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
ccd_exposure.o: /usr/include/asm-generic/errno-base.h /usr/include/unistd.h
ccd_exposure.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
ccd_exposure.o: /usr/include/getopt.h /home/dev/src/log_udp/include/log_udp.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_exposure_private.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_interface_private.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_setup_private.h
ccd_exposure.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_exposure.o: /home/dev/src/cfitsio3200/include/fitsio.h
ccd_exposure.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/limits.h
ccd_exposure.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/syslimits.h
ccd_exposure.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
ccd_exposure.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
ccd_exposure.o: /usr/include/bits/posix2_lim.h
ccd_exposure.o: /home/dev/src/cfitsio3200/include/longnam.h
ccd_exposure.o: /home/dev/src/ngatastro/include/ngat_astro.h
ccd_exposure.o: /home/dev/src/ngatastro/include/ngat_astro_mjd.h
ccd_filter_wheel.o: /usr/include/stdio.h /usr/include/features.h
ccd_filter_wheel.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
ccd_filter_wheel.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ccd_filter_wheel.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ccd_filter_wheel.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
ccd_filter_wheel.o: /usr/include/libio.h /usr/include/_G_config.h
ccd_filter_wheel.o: /usr/include/wchar.h /usr/include/bits/wchar.h
ccd_filter_wheel.o: /usr/include/gconv.h
ccd_filter_wheel.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ccd_filter_wheel.o: /usr/include/bits/stdio_lim.h
ccd_filter_wheel.o: /usr/include/bits/sys_errlist.h /usr/include/string.h
ccd_filter_wheel.o: /usr/include/math.h /usr/include/bits/huge_val.h
ccd_filter_wheel.o: /usr/include/bits/mathdef.h /usr/include/bits/mathcalls.h
ccd_filter_wheel.o: /usr/include/time.h /usr/include/sys/time.h
ccd_filter_wheel.o: /usr/include/bits/time.h /usr/include/sys/select.h
ccd_filter_wheel.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
ccd_filter_wheel.o: /home/dev/src/log_udp/include/log_udp.h
ccd_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_filter_wheel.h
ngat_o_ccd_CCDLibrary.o: /usr/include/stdio.h /usr/include/features.h
ngat_o_ccd_CCDLibrary.o: /usr/include/sys/cdefs.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/wordsize.h
ngat_o_ccd_CCDLibrary.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
ngat_o_ccd_CCDLibrary.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stddef.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/types.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/typesizes.h /usr/include/libio.h
ngat_o_ccd_CCDLibrary.o: /usr/include/_G_config.h /usr/include/wchar.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/wchar.h /usr/include/gconv.h
ngat_o_ccd_CCDLibrary.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/stdarg.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/stdio_lim.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/sys_errlist.h
ngat_o_ccd_CCDLibrary.o: /usr/include/stdlib.h /usr/include/sys/types.h
ngat_o_ccd_CCDLibrary.o: /usr/include/time.h /usr/include/endian.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/endian.h /usr/include/sys/select.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/select.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
ngat_o_ccd_CCDLibrary.o: /usr/include/sys/sysmacros.h
ngat_o_ccd_CCDLibrary.o: /usr/include/bits/pthreadtypes.h
ngat_o_ccd_CCDLibrary.o: /usr/include/alloca.h /usr/include/string.h
ngat_o_ccd_CCDLibrary.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/jni.h
ngat_o_ccd_CCDLibrary.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/jni_md.h
ngat_o_ccd_CCDLibrary.o: /usr/lib/gcc/i386-redhat-linux/4.1.2/include/gcj/libgcj-config.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_global.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_interface.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_exposure.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_filter_wheel.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_pci.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_setup.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_temperature.h
ngat_o_ccd_CCDLibrary.o: /home/dev/src/o/ccd/include/ccd_text.h
