# Makefile
# $Header: /space/home/eng/cjm/cvs/ioo/java/ngat/o/test/Makefile,v 1.1 2011-11-23 10:59:43 cjm Exp $
include ../../../../../Makefile.common
include	../../../../Makefile.common

PACKAGEDIR	= ngat/o/test
BINDIR 		= $(O_BIN_HOME)/java/$(PACKAGEDIR)
DOCSDIR 	= $(O_DOC_HOME)/javadocs/$(PACKAGEDIR)
DOCFLAGS 	= -version -author -private

SRCS 		= SicfTCPClientConnectionThread.java SicfTCPServerConnectionThread.java SicfTCPServer.java \
		SendBiasCommand.java SendConfigCommand.java SendDarkCommand.java \
		SendMultBiasCommand.java SendMultDarkCommand.java SendMultrunCommand.java SendGetStatusCommand.java
OBJS 		= $(SRCS:%.java=$(BINDIR)/%.class)
DOCS 		= $(SRCS:%.java=$(DOCSDIR)/%.html)

top: $(OBJS)

$(BINDIR)/%.class:%.java
	$(JAVAC) -sourcepath .:/home/dev/src:/home/dev/src/o/java \
		-classpath $(O_BIN_HOME)/java:$(CLASSPATH) $(JAVAC_OPTIONS) -d $(O_BIN_HOME)/java $<

checkout:
	$(CO) $(CO_OPTIONS) $(SRCS)

checkin:
	-$(CI) $(CI_OPTIONS) $(SRCS)

clean: tidy
	-$(RM) $(RM_OPTIONS) $(OBJS)

tidy:
	-$(RM) $(RM_OPTIONS) $(TIDY_OPTIONS)

backup: tidy
#
# $Log: not supported by cvs2svn $
#
