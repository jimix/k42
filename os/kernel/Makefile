# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.65 2004/02/27 17:14:29 mostrows Exp $
# ############################################################################

SUBDIRS = init exception mem proc stubtest bilge trace sys defines linux

default:: genhdrs libobjs targets

# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

libobjs :: subdirs_libobjs
install_libobjs :: subdirs_install_libobjs



#libobjs :: libkern.INPUT
install_libobjs :: libkern.INPUT

SUBDIRS_INPUTS = $(SUBDIRS:%=%/kernobjs.INPUT)

%/kernobjs.INPUT: %_install_libobjs

libkern.INPUT: $(SUBDIRS_INPUTS)
	-$(RM) $@
	$(CAT) $(SUBDIRS_INPUTS) > $@

clean ::
	-$(RM) libkern.INPUT


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
