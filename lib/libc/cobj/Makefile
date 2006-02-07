# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.28 2005/01/26 03:21:49 jappavoo Exp $
# ############################################################################

default:: libobjs
SUBDIRS = sys

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing include files
COMMINCS = CObjRep.H CObjRoot.H CObjRootSingleRep.H CObjRootMultiRep.H \
           ObjectRefs.H  XHandleTrans.H XObjectList.H BaseObj.H Obj.H \
	   ObjectHandle.H TypeMgr.H TypeMgrServer.H TypeFactory.H Example.H \
	   CObjRepArbiter.H CObjRepArbiterTarget.H CObjRootArbiter.H \
	   DataTransfer.H DTType.H CObjGlobals.H Factory.H COListServer.H \
	   COList.H
ARCHINCS = CallDescriptor.H

METAINCS  = MetaBaseObj.H
STUBINCS  = StubBaseObj.H
XOBJINCS  = XBaseObj.H
TKLUDGE	 = $(MKKITCHTOP)/include/xobj/XBaseObj.H    \
	   $(MKKITCHTOP)/include/meta/MetaBaseObj.H \
	   $(MKKITCHTOP)/include/stub/StubBaseObj.H \
	   $(MKKITCHTOP)/include/cobj/BaseObj.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/cobj/      ./           $(COMMINCS)
	$(INSTALL_INC) $(INCDIR)/stub/      ./           $(STUBINCS)
	$(INSTALL_INC) $(INCDIR)/meta/      ./           $(METAINCS)
	$(INSTALL_INC) $(INCDIR)/xobj/      ./           $(XOBJINCS)
        # following kludge prevents stubgen from trying to remake
        # XBaseObj.H etc. when any one is modified by making all times
        # the same as whichever is newest
        # N.B. if you modify BaseObj.H you must to a make install_includes
	$(CHMOD) +w $(TKLUDGE)
	$(KNEWEST) BaseObj.H $(TKLUDGE) $(XOBJINCS) $(STUBINCS) $(METAINCS)
	$(CHMOD) -w $(TKLUDGE)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that all <> definitions will work
INCPATH  += -I$(SRC_DIR_PATH)/..

COMMONSRCS = CObjRep.C CObjRoot.C CObjRootSingleRep.C CObjRootMultiRep.C \
             XHandleTrans.C XObjectList.C BaseObj.C Obj.C ObjectHandle.C \
	     TypeMgr.C missHandlingtest.C Factory.C \
	     CObjRepArbiter.C CObjRepArbiterTarget.C CObjRootArbiter.C \
	     CObjRepArbiterCallCounter.C COListServer.C COList.C
COMMONSRCS += CObjRootMediator.C CObjRepMediator.C MediatedThreadTable.C \
	      DataTransfer.C
LIBCSRCS += $(COMMONSRCS)
LIBKSRCS += $(COMMONSRCS) TypeMgrServer.C
CLASSES  += Obj TypeMgrServer COListServer

ifndef IN_OBJ_DIR
BaseObj.H : $(XOBJSINCS)
	@$(ECHO) "TOUCHING BaseObj.H, SINCE BASE OBJECTS CHANGED, SORRY"
	@-$(RM) $(MKKITCHTOP)/BaseObj.H
	$(TOUCH) BaseObj.H

install_includes:: BaseObj.H
	$(INSTALL_INC) $(INCDIR)           ./           $(COMMINCS)
endif

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
