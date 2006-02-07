# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.106 2005/06/20 06:59:50 cyeoh Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=1
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: libobjs

ARCHINCS = Access.H
COMMINCS = $(ARCHINCS) RegionType.H FR.H KernelPagingTransport.H \
	   SharedBufferProducerKernel.H SharedBufferConsumerKernel.H

install_includes::
	$(INSTALL_INC) $(INCDIR)/mem/            ./            $(COMMINCS)


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
CLASSES += FR

SERVE += RegionDefault RegionPerProcessor \
	 RegionRedZone FRComputation FRKernelPinned FRPA FRVA RegionFSComm \
	 RegionReplicated FRCRW FRLTransTable \
	 KernelPagingTransport \
	 KernelPagingTransportPA KernelPagingTransportVA

LIBKERNSRCS += PageAllocatorKernPinned.C PageAllocatorKernUnpinned.C \
               RegionDefault.C RegionReplicated.C RegionPerProcessor.C \
	       PageFaultTest.C FR.C FRPlaceHolder.C FCMStartup.C\
	       FCM.C FCMDefault.C FCMFile.C \
	       FCMDefaultRoot.C FCMReal.C FCMComputation.C \
	       FCMPrimitiveKernel.H FCMFixed.C FCMPrimitive.C \
	       FCMSharedTrivial.C FCMPartitionedTrivial.C \
	       FCMCommon.C FCMCommonMultiRep.C FCMCommonMultiRepRoot.C \
               FCMDefaultMultiRep.C FCMDefaultMultiRepRoot.C \
               FCMPrimitiveKernelMultiRep.C FCMPrimitiveKernelMultiRepRoot.C \
               PageDescData.C PageList.C  SegmentTable.C \
	       PageSet.C PageSetDense.C SegmentHATPrivate.C \
	       HATDefault.C HATKernel.C FRVA.C FRPA.C FRPANonPageable.C\
	       FRPANonPageableRamOnly.C FRCRW.C \
	       RegionRedZone.C PageFaultNotification.C \
	       FRComputation.C FRCommon.C FRKernelPinned.C \
	       PMRoot.C FreeFrameList.C PMLeaf.C PMKern.C RegionFSComm.C \
               VAllocServicesKern.C MemDescKern.C AllocRegionManagerKern.C \
	       PMallocKern.C PageCopy.C Pin.C \
	       FRLTransTable.C FCMLTransTable.C \
	       RegionList.C \
	       SharedBufferProducerKernel.C SharedBufferConsumerKernel.C \
	       KernelPagingTransport.C KernelPagingTransportPA.C \
	       KernelPagingTransportVA.C PMLeafExp.C PMLeafChunk.C \
	       FreeFrameListChunk.C SyncService.C \
	       $(SERVE:%=X%.C)

#FIXME BIG Workaround to fix the compiler
ifeq ($(OPTIMIZATION),partDeb)
ifeq ($(PLATFORM_OS), AIX)
FCMCommonMultiRepRoot.o_CXXFLAGS= -O2 -DGCC_OPTIMIZATION_WORKAROUND
TARGET_CXXFLAGS += $($@_CXXFLAGS)
endif
endif


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
