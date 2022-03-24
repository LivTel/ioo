# $Header: /space/home/eng/cjm/cvs/ioo/docs/Makefile,v 1.1 2022-03-21 17:04:54 cjm Exp $

include ../../Makefile.common
include ../Makefile.common

DOCSDIR 	= $(O_DOC_HOME)/latex

TEXSRCS 	= o_shutter_timing.tex
FIGSRCS 	= o_shutter_delays.fig

DVIOBJS		= $(TEXSRCS:%.tex=%.dvi)
PSOBJS		= $(TEXSRCS:%.tex=$(DOCSDIR)/%.ps)

EPSOBJS		= $(FIGSRCS:%.fig=%.eps)

TXTSRCS		= 
TXTOBJS		= $(TXTSRCS:%.txt=$(DOCSDIR)/%.txt)

DIRS 		= 

top: $(PSOBJS) $(TXTOBJS)
	@for i in $(DIRS); \
	do \
		(echo making in $$i...; cd $$i; $(MAKE) ); \
	done;

$(DOCSDIR)/%.ps: %.dvi
	$(DVIPS) $< -o
	$(MV) $*.ps $(DOCSDIR)

$(DVIOBJS): $(EPSOBJS)

%.dvi: %.tex
	$(LATEX) $<
	$(LATEX) $<
	$(LATEX) $<

%.eps: %.fig
	$(FIG2DEV) $(FIG2DEV_OPTIONS) $< $@

$(DOCSDIR)/%.txt: %.txt
	$(CP) $< $(DOCSDIR)
	$(CHMOD) ug+w $@

depend:
	echo "NO depend rule for latex files."

clean:
	$(RM) $(RM_OPTIONS) $(DVIOBJS) $(PSOBJS) $(EPSOBJS) $(TXTOBJS) $(LATEX_TIDY_OPTIONS) $(TIDY_OPTIONS)
	@for i in $(DIRS); \
	do \
		(echo cleaning in $$i...; cd $$i; $(MAKE) clean); \
	done;

tidy:
	-$(RM) $(RM_OPTIONS) $(LATEX_TIDY_OPTIONS) $(TIDY_OPTIONS)
	@for i in $(DIRS); \
	do \
		(echo tidying in $$i...; cd $$i; $(MAKE) tidy); \
	done;

backup: tidy
	-$(RM) $(RM_OPTIONS) $(DVIOBJS) $(EPSOBJS)
	@for i in $(DIRS); \
	do \
		(echo backup in $$i...; cd $$i; $(MAKE) backup); \
	done;

checkin:
	-$(CI) $(CI_OPTIONS) $(TEXSRCS) $(FIGSRCS) $(TXTSRCS)
	@for i in $(DIRS); \
	do \
		(echo checking in in $$i...; cd $$i; $(MAKE) checkin); \
	done;

checkout:
	-$(CO) $(CO_OPTIONS) $(TEXSRCS) $(FIGSRCS) $(TXTSRCS)
	@for i in $(DIRS); \
	do \
		(echo checking out in $$i...; cd $$i; $(MAKE) checkout); \
	done;

#
# $Log: not supported by cvs2svn $
#
