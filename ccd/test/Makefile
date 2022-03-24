# $Header: /space/home/eng/cjm/cvs/ioo/ccd/test/Makefile,v 1.1 2012-05-29 13:41:04 cjm Exp $

include ../../../Makefile.common
include ../../Makefile.common
include ../Makefile.common

INCDIR 		= $(O_CCD_SRC_HOME)/include
TESTDIR 	= test
BINDIR 		= $(O_CCD_BIN_HOME)/$(TESTDIR)/$(HOSTTYPE)
DOCSDIR 	= $(O_CCD_DOC_HOME)/$(TESTDIR)

# for using estar_config (test_generate_waveform.c)
CONFIG_CFLAGS	=-I$(eSTAR_SRC_HOME)/config/include -I$(eSTAR_SRC_HOME)/common/include

CFLAGS 		= -g -I$(INCDIR) -I$(CFITSIOINCDIR) -I$(JNIINCDIR) -I$(JNIMDINCDIR) $(CONFIG_CFLAGS) $(LOG_UDP_CFLAGS)
DOCFLAGS 	= -static

SRCS 		= ccd_read_memory.c ccd_write_memory.c test_dsp_download.c test_reset_controller.c \
			test_data_link.c test_data_link_multi.c test_analogue_power.c test_gain.c \
			test_setup_startup.c test_setup_shutdown.c \
			test_exposure.c test_shutter.c test_generate_waveform.c test_idle_clocking.c \
			test_manual_command.c test_filter_wheel.c test_temperature.c

OBJS 		= $(SRCS:%.c=%.o)
PROGS 		= $(SRCS:%.c=$(BINDIR)/%)
DOCS 		= $(SRCS:%.c=$(DOCSDIR)/%.html)
#SCRIPT_SRCS	= 
#SCRIPT_BINS	= $(SCRIPT_SRCS:%=$(BINDIR)/%)
CONFIG_SRCS	= e2v231-84.config bif486.config hamamatsu-s9970.config
CONFIG_BINS	= $(CONFIG_SRCS:%=$(BINDIR)/%)
LIBNAME		= $(O_HOME)_$(O_CCD_HOME)
top: $(PROGS) configs docs
#scripts

$(BINDIR)/ccd_read_memory: ccd_read_memory.o
	cc -o $@ ccd_read_memory.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/ccd_write_memory: ccd_write_memory.o
	cc -o $@ ccd_write_memory.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_dsp_download: test_dsp_download.o
	cc -o $@ test_dsp_download.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_reset_controller: test_reset_controller.o
	cc -o $@ test_reset_controller.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_data_link: test_data_link.o
	cc -o $@ test_data_link.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_data_link_multi: test_data_link_multi.o
	cc -o $@ test_data_link_multi.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_analogue_power: test_analogue_power.o
	cc -o $@ test_analogue_power.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_gain: test_gain.o
	cc -o $@ test_gain.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc
$(BINDIR)/test_temperature: test_temperature.o
	cc -o $@ test_temperature.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_setup_startup: test_setup_startup.o
	cc -o $@ test_setup_startup.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_setup_shutdown: test_setup_shutdown.o
	cc -o $@ test_setup_shutdown.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_exposure: test_exposure.o
	cc -o $@ test_exposure.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_filter_wheel: test_filter_wheel.o
	cc -o $@ test_filter_wheel.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_shutter: test_shutter.o
	cc -o $@ test_shutter.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_generate_waveform: test_generate_waveform.o
	cc -o $@ test_generate_waveform.o -L$(LT_LIB_HOME) -L$(eSTAR_LIB_HOME) -l$(LIBNAME) -lcfitsio -lestar_config $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_idle_clocking: test_idle_clocking.o
	cc -o $@ test_idle_clocking.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

$(BINDIR)/test_manual_command: test_manual_command.o
	cc -o $@ test_manual_command.o -L$(LT_LIB_HOME) -l$(LIBNAME) -lcfitsio $(TIMELIB) $(SOCKETLIB) -lm -lc

scripts: $(SCRIPT_BINS)

$(BINDIR)/%.csh:%.csh
	$(CP) $< $@

configs: $(CONFIG_BINS)

$(BINDIR)/%.config:%.config
	$(CP) $< $@

$(BINDIR)/%:%
	$(CP) $< $@

docs: $(DOCS)

$(DOCS): $(SRCS)
	-$(CDOC) -d $(DOCSDIR) -h $(INCDIR) $(DOCFLAGS) $(SRCS)

depend:
	makedepend $(MAKEDEPENDFLAGS) -- $(CFLAGS) -- $(SRCS)

