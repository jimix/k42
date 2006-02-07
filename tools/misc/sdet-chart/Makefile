# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# The purpose of this makefile is to automate the process of generating
# performance charts from our nightly SDET runs.  If you do not want to
# have to type in your password when fetching the raw data, put your
# ssh public key on the remote host.
#
#  $Id: Makefile,v 1.2 2005/04/14 21:09:53 apw Exp $
# ############################################################################

DATA = sdet1_results-noDeb sdet4_results-noDeb
TRIM = sdet1_results-noDeb.dat sdet4_results-noDeb.dat
PLOT = sdet-k4-all.png sdet-k4-trim.png sdet-k4-trim-4way.png
USER = kitchawa
HOST = klinux1.watson.ibm.com
PLAC = /home/kitchawa/knightly/build/hardware-regress
VICT = k4
V = 0

ifneq ("$(V)", "1")
.SILENT:
endif

all: $(PLOT)

sdet-k4-all.png: $(TRIM) sdet-k4-all.gp
	@echo "GNUPLOT  $@"
	gnuplot < sdet-k4-all.gp

sdet-k4-trim.png: $(TRIM) sdet-k4-trim.gp
	@echo "GNUPLOT  $@"
	gnuplot < sdet-k4-trim.gp

sdet-k4-trim-4way.png: $(TRIM) sdet-k4-trim-4way.gp
	@echo "GNUPLOT  $@"
	gnuplot < sdet-k4-trim-4way.gp


sdet1_results-noDeb.dat: sdet1_results-noDeb
	@echo "PARSE    $@"
	./clean.sh sdet1_results-noDeb > sdet1_results-noDeb.dat

sdet4_results-noDeb.dat: sdet4_results-noDeb
	@echo "PARSE    $@"
	./clean.sh sdet4_results-noDeb > sdet4_results-noDeb.dat


sdet1_results-noDeb:
	@echo "SCP      $@"
	scp -q $(USER)@$(HOST):$(PLAC)/$(VICT)/sdet1_results-noDeb .

sdet4_results-noDeb:
	@echo "SCP      $@"
	scp -q $(USER)@$(HOST):$(PLAC)/$(VICT)/sdet4_results-noDeb .


clean:
	@echo "CLEAN    generated"
	rm -f $(PLOT) $(TRIM)

mrproper: clean
	@echo "CLEAN    raw"
	rm -f sdet1_results-noDeb sdet4_results-noDeb
