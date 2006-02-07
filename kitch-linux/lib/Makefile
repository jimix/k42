# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.34 2002/11/05 22:24:58 mostrows Exp $
# ############################################################################


SUBDIRS = emu

default::
	$(MAKE) SEQ=1 configure_sesame snapshot

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

