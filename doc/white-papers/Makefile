# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.35 2004/01/30 18:21:07 aabauman Exp $
# ############################################################################
#
# Makefile with all th rules to make html and PostScript documents
#

#
# Commands
#

#DSSSL_DIR=/usr/lib/sgml/stylesheets/nwalsh-modular
#DSSSL_DIR=/usr/local/incoming/tmp/docbook-dsssl-1.72

JADE=openjade -D$(DSSSL_DIR)
FIG2x=fig2dev		# fig translator

FIGMAG=-m 1.5
FIG2gif=$(FIG2x) -L gif $(FIGMAG)
FIG2jpg=$(FIG2x) -L jpeg $(FIGMAG)
FIG2png=$(FIG2x) -L png $(FIGMAG)
FIG2ps =$(FIG2x) -L ps

DIA2x=dia --nosplash	# dia translator

STYLE_SHEET=wp.dsl

# Uncomment if you want html output into several files. 
#HTML_VARS += -V '(define nochunks #f)'

HTML_ARGS =-t sgml -ihtml $(HTML_VARS) -d $(STYLE_SHEET)\#html

# Uncomment if you would like to get double spacing for easy editing of docs
# default spacing is 1.3
#TEX_VARS += -V '(define %line-spacing-factor% 2.5)'

TEX_ARGS =-t tex $(TEX_VARS) -d $(STYLE_SHEET)\#print

RTF_VARS=
RTF_ARGS= -t rtf $(RTF_VARS) -d print/docbook.dsl

COMMON= wp.ent wp.dsl bib.sgml

DVI_UNDFREF='^LaTeX Warning: There were undefined references.'

TEX.dvi = jadetex $< >/dev/null 2>&1 || true
RERUN   = egrep $(DVI_UNDFREF) $(<:%.tex=%.log) >/dev/null 2>&1
%.dvi: %.tex
	$(TEX.dvi)
	@$(RERUN) && $(TEX.dvi)
	@$(RERUN) && $(TEX.dvi)
	@$(RM) $(<:%.tex=%.log) $(<:%.tex=%.aux) $(<:%.tex=%.out)

# Must figure out the right way to do this.
# Jadetex emits no useful warnings yet to indicate whether or not a
# rerun is necessary, so we use brute force
%.pdf: %.tex
	pdfjadetex $< > /dev/null 2>&1 || true
	@pdfjadetex $< > /dev/null 2>&1 || true
	@pdfjadetex $< > /dev/null 2>&1 || true
	@$(RM) $(<:%.tex=%.log) $(<:%.tex=%.aux) $(<:%.tex=%.out)

%.ps: %.dvi
	dvips -o $@ $< >/dev/null 2>&1

#
# XFig rules
#
%.gif: %.fig
	$(FIG2gif) $< $@

%.jpg: %.fig
	$(FIG2jpg) $< $@

%.png: %.fig
	$(FIG2png) $< $@


#
# Dia rules
#
%.jpg %.png %.eps : %.dia
	$(DIA2x) --export=$@ $<

#
# IDraw rules (Native form is EPS compatible)
#
%.eps: %.iv
	$(CP) $< $@

%.jpg: %.iv


#
# Jade rules
#
%.html: %.sgml $(COMMON) $(FIG_HTML)
	$(JADE) $(HTML_ARGS) $< > $@ || $(RM) $@

%.tex: %.sgml $(COMMON) $(FIG_TEX)
	$(JADE) $(TEX_ARGS) -o $@ $<

%.rtf: %.sgml $(COMMON) $(FIG_TEX)
	$(JADE) $(RTF_ARGS) -o $@ $<

CO_FIGS=figures/COoop.fig		\
	figures/COabstract.fig		\
	figures/COunrolled.fig		\
	figures/COclustering.fig	\
	figures/COpolicies.fig	        \
	figures/COabstractMH.fig	\
	figures/COunrolledMH.fig 	\
	figures/COProcess.fig		\
	figures/COOTTS.fig		\
	figures/COOTTS2.fig		\
	figures/COOTTSMH.fig		\
	figures/COrepinstantiation.fig  \

FIGS =	figures/XFigSample.fig		\
	figures/DiaSample.dia		\
	figures/KernelBoundary.fig      \
	figures/SystemCalls.fig		\
	figures/Open.fig		\
	figures/Chmod-path.fig		\
	figures/Chmod-fd.fig		\
	figures/K42Struct.fig		\
	figures/FSStructure.fig		\
	figures/DirObjects.fig		\
	figures/KernSched.fig		\
	figures/MemoryMgmt.fig          \
	figures/Dispatcher.fig		\
	$(CO_FIGS)

base_FIGS= $(basename $(FIGS))
FIGS_HTML = $(base_FIGS:%=%.jpg) $(base_FIGS:%=%.png)
FIGS_TEX  = $(base_FIGS:%=%.eps) $(base_FIGS:%=%.ps)


