# ############################################################################
# K42: (C) Copyright IBM Corp. 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.4 2001/11/06 19:51:30 dilma Exp $
# ############################################################################

#
# Path to the kitchsrc directory
#
topsrc=../../../..

#
# SGML file that is the begining of one complete document
#
DOCSRCS=fslib.sgml

#
# List of supporting (or secondary) files
#
COMMON= fslib.ent fslibOver.sgml fslibPrgG.sgml

FIGS = figures/multilink.fig \
       figures/detach-file.fig \
       figures/detach-multilink.fig

fslib.html: $(FIGS:%.fig=%.jpg)
fslib.ps: $(FIGS:%.fig=%.eps)

include $(topsrc)/doc/Makefile.doc
