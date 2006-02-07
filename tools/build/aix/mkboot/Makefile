# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.9 2001/06/19 17:22:00 peterson Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

ifeq ($(findstring CYGWIN,$(PLATFORM)),)

TARGETS += mkboot

HOST_CXXFLAGS += -idirafter $(MKKITCHTOP)/include

mkboot: mkboot.c
	$(HOST_CXX) $(HOST_CXXFLAGS) mkboot.c -o mkboot

install_targets::
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)
endif
