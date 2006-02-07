# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.18 2002/10/15 13:44:08 mostrows Exp $
# ############################################################################

# SUBDIRS =
# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

# some implicit rules for making stub stuff
# these overwrite some potential rules from the global includes

ifdef IN_OBJ_DIR
#############################################################################
#  rules to be made in object directory
#############################################################################

SERVE += Bonnie

LIBKERNSRCS  += client.C Bonnie.C XBonnie.C        # real sources

#############################################################################
#  end of object directory rules
#############################################################################
endif
