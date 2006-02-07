# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# Adapted for Linux compilation by Livio Soares (livio@ime.usp.br)
#
#  $Id: Makefile,v 1.16 2003/12/30 20:00:19 dilma Exp $
# ############################################################################

TARGETS += mkfs.kfs fscp kfsToolTest kfsList #fsck.kfs
all: $(TARGETS)

# Change '1' below to '0', if snaphshotting is to be turned off
KFS_SNAPSHOT  := 0

HOST_CXX=g++

CPPFLAGS += #-DKFS_DEBUG

VPATH += $(MKSRCTOP)/tools/build/shared/fstools
VPATH += $(MKSRCTOP)/os/servers/kfs
VPATH += $(MKSRCTOP)/tools/build/shared/fstools/misc

# Places to look for header files
INCPATH += -I$(MKSRCTOP)/os/servers/kfs -I$(MKSRCTOP)/lib/libc

# Flags to be used by the compiler & linker
HOST_CXXFLAGS += -g -Wall -DTOOLSIDE -MD $(BITS) \
		 -DTARGET_MACHINE="$(TARGET_MACHINE)" \
		 -DKFS_TOOLS

# The objects from KFS that we must link in
OBJS_kfs =	../../DataBlock.o \
		../../LSOBasic.o \
		../../LSOBasicDir.o \
		../../LSOBasicFile.o \
		../../LSOBasicSymlink.o \
		../../ObjToken.o \
		../../PSOBasicRW.o \
		../../RecordMap.o \
		../../PSODiskBlock.o \
		../../PSOPreallocExtent.o \
		../../PSOSmall.o \
		../../PSOSmallMeta.o \
		../../ServerObject.o \
		../../SuperBlock.o \
		../../KFSGlobals.o \
		../../BlockCacheTools.o \
		../../KFSDebug.o \
		../misc/HashSimple.o

ifeq (1, $(KFS_SNAPSHOT))
HOST_CXXFLAGS += -DKFS_SNAPSHOT -I../../private -I../../private/linux
OBJS_kfs +=     ../../private/SuperBlockSnap.o \
		../../private/RecordMapSnap.o \
		../../private/PSOSmallSnap.o
endif

# Build pseudo-disk creator
OBJS_mkfs = mkfs.o FileDisk.o KFS.o $(OBJS_kfs)
mkfs.kfs: $(OBJS_mkfs)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

# Build pseudo-disk copier
OBJS_fscp = fscp.o FileDisk.o KFS.o $(OBJS_kfs)
fscp: $(OBJS_fscp)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

# Build test program
OBJS_test = test.o FileDisk.o KFS.o $(OBJS_kfs)
kfsToolTest: $(OBJS_test)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

# Build listing program
OBJS_list = list.o FileDisk.o KFS.o $(OBJS_kfs)
kfsList: $(OBJS_list)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

OBJS_fsck.kfs = fsck.o FileDisk.o KFS.o $(OBJS_kfs)
fsck.kfs: $(OBJS_fsck.kfs)
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@


install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)

clean::
	rm -f *.o *.d $(TARGETS)


# Include dependency information
DEP_MKFS = $(OBJS_mkfs)
-include $(DEP_MKFS:.o=.d)

DEP_FSCP = $(OBJS_fscp)
-include $(DEP_FSCP:.o=.d)

CXX                := g++
CXX_CFLAGS         := $(subst -O2,-O0,$(CFLAGS)) -Wall

ifeq (1, $(KFS_SNAPSHOT))
CXX_EXTRA_CFLAGS += -DKFS_SNAPSHOT -I../../private -I../../private/linux
endif

CXX_EXTRA_CFLAGS   += -DKFS_TOOLS -I. -idirafter .. -I../.. -fno-rtti -fno-exceptions

%.o: ../../tools/%.C
	$(CXX) $(CXX_CFLAGS) $(CXX_EXTRA_CFLAGS) $(CXX_CFLAGS_$@) -c -o $@ $<

%.o: %.C
	$(CXX) $(CXX_CFLAGS) $(CXX_EXTRA_CFLAGS) $(CXX_CFLAGS_$@) -c -o $@ $<