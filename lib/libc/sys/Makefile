# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.45 2005/06/13 14:11:04 rosnbrg Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

COMMINCS = systemAccess.H sysTypes.H sysIncs.H BaseProcess.H ProcessWrapper.H \
	   ProcessSet.H ProcessSetUser.H macdefs.H config.H types.H \
	   ppccore.H kinclude.H SysStatus.H TAssert.H ProcessClient.H \
           ProcessLinux.H LinuxCreds.H ResMgrWrapper.H SystemMiscWrapper.H \
	   MountPointMgrClient.H MountPointMgrCommon.H SMTMgr.H \
	   ProcessLinuxClient.H ShMemBuf.H ShMemClnt.H \
	   InitStep.H Initialization.H hostSysTypes.H bswap.h extRegs.H

ARCHINCS = sysTypes.H macdefs.H config.H types.H ppccore.H extRegs.H

install_includes::
	$(INSTALL_INC)  $(INCDIR)/sys/            ./            $(COMMINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that all <> definitions will work
INCPATH   += -I$(SRC_DIR_PATH)/../

COMMONSRCS = ppccore.C TAssert.C g++sup.C ProcessClient.C LinuxCreds.C \
	     ResMgrWrapper.C SystemMiscWrapper.C \
	     SMTMgr.C ProcessSet.C InitStep.C
LIBCSRCS += $(COMMONSRCS) ProcessWrapper.C ProcessSetUser.C abort.C \
	    ppccoreusr.C ProcessLinuxClient.C MountPointMgrClient.C \
	    MountPointMgrCommon.C ShMemClnt.C ShMemBuf.C
LIBKSRCS += $(COMMONSRCS)
CLASSES  += ProcessClient ProcessLinuxClient ShMemBuf InitStep
SRCS     +=
TARGETS  +=

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
