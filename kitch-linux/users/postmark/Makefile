# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2004/06/15 20:06:21 dilma Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR


POSTMARK_HOME=$(MKKITCHROOT)/home/postmark

SCRIPTS= runPostmark runtest.sh
INPUTFILES= input.txt input.txt.old
SCRIPTS_INSTALL= $(SCRIPTS:%=$(POSTMARK_HOME)/%)
INPUT_INSTALL= $(INPUTFILES:%=$(POSTMARK_HOME)/%)
$(SCRIPTS_INSTALL) : $(POSTMARK_HOME)/%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

$(INPUT_INSTALL) : $(POSTMARK_HOME)/%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0644 $< $@
	$(INSTALL_DSK)

install_targets:: $(SCRIPTS_INSTALL) $(INPUT_INSTALL)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
