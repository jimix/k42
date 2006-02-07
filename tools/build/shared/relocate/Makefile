# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.5 2004/01/18 16:35:54 marc Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

RELOCATE = relocate 

TARGETS += $(RELOCATE)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

$(RELOCATE): relocate.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -g -I. $< -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
# alternate aix/linux build kludge
	$(RM) $(TARGETS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
