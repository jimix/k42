# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.52 2005/08/09 12:03:23 dilma Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

# for installing machine specific includes
# many files have a machine independent file which fronts for a 
# machine specific sub-file (like machine independent execute.H
# which include a machine specific execute.H subfile
# but we also have some files (like fpr.h) which are simply
# machine dependent.

SOLEINCS  = fpr.h

SUBINCS  =  asm.h asmdef.h alloca.H hardware.H execute.H \
	    expedient.H volatileFrame.H

# the architecture specific files
ARCHINCS  = $(SUBINCS) $(SOLEINCS)

# the common files 
COMMINCS = $(SUBINCS) linkage.H ListSimple.H ListArraySimple.H \
	   ListArraySimple.C ListSimpleKey.H ListSimpleKeyLocked.H \
           ListSimpleLocked.H  StringShort.H BitStructure.H macros.H \
           StringTable.H StringTable.I utilities.H baseStdio.H \
           simpleThread.H  SSAC.H SSACSimplePartitionedArray.H \
           SSACSimplePartitionedArray.C testSupport.H HashSimple.H \
	   BitVec.H DHashTable.H AutoList.H AutoList.I BaseRandom.H \
	   HashNonBlocking.H HashNonBlocking.I AtomicBitVec64.H BlockSet.H \
           DiskMountInfoList.H Callback.H \
	   SharedBufferProducer.H SharedBufferProducerUser.H \
           SharedBufferConsumer.H SharedBufferConsumerUser.H
#	   DLList.H DLList.I

CLASSES +=

install_includes::
	$(INSTALL_INC) $(INCDIR)/misc/            ./          	$(COMMINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

COMMONSRCS = ListSimple.C ListSimpleKey.C testSupport.C \
	     BaseRandom.H linkage.C hardware.C \
	     HashSimple.C BlockSet.C \
	     DiskMountInfoList.C \
	     SharedBufferProducer.C SharedBufferProducerUser.C \
             SharedBufferConsumer.C
	     # StringTable.C

LIBCSRCS += $(COMMONSRCS)
LIBKSRCS += $(COMMONSRCS)
CLASSES  +=
SRCS     +=
TARGETS  +=

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
