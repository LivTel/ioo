# Makefile
# $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/Makefile,v 1.4 2014-10-02 13:31:50 cjm Exp $
include ../../../Makefile.common
include ../../../../Makefile.common

MAKE 		= make
PACKAGEDIR	= ngat/o/
BINDIR 		= $(O_BIN_HOME)/java/$(PACKAGEDIR)
CONFIG_BINDIR 	= $(O_BIN_HOME)/java
DOCSDIR 	= $(O_DOC_HOME)/javadocs/$(PACKAGEDIR)
DOCFLAGS 	= -version -author -private
SRCS 		= $(MAIN_SRCS) $(IMPL_SRCS)
MAIN_SRCS 	= OConstants.java OStatus.java OTCPServer.java OTCPServerConnectionThread.java \
		OTCPClientConnectionThread.java O.java OREBOOTQuitThread.java
IMPL_SRCS = $(BASE_IMPL_SRCS) $(CALIBRATE_IMPL_SRCS) $(EXPOSE_IMPL_SRCS) $(INTERRUPT_IMPL_SRCS) $(SETUP_IMPL_SRCS)
BASE_IMPL_SRCS	= JMSCommandImplementation.java CommandImplementation.java UnknownCommandImplementation.java \
		HardwareImplementation.java FITSImplementation.java \
		ACQUIREImplementation.java ACQUIREImplementationTweak.java  \
		CALIBRATEImplementation.java EXPOSEImplementation.java INTERRUPTImplementation.java \
		SETUPImplementation.java
CALIBRATE_IMPL_SRCS = BIASImplementation.java DARKImplementation.java DAY_CALIBRATEImplementation.java \
		MULTBIASImplementation.java MULTDARKImplementation.java TWILIGHT_CALIBRATEImplementation.java \
		TWILIGHT_CALIBRATEImplementation2.java 
EXPOSE_IMPL_SRCS    = GLANCEImplementation.java MULTRUNImplementation.java SAVEImplementation.java
INTERRUPT_IMPL_SRCS = ABORTImplementation.java GET_STATUSImplementation.java REBOOTImplementation.java 
SETUP_IMPL_SRCS	    = CONFIGImplementation.java TELFOCUSImplementation.java 
OBJS 		= $(SRCS:%.java=$(BINDIR)/%.class)
DOCS 		= $(SRCS:%.java=$(DOCSDIR)/%.html)

CONFIG_SRCS	= o1.o.net.properties o1.o.properties o1.fits.properties
CONFIG_OBJS	= $(CONFIG_SRCS:%.properties=$(CONFIG_BINDIR)/%.properties)

DIRS 		= ccd ndfilter test
 
top: $(OBJS) config
	@for i in $(DIRS); \
	do \
		(echo making in $$i...; cd $$i; $(MAKE) ); \
	done;

$(BINDIR)/%.class:%.java
	$(JAVAC) -sourcepath .:/home/dev/src/o/java -classpath $(O_BIN_HOME)/java:$(CLASSPATH) $(JAVAC_OPTIONS) \
		-d $(O_BIN_HOME)/java $<

config: $(CONFIG_OBJS)

$(CONFIG_BINDIR)/%.properties: %.properties
	$(CP) $< $@

checkin:
	-$(CI) $(CI_OPTIONS) $(SRCS) $(CONFIG_SRCS)
	-@for i in $(DIRS); \
	do \
		(echo checkin in $$i...; cd $$i; $(MAKE) checkin; $(CI) $(CI_OPTIONS) Makefile); \
	done;

checkout:
	$(CO) $(CO_OPTIONS) $(SRCS) $(CONFIG_SRCS)
	@for i in $(DIRS); \
	do \
		(echo checkout in $$i...; cd $$i; $(CO) $(CO_OPTIONS) Makefile; $(MAKE) checkout); \
	done;

depend:
	echo no depend in ngat directory.

clean: tidy
	-$(RM) $(RM_OPTIONS) $(OBJS)
	@for i in $(DIRS); \
	do \
		(echo clean in $$i...; cd $$i; $(MAKE) clean); \
	done;

tidy:
	-$(RM) $(RM_OPTIONS) $(TIDY_OPTIONS)
	@for i in $(DIRS); \
	do \
		(echo tidy in $$i...; cd $$i; $(MAKE) tidy); \
	done;

backup: tidy checkin
	@for i in $(DIRS); \
	do \
		(echo backup in $$i...; cd $$i; $(MAKE) backup); \
	done;

#
# $Log: not supported by cvs2svn $
# Revision 1.3  2013/05/21 09:21:29  cjm
# Added ndfilter.
#
# Revision 1.2  2012/10/12 14:30:19  cjm
# Added TWILIGHT_CALIBRATEImplementation2.java .
#
# Revision 1.1  2011/11/23 10:59:46  cjm
# Initial revision
#
#
