# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.6 2003/03/23 17:59:09 mostrows Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR

install_TARGETS= $(home_INSTALL) $(dot_INSTALL) $(bin_INSTALL)

USER_HOME=$(MKKITCHROOT)/home/pftest

home_TARGETS=	grepdata
home_INSTALL= $(home_TARGETS:%=$(USER_HOME)/%)

bin_TARGETS=	testdriver.sh testbody.sh runTest
bin_INSTALL= $(bin_TARGETS:%=$(USER_HOME)/%)

dot_TARGETS=	profile
dot_INSTALL= $(dot_TARGETS:%=$(USER_HOME)/.%)


$(home_INSTALL): $(USER_HOME)/%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0644 $< $@
#	$(INSTALL_DSK)

$(bin_INSTALL): $(USER_HOME)/%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0755 $< $@
#	$(INSTALL_DSK)

$(dot_INSTALL): $(USER_HOME)/.%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0644 $< $@
#	$(INSTALL_DSK)

#install_targets:: $(USER_INSTALL)
install_targets:: $(install_TARGETS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
