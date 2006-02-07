# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.33 2002/10/30 15:00:26 rosnbrg Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs tst_targets

COMMINCS = Thread.H Scheduler.H DispatcherDefault.H DispatcherDefaultExp.H \
	   SchedulerTimer.H SchedulerService.H VPSet.H \
	   DispatcherMgr.H CachePaddedArray.H

SYSINCS  = TimerEvent.H

# for installing machine specific includes
ARCHINCS = DispatcherDefault.H DispatcherDefaultExp.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/scheduler/ ./ $(COMMINCS)
	$(INSTALL_INC) $(INCDIR)/sys/       ./ $(SYSINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

CLASSES += SchedulerService

INCPATH   += -I$(SRC_DIR_PATH)/..
COMMONSRCS = Thread.C Scheduler.C DispatcherDefault.C DispatcherDefaultExp.C \
	     TimerEvent.C SchedulerTimer.C SchedulerService.C DispatcherMgr.C

LIBCSRCS += $(COMMONSRCS)
LIBKSRCS += $(COMMONSRCS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
