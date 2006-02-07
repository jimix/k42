# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.41 2005/02/14 16:04:00 dilma Exp $
# ############################################################################

# If host OS is AIX 32-bit/64-bit
# -------------------------------
ifeq ($(PLATFORM_OS),AIX)
# 64 bit or 32 bit ABI. Uncomment only 1.
#BITS = -maix64
BITS = -maix32
CPPFLAGS += -DAIX_OS

# Else if host OS is IRIX64 (64-bit)
# ----------------------------------
else
ifeq ($(PLATFORM_OS),IRIX64)
# 64 bit or 32 bit ABI. Uncomment only 1.
#BITS = -mabi=64 -mips3
BITS = -mabi=n32 -mips3
CPPFLAGS += -DIRIX_OS

# Else if host OS is IRIX 32-bit
# ------------------------------
else
ifeq ($(PLATFORM_OS),IRIX)
BITS = -mabi=n32 -mips3
CPPFLAGS += -DIRIX_OS

# Else if host OS is Linux 32-bit
# -------------------------------
else
ifeq ($(PLATFORM_OS),Linux)
BITS = -DLINUX_OS

else
ifeq ($(PLATFORM_OS),Darwin)
BITS = -DLINUX_OS

# Else if host OS is unknown
# --------------------------
else
$(warning "$(PLATFORM_OS) is an unsupported OS.")

endif
endif
endif
endif
endif

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

ifeq (KFS, $(FILESYS))

ifdef IN_OBJ_DIR
#TARGETS += mkfs fscp kfsToolTest kfsList fsck.kfs
TARGETS += mkfs.kfs fscp kfsToolTest kfsList kfsgetversion

CPPFLAGS += #-DKFS_DEBUG

ifndef KFS_UNSTABLE
KFS_SRC_DIR_PATH = $(MKSRCTOP)/os/servers/kfs
else
KFS_SRC_DIR_PATH = $(MKSRCTOP)/os/servers/kfs-unstable
endif

VPATH += $(MKSRCTOP)/tools/build/shared/fstools
VPATH += $(KFS_SRC_DIR_PATH)
VPATH += $(KFS_SRC_DIR_PATH)/os-k42
# VPATH += $(MKSRCTOP)/tools/build/shared/fstools/misc
VPATH += $(MKSRCTOP)/os/kernel/defines
VPATH += $(MKSRCTOP)/lib/libc/misc/

# Places to look for header files
INCPATH = -I$(KFS_SRC_DIR_PATH)/tools/
INCPATH += -I$(KFS_SRC_DIR_PATH) -I$(KFS_SRC_DIR_PATH)/os-k42
INCPATH += -I$(MKSRCTOP)/lib/libc

# Flags to be used by the compiler & linker
HOST_CXXFLAGS += -g -Wall -DTOOLSIDE -MD $(BITS) \
		 -DTARGET_MACHINE="$(TARGET_MACHINE)" \
		 -DKFS_TOOLS

# The objects from KFS that we must link in
OBJS_kfs =	HashSimple.o \
		DataBlock.o \
		LSOBasic.o \
		LSOBasicDir.o \
		LSOBasicFile.o \
		LSOBasicSymlink.o \
		PSOBasicRW.o \
		RecordMap.o \
		PSODiskBlock.o \
		PSOPreallocExtent.o \
		ServerObject.o \
		SuperBlock.o \
		ListSimple.o \
		KFSGlobals.o \
		BlockCacheTools.o \
		PSOSmall.o \
		PSOSmallMeta.o \
		KFSDebug.o

ifdef KFS_UNSTABLE
OBJS_kfs +=     FSFileKFS.o FSFileKFSTools.o LSODirEmb.o GlobalRecordMap.o \
		LocalRecordMap.o
else
OBJS_kfs  +=     ObjToken.o
endif

SRCDIR = $(KFS_SRC_DIR_PATH)/tools
#SRC_kfs = $(SRCDIR)/FileDisk.C $(SRCDIR)/KFS.C

#FIXME: we can specify these rules in a more elegant way
mkfs.o: $(SRCDIR)/mkfs.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
FileDisk.o : $(SRCDIR)/FileDisk.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
KFS.o : $(SRCDIR)/KFS.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
fscp.o : $(SRCDIR)/fscp.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
fsck.o : $(SRCDIR)/fsck.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
list.o : $(SRCDIR)/list.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
test.o : $(SRCDIR)/test.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
getversion.o : $(SRCDIR)/getversion.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<
FSFileKFSTools.o: $(SRCDIR)/FSFileKFSTools.C
	$(HOST_CXX) $(HOST_CXXFLAGS) -c $<

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

kfsgetversion: getversion.o
	$(HOST_CXX) $(HOST_CXXFLAGS) $^ -o $@

OBJS_all = $(OBJS_mkfs) $(OBJS_fscp) $(OBJS_test) $(OBJS_list) $(OBJS_fsck.kfs)

install_targets:: $(TARGETS)
	$(INSTALL) -m 0555 $(TARGETS) $(MKTOOLBIN)
# clean obj directory so alternat aix/linux builds work
	$(RM) $(TARGETS) getversion.o $(OBJS_all) $(OBJS_all:.o=.d)
	$(RM) $(OBJS_mkfs:.o=.d) $(OBJS_fscp:.o=.d) $(OBJ_fscp:.o=.d)
	$(RM) $(OBJS_mkfs) $(OBJS_list)
	$(RM) $(OBJS_list:.o=.d) 

clean::
	rm -f *.o *.d $(TARGETS)

#location of .d for kfs files
KFS_DEP_DIR=$(MKANCHOR)/$(TARGET_MACHINE)/$(OPTIMIZATION)/os/servers/baseServers
DEP_KFS = $(addprefix $(KFS_DEP_DIR)/, $(OBJS_kfs))

DEP_MKFS = $(DEP_KFS) mkfs.o FileDisk.o KFS.o
-include $(DEP_MKFS:.o=.d)

DEP_LIST = $(DEP_KFS) list.o FileDisk.o KFS.o
-include $(DEP_LIST:.o=.d)



endif # IN_OBJ_DIR

endif # FILESYS KFS