DOCSRCS =	ClusteredObject.sgml    \
		FileSystems.sgml        \
		KitchenSink.sgml	\
		LKIntern.sgml		\
		MemoryMgmtDoc.sgml	\
		OverviewDoc.sgml        \
		PerfMon.sgml            \
		ReadMe.sgml		\
		SchedulingDoc.sgml         \
		Template.sgml		\
		UnixEnvironment.sgml

TARGETS=$(DOCSRCS:%.sgml=%)
TARGETS_HTML=$(DOCSRCS:%.sgml=%.html)
TARGETS_PS=$(DOCSRCS:%.sgml=%.ps)
TARGETS_PDF=$(DOCSRCS:%.sgml=%.pdf)
TARGETS_RTF=$(DOCSRCS:%.sgml=%.rtf)

.SECONDARY: $(DOCSRCS:%.sgml=%.tex)

all: html ps

web: LKIntern MemoryMgmtDoc OverviewDoc PerfMon Scheduling
#webpdf:	LKIntern.pdf MemoryMgmtDoc.pdf OverviewDoc.pdf PerfMon.pdf SchedulingDoc.pdf 
webpdf:	
	convert LKIntern.ps LKIntern.pdf; 		\
	convert MemoryMgmtDoc.ps MemoryMgmtDoc.pdf; 		\
	convert OverviewDoc.ps OverviewDoc.pdf; 		\
	convert PerfMon.ps PerfMon.pdf; 		\
	convert SchedulingDoc.ps SchedulingDoc.pdf

#
# List specific dependencies on a per target basis (no commands allowed)
#
KitchenSink.html: figures/XFigSample.jpg figures/DiaSample.png
KitchenSink.tex:   figures/XFigSample.eps figures/DiaSample.eps
KitchenSink.rtf:   figures/XFigSample.eps figures/DiaSample.eps

UnixEnvironment.html: figures/KernelBoundary.png figures/SystemCalls.png
UnixEnvironment.tex: figures/KernelBoundary.eps figures/SystemCalls.eps
UnixEnvironment.rtf: figures/KernelBoundary.eps figures/SystemCalls.eps

SchedulingDoc.html: figures/KernSched.jpg figures/Dispatcher.jpg
SchedulingDoc.tex:   figures/KernSched.eps figures/Dispatcher.eps
SchedulingDoc.rtf:   KernSched.eps Dispatcher.eps

MemoryMgmtDoc.html: MemoryMgmt.jpg 
MemoryMgmtDoc.tex:  MemoryMgmt.eps 
MemoryMgmtDoc.rtf:  MemoryMgmt.eps 
OverviewDoc.html: K42Struct.png Overview.sgml
OverviewDoc.tex: K42Struct.eps Overview.sgml
OverviewDoc.rtf: K42Struct.eps Overview.sgml

SchedulingDoc.ps : Scheduling.sgml

MemoryMgmt.fig: figures/MemoryMgmt.fig
	cp figures/MemoryMgmt.fig .
K42Struct.fig: figures/K42Struct.fig
	cp figures/K42Struct.fig .
KernSched.fig: figures/KernSched.fig
	cp $< .
Dispatcher.fig: figures/Dispatcher.fig
	cp $< .

ClusteredObject.html: $(CO_FIGS:%.fig=%.jpg)
ClusteredObject.tex:  $(CO_FIGS:%.fig=%.eps)
ClusteredObject.rtf:  $(CO_FIGS:%.fig=%.eps)

FILESYSTEM_PS_FIGS= figures/Chmod-fd.ps figures/Chmod-path.ps figures/Open.ps
FileSystems.tex: $(FILESYSTEM_PS_FIGS) figures/FSStructure.eps
FileSystems.rtf: $(FILESYSTEM_PS_FIGS) figures/FSStructure.eps

#Using .ps files instead of .eps because we want the figure to be in
#landscape mode
$(FILESYSTEM_PS_FIGS): %.ps: %.fig
	$(FIG2ps) -l dummy_arg $< $@

html: $(TARGETS_HTML)
ps:   $(TARGETS_PS)
pdf:  $(TARGETS_PDF)
rtf:  $(TARGETS_RTF)

$(TARGETS): %: %.html %.ps

CLEAN_FILES  = $(TARGETS_HTML) $(TARGETS_PS) $(TARGETS_PDF) $(TARGETS_RTF)
CLEAN_FILES += $(FIGS_HTML) $(FIGS_TEX)

CLEAN_TEX  = $(DOCSRCS:%.sgml=%.tex) $(DOCSRCS:%.sgml=%.dvi)
CLEAN_TEX += $(DOCSRCS:%.sgml=%.aux) $(DOCSRCS:%.sgml=%.log)
CLEAN_TEX += $(DOCSRCS:%.sgml=%.out)

clean_tex:
	$(RM) $(CLEAN_TEX)

clean: clean_tex
	$(RM) $(CLEAN_FILES)

clobber: clean
	$(RM) *~ figures/*.bak

topsrc=../..
include $(topsrc)/doc/Makefile.doc
