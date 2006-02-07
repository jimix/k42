# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2003/06/23 18:05:30 kdiaa Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR

USER_DOT =	profile
home_TARGETS=	 clean.sql cleanall.sh listen_server.pl reboot.sh setup.sh startall.sh stopall.sh
home_INSTALL= $(home_TARGETS:%=$(USER_HOME)/%)

USER_HOME=$(MKKITCHROOT)/home/mysql
USER_INSTALL= $(USER_DOT:%=$(USER_HOME)/.%)

$(USER_HOME)/.% : $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)

$(home_INSTALL): $(USER_HOME)/%: $(SRC_DIR_PATH)/%
		 $(INSTALL) --mode 0755 $< $@

install_targets:: $(USER_INSTALL) $(home_INSTALL)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
