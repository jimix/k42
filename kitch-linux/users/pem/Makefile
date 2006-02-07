# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.1 2004/12/30 13:43:13 bob Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR

USER_DOT =	profile
USER_SCRIPT=sysinit
USER_HOME=$(MKKITCHROOT)/home/pem
USER_INSTALL= $(USER_DOT:%=$(USER_HOME)/.%) $(USER_SCRIPT:%=$(USER_HOME)/%)

$(USER_HOME)/.% : $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)

$(USER_HOME)/%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

install_targets:: $(USER_INSTALL)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
