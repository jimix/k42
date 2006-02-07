# ############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: Makefile,v 1.13 2005/06/28 19:44:32 rosnbrg Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing include files
INCFILES = fixup.H lkIncs.H LinuxEnv.H Memory.H LinuxGlue.H InitCalls.H Wait.H
#Interrupt.H Memory.H \
#	 LinuxMode.H LinuxArchSys.H Utils.H modes.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/lk/           ./        $(INCFILES)

linux_INCPATH = -I$(LINUXSRCROOT)/arch/k42/include -I$(LINUXSRCROOT)/include
INCPATH  = $(linux_INCPATH) -I. -I$(INC_TOP) -I$(INC_GCC) -I$(INC_STBG)
INCPATH += $(KITCH_GINC)


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
TARGET_CFLAGS := $(TARGET_CFLAGS) -D__KERNEL__ -D__powerpc__ -D__linux__ -O  -w
TARGET_CXXFLAGS += $(TARGET_CFLAGS)

# so that all <> definitions will work
INCPATH   += -I$(SRC_DIR_PATH)/..
ifdef LINUXSRCROOT

COMMONSRCS += Locks.C Timer.C LinuxEnv.C Semaphore.C Memory.C MultiProc.C \
	Scheduler.C Configure.C Bug.C Wait.C Utils.C RCUCollector.C \
	workqueue.C InitCalls.C LinuxDevFS.C SysFS.C
LIBLKSRCS += $(COMMONSRCS) 

endif


# ############################################################################
#  end of object directory rules
# ############################################################################
endif



