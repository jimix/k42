# ############################################################################
# K42: (C) Copyright IBM Corp. 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

INCFILES = FileInfoVirtFS.H ServerFileVirtFS.H FIVFAccessOH.H

CLASSES += 

install_includes::
	$(INSTALL_INC) $(INCDIR)/fslib/virtfs	./        $(INCFILES)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
# so that all <> definitions will work

INCPATH   += -I$(SRC_DIR_PATH)/../..
LIBVFSSRCS += FileInfoVirtFS.C ServerFileVirtFS.C FIVFAccessOH.C ../FSStats.C

# ############################################################################
#  end of object directory rules
# ############################################################################
endif


