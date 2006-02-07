# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.56 2005/06/28 19:44:40 rosnbrg Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

install_includes::

ifdef IN_OBJ_DIR
#############################################################################
#  rules to be made in object directory
#############################################################################

CLASSES +=
SERVE += CPUContainer

LIBKERNSRCS += DispatchQueue.C IPCTargetTable.C CPUDomainAnnex.C \
		ProcessAnnex.C ProcessAnnexArch.C \
		ExceptionLocal.C ExceptionExp.C \
		MPMsgMgrException.C HWInterrupt.C KernelTimer.C \
		HWIOCommon.C KernelInfoMgr.C IPCRetryManager.C \
		FaultNotificationMgr.C \
		$(SERVE:%=X%.C)


#############################################################################
#  end of object directory rules
#############################################################################
endif
