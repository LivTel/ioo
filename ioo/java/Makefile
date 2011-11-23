# Makefile
# $Header: /space/home/eng/cjm/cvs/ioo/java/Makefile,v 1.1 2011-11-23 10:59:50 cjm Exp $
include ../Makefile.common
include ../../Makefile.common

MAKE 		= make
DIRS 		= ngat
JAR_FILE       	= ngat_o.jar
LIBDIR		= $(O_BIN_HOME)/java/
JARLIBDIR	= $(LT_BIN_HOME)/javalib

DOCSDIR 	= $(O_DOC_HOME)/javadocs/
DOCFLAGS 	= -version -author -private
JAVASRCS 	= ngat/o/*.java ngat/o/ccd/*.java ngat/o/ccd/test/*.java ngat/o/test/*.java 

JAVAPKGS	= ngat.o ngat.o.ccd ngat.o.ccd.test ngat.o.test 

JAVADOCS 	= $(JAVASRCS:%.java=$(DOCSDIR)/%.html)

top: dirs jar javadocs

dirs:
	@for i in $(DIRS); \
	do \
		(echo making in $$i...; cd $$i; $(MAKE) ); \
	done;

jar: $(JARLIBDIR)/$(JAR_FILE)

$(JARLIBDIR)/$(JAR_FILE): $(JAVASRCS)
	( cd $(LIBDIR); $(JAR) $(JAR_OPTIONS) $(JARLIBDIR)/$(JAR_FILE) ngat )

#
# Do all javadocs at top level, to ensure javadoc creates package links correctly
#
javadocs: $(JAVADOCS)

$(JAVADOCS): $(JAVASRCS)
	$(JAVADOC) -sourcepath /home/dev/src:/home/dev/src/o/java -d $(DOCSDIR) $(DOCFLAGS) \
		-link http://download.oracle.com/javase/1.4.2/docs/api/ $(JAVAPKGS)
#Was: http://java.sun.com/j2se/1.4.2/docs/api/
#Now: http://download.oracle.com/javase/1.4.2/docs/api/

checkin:
	-@for i in $(DIRS); \
	do \
		(echo checkin in $$i...; cd $$i; $(MAKE) checkin; $(CI) $(CI_OPTIONS) Makefile); \
	done;

checkout:
	@for i in $(DIRS); \
	do \
		(echo checkout in $$i...; cd $$i; $(CO) $(CO_OPTIONS) Makefile; $(MAKE) checkout); \
	done;

depend:
	echo no depend in ngat directory.

clean:
	-$(RM) $(RM_OPTIONS) $(TIDY_OPTIONS)
	-$(RM) $(RM_OPTIONS) $(JARLIBDIR)/$(JAR_FILE)
	@for i in $(DIRS); \
	do \
		(echo clean in $$i...; cd $$i; $(MAKE) clean); \
	done;
	$(RM) $(RM_OPTIONS) $(JAVADOCS)
	$(RM) $(RM_OPTIONS) -r $(DOCSDIR)/*

tidy:
	-$(RM) $(RM_OPTIONS) $(TIDY_OPTIONS)
	@for i in $(DIRS); \
	do \
		(echo tidy in $$i...; cd $$i; $(MAKE) tidy); \
	done;

backup: checkin
	@for i in $(DIRS); \
	do \
		(echo backup in $$i...; cd $$i; $(MAKE) backup); \
	done;

#
# $Log: not supported by cvs2svn $
#
