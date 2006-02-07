# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.24 2004/08/20 17:30:48 mostrows Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

MKSERV = mkserv

# FIXME: should use INSTALL_INC
$(MKKITCHTOP)/include/sys:
	$(INSTALL) --mode 0775 -d $@

install_includes:: $(MKKITCHTOP)/include/sys
	@$(ECHO) "installing $(MKKITCHTOP)/include/sys/bootServers.H"
	@$(CMP) -s bootServers.H $(MKKITCHTOP)/include/sys/bootServers.H > \
							    /dev/null 2>&1 || \
	    $(INSTALL) --mode 0444 bootServers.H $(MKKITCHTOP)/include/sys
	@$(ECHO) "installing $(MKKITCHTOP)/include/sys/baseBootServers.H"
	@$(CMP) -s bootServers.H $(MKKITCHTOP)/include/sys/baseBootServers.H > \
							    /dev/null 2>&1 || \
	    $(INSTALL) --mode 0444 baseBootServers.H $(MKKITCHTOP)/include/sys

TARGETS += $(MKSERV)

ifneq ($(PLATFORM_OS),AIX)
HOST_CXXFLAGS += -I$(SRC_DIR_PATH)/../stubgen/elf
endif

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

$(MKSERV): mkserv.c
	$(HOST_CXX) $(HOST_CXXFLAGS) $< -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
# clean obj directory so alternate aix/linux makes work
	$(RM) $(TARGETS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
