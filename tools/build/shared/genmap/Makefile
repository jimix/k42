# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2005/06/07 05:19:10 jk Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

GENMAP = genmap 

TARGETS += $(GENMAP)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

$(GENMAP): genmap.c
	$(HOST_CC) $(HOST_CFLAGS) \
		-I$(MKSRCTOP)/tools/build/shared/relocate $< -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
# alternate aix/linux build kludge
	$(RM) $(TARGETS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
