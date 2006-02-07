# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.18 2005/01/26 03:21:50 jappavoo Exp $
# ############################################################################

default:: libobjs

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing include files
COMMINCS = COSMgr.H COSMgrObject.H \
           COSMissHandler.H COSTransObject.H COSconsts.h COVTable.H \
           TransEntry.H COSDefaultObject.H COSBreakpointObject.H \
	   TransPageDescList.H TransPageDescHash.H TransPageDesc.H \
	   ActiveThrdCnt.H CODesc.H
ARCHINCS =

install_includes::
	$(INSTALL_INC) $(INCDIR)/cobj/sys/     ./        $(COMMINCS)

ifdef IN_OBJ_DIR
# so that all <> definitions will work
INCPATH  += -I$(SRC_DIR_PATH)/../..

# ############################################################################
#  rules to be made in object directory
# ############################################################################
COMMONSRCS = COSMgrObject.C TransPageDesc.C \
             TransPageDescList.C \
	     TransPageDescHash.C COSDefaultObject.C \
	     ActiveThrdCnt.C

LIBCSRCS += $(COMMONSRCS)
LIBKSRCS += $(COMMONSRCS) 
CLASSES  +=
SRCS     +=
TARGETS  +=

# for kernel modules under the os/kernel tree
LIBKERNSRCS +=

# for server modules under the os/server tree
SERVERS     +=


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
