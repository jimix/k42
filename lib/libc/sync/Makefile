# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.29 2001/06/19 17:19:23 peterson Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

ARCHINCS   = atomic.h BLock.H FairBLock.H
COMMINCS  = RequestCount.H MPMsgMgr.H MPMsgMgrEnabled.H \
	    MPMsgMgrDisabled.H $(ARCHINCS) \
	    Lock.H BLock.H BLockGeneric.H PtrBLock.H PtrSLock.H SLock.H \
	    BlockedThreadQueues.H FairBLock.H Barrier.H RWLock.H RecLock.H \
	    Sem.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/sync/            ./          $(COMMINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that all <> definitions will work
INCPATH   += -I$(SRC_DIR_PATH)/..

COMMONSRCS = atomic.C RequestCount.C BLockImp.C BLockGeneric.C SLock.C \
	     MPMsgMgr.C MPMsgMgrEnabled.C  MPMsgMgrDisabled.C \
	     BlockedThreadQueues.C FairBLock.C Barrier.C
LIBCSRCS  += $(COMMONSRCS)
LIBKSRCS  += $(COMMONSRCS)
CLASSES   +=
SRCS      +=
TARGETS   +=

TSTSRCS += lockTest.C
TST_TARGETS += lockTest

lockTest: lockTest.o

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
