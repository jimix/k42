# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.9 2003/02/21 15:01:44 bob Exp $
# ############################################################################
##############################################################################

SUBDIRS = lib os tools kitch-linux

#
# Path to the kitchsrc directory
#
topsrc=..

#
# SGML file that is the begining of one complete document
#
QUICK_DOCSRCS = runningK42.sgml gettingStarted.sgml Style.sgml
DOCSRCS = $(QUICK_DOCSRCS) k42doc.sgml docPointers.sgml k42.sgml buildingOnx86.sgml

QUICK_TARGETS = $(QUICK_DOCSRCS:%.sgml=%.html)

#
# List of supporting (or secondary) files
#
COMMON= k42Over.sgml k42PrgG.sgml k42.ent 

include $(topsrc)/doc/Makefile.doc

quick: $(QUICK_TARGETS)
