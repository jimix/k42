# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.33 2001/09/13 14:16:13 mostrows Exp $
# ############################################################################



SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs tst_targets

COMMINCS = alloc.H AllocPool.H LMalloc.H AllocCell.H PageAllocatorUser.H \
	   PageAllocatorDefault.H MemoryMgrPrimitive.H AllocRegionManager.H \
	   MemDesc.H VAllocServices.H AllocStats.H DataChunk.H AllocStats.H \
           PMalloc.H ObjCache.H

SYSINCS   = PageAllocator.H LeakProof.H

# for installing machine specific includes
ARCHINCS  = LMalloc.H AllocCell.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/alloc/           ./           $(COMMINCS)
	$(INSTALL_INC) $(INCDIR)/sys/ 		 ./           $(SYSINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

INCPATH   += -I$(SRC_DIR_PATH)/..
COMMONSRCS  = LMalloc.C DataChunk.C GMalloc.C PMalloc.C MemDesc.C AllocPool.C \
	AllocCell.C PageAllocatorUser.C PageAllocatorDefault.C LeakProof.C \
	AllocRegionManager.C MemoryMgrPrimitive.C alloctst.C AllocStats.C \
	VAllocServices.C ObjCache.C
LIBCSRCS += $(COMMONSRCS)

LIBKSRCS += $(COMMONSRCS)

#tst_targets:: alloctst

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
