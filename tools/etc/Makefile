# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.18 2002/03/07 21:05:29 jimix Exp $
# ############################################################################
# This file is derived from Tornado software developed at the University
# of Toronto.
# ############################################################################

#TARGETS += kitchawan.elc

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

SCRIPTS = stdhdr.c++ stdhdr.make
#SCRIPTS += kitchawan.el

$(MKKITCHTOP)/etc:
	$(INSTALL) --mode 0775 -d $@

install_includes::  $(MKKITCHTOP)/etc $(TARGETS) $(SCRIPTS)
	$(INSTALL) --mode 0444 $(TARGETS) $(SCRIPTS) $<
	cd $< && $(RM) stdhdr.c; \
	    $(INSTALL) --mode 0444 stdhdr.c++ stdhdr.c

kitchawan.elc : kitchawan.el
	$(EMACS) -batch -f batch-byte-compile kitchawan.el
