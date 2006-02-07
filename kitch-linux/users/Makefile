# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.12 2004/12/30 14:47:39 bob Exp $
# ############################################################################

SUBDIRS = root regress pftest mysql regress-hw postmark pem

ifdef SPEC_PKG
SUBDIRS+=spec
endif

ifdef IN_OBJ_DIR

# make installation dirs
INSDIRS = spec regress pftest mysql regress-hw postmark pem

KINSDIRS = $(INSDIRS:%=$(MKKITCHROOT)/home/%)
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