clean:
	$(RM) $(RM_OPTIONS) $(OBJS) $(TIDY_OPTIONS)

tidy:
	$(RM) $(RM_OPTIONS) $(TIDY_OPTIONS)

backup: tidy
	$(RM) $(RM_OPTIONS) $(OBJS)

checkin:
	-$(CI) $(CI_OPTIONS) $(SRCS)

checkout:
	-$(CO) $(CO_OPTIONS) $(SRCS)

# DO NOT DELETE

ccd_read_memory.o: /usr/include/stdio.h /usr/include/features.h
ccd_read_memory.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
ccd_read_memory.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
ccd_read_memory.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
ccd_read_memory.o: /usr/include/bits/typesizes.h /usr/include/libio.h
ccd_read_memory.o: /usr/include/_G_config.h /usr/include/wchar.h
ccd_read_memory.o: /usr/include/bits/wchar.h /usr/include/gconv.h
ccd_read_memory.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
ccd_read_memory.o: /usr/include/bits/stdio_lim.h
ccd_read_memory.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
ccd_read_memory.o: /usr/include/string.h /usr/include/time.h
ccd_read_memory.o: /usr/include/bits/time.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_read_memory.o: /usr/include/sys/types.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_temperature.h
ccd_read_memory.o: /home/dev/src/o/ccd/include/ccd_setup.h
ccd_write_memory.o: /usr/include/stdio.h /usr/include/features.h
ccd_write_memory.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
ccd_write_memory.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
ccd_write_memory.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
ccd_write_memory.o: /usr/include/bits/typesizes.h /usr/include/libio.h
ccd_write_memory.o: /usr/include/_G_config.h /usr/include/wchar.h
ccd_write_memory.o: /usr/include/bits/wchar.h /usr/include/gconv.h
ccd_write_memory.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
ccd_write_memory.o: /usr/include/bits/stdio_lim.h
ccd_write_memory.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
ccd_write_memory.o: /usr/include/string.h /usr/include/time.h
ccd_write_memory.o: /usr/include/bits/time.h
ccd_write_memory.o: /home/dev/src/o/ccd/include/ccd_dsp.h
ccd_write_memory.o: /home/dev/src/o/ccd/include/ccd_interface.h
ccd_write_memory.o: /home/dev/src/o/ccd/include/ccd_global.h
ccd_write_memory.o: /home/dev/src/o/ccd/include/ccd_pci.h
ccd_write_memory.o: /home/dev/src/o/ccd/include/ccd_text.h
ccd_write_memory.o: /usr/include/sys/types.h
test_dsp_download.o: /usr/include/stdio.h /usr/include/features.h
test_dsp_download.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_dsp_download.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_dsp_download.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_dsp_download.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_dsp_download.o: /usr/include/_G_config.h /usr/include/wchar.h
test_dsp_download.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_dsp_download.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_dsp_download.o: /usr/include/bits/stdio_lim.h
test_dsp_download.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_dsp_download.o: /usr/include/string.h /usr/include/time.h
test_dsp_download.o: /usr/include/bits/time.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_global.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_text.h
test_dsp_download.o: /usr/include/sys/types.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_dsp_download.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_reset_controller.o: /usr/include/stdio.h /usr/include/features.h
test_reset_controller.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_reset_controller.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_reset_controller.o: /usr/include/bits/types.h
test_reset_controller.o: /usr/include/bits/wordsize.h
test_reset_controller.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_reset_controller.o: /usr/include/_G_config.h /usr/include/wchar.h
test_reset_controller.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_reset_controller.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_reset_controller.o: /usr/include/bits/stdio_lim.h
test_reset_controller.o: /usr/include/bits/sys_errlist.h
test_reset_controller.o: /usr/include/stdlib.h /usr/include/string.h
test_reset_controller.o: /usr/include/time.h /usr/include/bits/time.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_global.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_text.h
test_reset_controller.o: /usr/include/sys/types.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_reset_controller.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_data_link.o: /usr/include/stdio.h /usr/include/features.h
test_data_link.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_data_link.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_data_link.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_data_link.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_data_link.o: /usr/include/_G_config.h /usr/include/wchar.h
test_data_link.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_data_link.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_data_link.o: /usr/include/bits/stdio_lim.h
test_data_link.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_data_link.o: /usr/include/string.h /usr/include/time.h
test_data_link.o: /usr/include/bits/time.h
test_data_link.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_data_link.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_data_link.o: /home/dev/src/o/ccd/include/ccd_global.h
test_data_link.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_data_link.o: /home/dev/src/o/ccd/include/ccd_text.h
test_data_link.o: /usr/include/sys/types.h
test_data_link_multi.o: /usr/include/stdio.h /usr/include/features.h
test_data_link_multi.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_data_link_multi.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_data_link_multi.o: /usr/include/bits/types.h
test_data_link_multi.o: /usr/include/bits/wordsize.h
test_data_link_multi.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_data_link_multi.o: /usr/include/_G_config.h /usr/include/wchar.h
test_data_link_multi.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_data_link_multi.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_data_link_multi.o: /usr/include/bits/stdio_lim.h
test_data_link_multi.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_data_link_multi.o: /usr/include/string.h /usr/include/time.h
test_data_link_multi.o: /usr/include/bits/time.h
test_data_link_multi.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_data_link_multi.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_data_link_multi.o: /home/dev/src/o/ccd/include/ccd_global.h
test_data_link_multi.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_data_link_multi.o: /home/dev/src/o/ccd/include/ccd_text.h
test_data_link_multi.o: /usr/include/sys/types.h
test_analogue_power.o: /usr/include/stdio.h /usr/include/features.h
test_analogue_power.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_analogue_power.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_analogue_power.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_analogue_power.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_analogue_power.o: /usr/include/_G_config.h /usr/include/wchar.h
test_analogue_power.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_analogue_power.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_analogue_power.o: /usr/include/bits/stdio_lim.h
test_analogue_power.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_analogue_power.o: /usr/include/string.h /usr/include/time.h
test_analogue_power.o: /usr/include/bits/time.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_global.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_text.h
test_analogue_power.o: /usr/include/sys/types.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_analogue_power.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_setup_startup.o: /usr/include/stdio.h /usr/include/features.h
test_setup_startup.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_setup_startup.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_setup_startup.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_setup_startup.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_setup_startup.o: /usr/include/_G_config.h /usr/include/wchar.h
test_setup_startup.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_setup_startup.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_setup_startup.o: /usr/include/bits/stdio_lim.h
test_setup_startup.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_setup_startup.o: /usr/include/string.h /usr/include/time.h
test_setup_startup.o: /usr/include/bits/time.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_global.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_text.h
test_setup_startup.o: /usr/include/sys/types.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_setup_startup.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_setup_shutdown.o: /usr/include/stdio.h /usr/include/features.h
test_setup_shutdown.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_setup_shutdown.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_setup_shutdown.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_setup_shutdown.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_setup_shutdown.o: /usr/include/_G_config.h /usr/include/wchar.h
test_setup_shutdown.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_setup_shutdown.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_setup_shutdown.o: /usr/include/bits/stdio_lim.h
test_setup_shutdown.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_setup_shutdown.o: /usr/include/string.h /usr/include/time.h
test_setup_shutdown.o: /usr/include/bits/time.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_global.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_text.h
test_setup_shutdown.o: /usr/include/sys/types.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_setup_shutdown.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_exposure.o: /usr/include/stdio.h /usr/include/features.h
test_exposure.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_exposure.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_exposure.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_exposure.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_exposure.o: /usr/include/_G_config.h /usr/include/wchar.h
test_exposure.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_exposure.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_exposure.o: /usr/include/bits/stdio_lim.h
test_exposure.o: /usr/include/bits/sys_errlist.h /usr/include/string.h
test_exposure.o: /usr/include/time.h /usr/include/bits/time.h
test_exposure.o: /home/dev/src/log_udp/include/log_udp.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_global.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_text.h
test_exposure.o: /usr/include/sys/types.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_exposure.o: /home/dev/src/o/ccd/include/ccd_exposure.h
test_exposure.o: /home/dev/src/cfitsio3200/include/fitsio.h
test_exposure.o: /usr/include/stdlib.h /usr/include/limits.h
test_exposure.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/limits.h
test_exposure.o: /home/dev/src/cfitsio3200/include/longnam.h
test_shutter.o: /usr/include/stdio.h /usr/include/features.h
test_shutter.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_shutter.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_shutter.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_shutter.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_shutter.o: /usr/include/_G_config.h /usr/include/wchar.h
test_shutter.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_shutter.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_shutter.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
test_shutter.o: /usr/include/stdlib.h /usr/include/string.h
test_shutter.o: /usr/include/time.h /usr/include/bits/time.h
test_shutter.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_shutter.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_shutter.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_shutter.o: /home/dev/src/o/ccd/include/ccd_global.h
test_shutter.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_shutter.o: /home/dev/src/o/ccd/include/ccd_text.h
test_shutter.o: /usr/include/sys/types.h
test_generate_waveform.o: /usr/include/stdio.h /usr/include/features.h
test_generate_waveform.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_generate_waveform.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_generate_waveform.o: /usr/include/bits/types.h
test_generate_waveform.o: /usr/include/bits/wordsize.h
test_generate_waveform.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_generate_waveform.o: /usr/include/_G_config.h /usr/include/wchar.h
test_generate_waveform.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_generate_waveform.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_generate_waveform.o: /usr/include/bits/stdio_lim.h
test_generate_waveform.o: /usr/include/bits/sys_errlist.h
test_generate_waveform.o: /usr/include/stdlib.h /usr/include/string.h
test_generate_waveform.o: /usr/include/time.h /usr/include/bits/time.h
test_generate_waveform.o: /home/dev/src/estar/common/include/estar_common.h
test_generate_waveform.o: /usr/include/limits.h
test_generate_waveform.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/limits.h
test_generate_waveform.o: /home/dev/src/estar/config/include/estar_config.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_global.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_text.h
test_generate_waveform.o: /usr/include/sys/types.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_generate_waveform.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_idle_clocking.o: /usr/include/stdio.h /usr/include/features.h
test_idle_clocking.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_idle_clocking.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_idle_clocking.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_idle_clocking.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_idle_clocking.o: /usr/include/_G_config.h /usr/include/wchar.h
test_idle_clocking.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_idle_clocking.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_idle_clocking.o: /usr/include/bits/stdio_lim.h
test_idle_clocking.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_idle_clocking.o: /usr/include/string.h /usr/include/time.h
test_idle_clocking.o: /usr/include/bits/time.h
test_idle_clocking.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_idle_clocking.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_idle_clocking.o: /home/dev/src/o/ccd/include/ccd_global.h
test_idle_clocking.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_idle_clocking.o: /home/dev/src/o/ccd/include/ccd_text.h
test_idle_clocking.o: /usr/include/sys/types.h
test_manual_command.o: /usr/include/stdio.h /usr/include/features.h
test_manual_command.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_manual_command.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_manual_command.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_manual_command.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_manual_command.o: /usr/include/_G_config.h /usr/include/wchar.h
test_manual_command.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_manual_command.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_manual_command.o: /usr/include/bits/stdio_lim.h
test_manual_command.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_manual_command.o: /usr/include/string.h /usr/include/time.h
test_manual_command.o: /usr/include/bits/time.h
test_manual_command.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_manual_command.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_manual_command.o: /home/dev/src/o/ccd/include/ccd_global.h
test_manual_command.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_manual_command.o: /home/dev/src/o/ccd/include/ccd_text.h
test_manual_command.o: /usr/include/sys/types.h
test_filter_wheel.o: /usr/include/stdio.h /usr/include/features.h
test_filter_wheel.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_filter_wheel.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_filter_wheel.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_filter_wheel.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_filter_wheel.o: /usr/include/_G_config.h /usr/include/wchar.h
test_filter_wheel.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_filter_wheel.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_filter_wheel.o: /usr/include/bits/stdio_lim.h
test_filter_wheel.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_filter_wheel.o: /usr/include/string.h /usr/include/time.h
test_filter_wheel.o: /usr/include/bits/time.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_global.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_text.h
test_filter_wheel.o: /usr/include/sys/types.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_filter_wheel.h
test_filter_wheel.o: /home/dev/src/o/ccd/include/ccd_setup.h
test_temperature.o: /usr/include/stdio.h /usr/include/features.h
test_temperature.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_temperature.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stddef.h
test_temperature.o: /usr/include/bits/types.h /usr/include/bits/wordsize.h
test_temperature.o: /usr/include/bits/typesizes.h /usr/include/libio.h
test_temperature.o: /usr/include/_G_config.h /usr/include/wchar.h
test_temperature.o: /usr/include/bits/wchar.h /usr/include/gconv.h
test_temperature.o: /usr/lib/gcc-lib/i386-redhat-linux/3.2.2/include/stdarg.h
test_temperature.o: /usr/include/bits/stdio_lim.h
test_temperature.o: /usr/include/bits/sys_errlist.h /usr/include/stdlib.h
test_temperature.o: /usr/include/string.h /usr/include/time.h
test_temperature.o: /usr/include/bits/time.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_dsp.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_interface.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_dsp_download.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_global.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_pci.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_text.h
test_temperature.o: /usr/include/sys/types.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_temperature.h
test_temperature.o: /home/dev/src/o/ccd/include/ccd_setup.h
