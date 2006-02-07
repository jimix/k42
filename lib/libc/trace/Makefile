# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.44 2005/08/22 14:09:50 dilma Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

COMMINCS = traceCore.H traceReserve.H traceLib.H tracePack.H traceK42.h

COMMXMLS_x = traceUser.xml traceIO.xml traceAlloc.xml traceMisc.xml \
	traceClustObj.xml traceLock.xml traceScheduler.xml \
	traceFS.xml traceLinux.xml traceResMgr.xml traceApp.xml \
	traceDbg.xml traceOSClasses.xml \
	traceMem.xml traceMiscKern.xml traceProc.xml traceTest.xml \
	traceException.xml traceDefault.xml traceControl.xml \
	traceHWPerfMon.xml traceSwap.xml traceMemColor.xml \
	traceInfo.xml traceOMP.xml traceLibC.xml traceMPI.xml \
	traceDisk.xml

COMMXMLS = $(addprefix k42-trace-xml/, $(COMMXMLS_x))

# for installing machine specific includes
ARCHINCS = traceReserve.H

PEMGENHDRS = pemEvent.py pemGenCPP.py pemGenHdrs.py pemGenPE.py pemGenC.py \
	     pemGenFortran.py pemGenJava.py pemGlobals.py pemGenPEMAPI.py \
             pemTypes.py

PEMGENHDRS_WITH_PATH = $(PEMGENHDRS:%.py=$(PEM_GEN_HDR_PATH)/%.py)
PEMGEN_SCRIPTS:=$(addprefix $(PEM_GEN_HDR_PATH)/, $(PEMGENHDRS))

install_includes:: $(INCDIR)/trace/.install_xml
	$(INSTALL_INC) $(INCDIR)/trace/          ./              $(COMMINCS)

# yes we really mean to pipe to /dev/null this test much of the java
#  functionality to make sure we haven't introduced any syntax error for java
$(INCDIR)/trace/.install_xml: $(COMMXMLS)
	$(INSTALL) $(PEMGEN_SCRIPTS) $(MKTOOLBIN)
	pemGenHdrs.py --outputDir $(INCDIR)/trace/ --language C -s K42 $(COMMXMLS)
	pemGenHdrs.py -n -l Java -o /dev/null $(COMMXMLS) > /dev/null
	touch $@


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

COMMONSRCS = traceLib.C
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
