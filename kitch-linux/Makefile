# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.26 2005/06/28 19:42:54 rosnbrg Exp $
# ############################################################################

SUBDIRS = lib tests etc users ketc

ifdef IN_OBJ_DIR

# Make installation dirs on NFS & local file system
INSDIRS = tests tmp etc root usr usr/tmp
# Make installation dirs on NFS only
INSNFSDIRS = home

KINSDIRS = $(INSDIRS:%=$(MKKITCHROOT)/%)
KINSNFSDIRS = $(INSNFSDIRS:%=$(MKKITCHROOT)/%)

$(KINSDIRS):
	$(INSTALL) --mode 0775 -d $@
	$(INSTALL_DIR_DSK)

$(KINSNFSDIRS):
	$(INSTALL) --mode 0775 -d $@

install_targets:: $(KINSDIRS) $(KINSNFSDIRS)
endif

default::
	$(MAKE) $(PARALLEL) configure_sesame snapshot

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch



