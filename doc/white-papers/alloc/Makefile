LATEX = latex
DOC = alloc
TEXFILES = alloc.tex
FIGFILES = imp-overview.fig freelist-array.fig freeremote-array.fig \
	allocregions.fig
IDRAW =
EPS = imp-overview.eps freelist-array.eps freeremote-array.eps \
	allocregions.eps

all: $(DOC).ps

gv: all
	gv $(DOC)

dvi: $(DOC).dvi

doall:
	make clean
	make $(DOC).dvi
	echo -bibtex $(DOC)
	make remake
	make remake

remake: removedvi all

removedvi:
	-rm -f $(DOC).dvi

#$(DOC).dvi: $(DOC).bbl ${TEXFILES}
$(DOC).dvi: ${TEXFILES} ${IDRAW} ${EPS}

.SUFFIXES: .aux .bib .bbl .tex .ps .dvi .eps .fig

#.bib.bbl:
#	$(LATEX) $*.tex
#	-bibtex $*
#	$(LATEX) $*.tex

.tex.dvi:
	$(LATEX) $*.tex

.dvi.ps:
	dvips $* -o $*.ps

.fig.eps:
	fig2dev -L eps $*.fig $*.eps

clean:
	-rm -f $(DOC).aux $(DOC).dvi $(DOC).log $(DOC).ps

cleanall: clean
	-rm -f $(DOC).bbl $(DOC).blg
