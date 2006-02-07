# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.49 2005/06/28 19:44:03 rosnbrg Exp $
# ############################################################################

SUBDIRS = linux pthread mixed

ifdef IN_OBJ_DIR

# make installation dirs

INSDIRS= tests/linux tests/pthread
KINSDIRS=$(INSDIRS:%=$(MKKITCHROOT)/%)
$(KINSDIRS):
	$(INSTALL) --mode 0775 -d $@

install_targets:: $(KINSDIRS)

endif

default::
	$(MAKE) SEQ=1 configure_sesame snapshot

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch
