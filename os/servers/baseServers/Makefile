# ############################################################################
# K42: (C) Copyright IBM Corp. 2001, 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.73 2005/06/28 19:47:48 rosnbrg Exp $
# ############################################################################

SUBDIRS =

CLASSES += BaseServers
SERVE += ResMgr ProcessLinuxServer StreamServerSocket
COMMINCS += baseBootServers.H bootServers.H

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

fast::
	$(MAKE) install_targets
	cd ../.. && $(MAKE) install_targets

install_includes::
	$(INSTALL_INC) $(INCDIR)/sys/ ./ $(COMMINCS)

ifdef IN_OBJ_DIR


# ############################################################################
#  rules to be made in object directory
# ############################################################################

# include all the information to let dev file system compile here
VPATH    += $(SRC_DIR_PATH)/../dev
INCPATH  += -I$(SRC_DIR_PATH)/../dev
include $(SRC_DIR_PATH)/../dev/Make.files

# include all the information to let mount point server compile here
VPATH    += $(SRC_DIR_PATH)/../mountPoint
INCPATH  += -I$(SRC_DIR_PATH)/../mountPoint
include $(SRC_DIR_PATH)/../mountPoint/Make.files

# include all the information to let pty compile here
VPATH    += $(SRC_DIR_PATH)/../pty
INCPATH  += -I$(SRC_DIR_PATH)/../pty
include $(SRC_DIR_PATH)/../pty/Make.files

# include all the information to let pipe compile here
VPATH    += $(SRC_DIR_PATH)/../pipe
INCPATH  += -I$(SRC_DIR_PATH)/../pipe
include $(SRC_DIR_PATH)/../pipe/Make.files

# include all the information to let nfs compile here
VPATH    += $(SRC_DIR_PATH)/../nfs
INCPATH  += -I$(SRC_DIR_PATH)/../nfs
include $(SRC_DIR_PATH)/../nfs/Make.files

# include all the information to let k42Ramfs compile here
VPATH    += $(SRC_DIR_PATH)/../k42Ramfs
INCPATH  += -I$(SRC_DIR_PATH)/../k42Ramfs
include $(SRC_DIR_PATH)/../k42Ramfs/Make.files

# include all the information to let loadImage compile here
VPATH    += $(SRC_DIR_PATH)/../loadImage
INCPATH  += -I$(SRC_DIR_PATH)/../loadImage
include $(SRC_DIR_PATH)/../loadImage/Make.files

# include all the information to let kfs compile here
ifeq (KFS, $(FILESYS))
ifndef KFS_UNSTABLE
KFS_SRC_DIR_PATH = $(SRC_DIR_PATH)/../kfs
else
KFS_SRC_DIR_PATH = $(SRC_DIR_PATH)/../kfs-unstable
endif
kfs_priv_VPATH  = $(KFS_SRC_DIR_PATH)/private
kfs_priv_VPATH  += $(KFS_SRC_DIR_PATH)/private/os-k42
VPATH    += $(kfs_priv_VPATH)
VPATH    += $(KFS_SRC_DIR_PATH)
VPATH    += $(KFS_SRC_DIR_PATH)/os-k42
INCPATH  += -I$(KFS_SRC_DIR_PATH)/private
INCPATH  += -I$(KFS_SRC_DIR_PATH)/private/os-k42
INCPATH  += -I$(KFS_SRC_DIR_PATH) -I$(KFS_SRC_DIR_PATH)/include
INCPATH  += -I$(KFS_SRC_DIR_PATH)/os-k42
include $(KFS_SRC_DIR_PATH)/Make.files
endif

ifdef EXT2_ENABLED
# include all the information to let ext2 compile here
VPATH    += $(SRC_DIR_PATH)/../ext2
INCPATH  := -I$(LINUXSRCROOT)/arch/k42/include $(INCPATH) -Idilma
INCPATH  += -I$(SRC_DIR_PATH)/../ext2
include $(SRC_DIR_PATH)/../ext2/Make.files
endif

NOINST_SERVERS  += baseServers
SRCS += baseServer.C fileSystemServices.C $(SRVSRCS)
SRCS += crtServer.C PrivilegedServiceWrapper.C
SRCS += ResMgr.C XResMgr.C
SRCS += ProcessLinuxServer.C XProcessLinuxServer.C
SRCS += StreamServerSocket.C XStreamServerSocket.C
TARGET_CXXFLAGS += -DVIRTFS_ENABLED 

# need to rebuild if this Makefile or Make.paths changes
baseServer.o : $(SRC_DIR_PATH)/Makefile $(MKSRCTOP)/Make.paths

ifdef EXT2_ENABLED
# This means that pty functionality has been factored out into 2 libraries
LKMOD =  $(LINUXSRCROOT)/base.k42lib.o $(LINUXSRCROOT)/pty.k42lib.o 
LKMOD += $(LINUXSRCROOT)/fs.k42lib.o
else
LKMOD = $(LINUXSRCROOT)/pty.k42lib.o
endif
LKMOD += $(LINUXSRCROOT)/util.k42lib.o

SERV_LIBS =  $(LKMOD) \
		-Wl,--whole-archive -lemu -Wl,--no-whole-archive \
		-llk26 -lvirtfs -lfs -lsrv
SERV_DEPS = $(LKMOD) $(LIB_EMU) $(LIB_LK) $(LIB_VFS) $(LIB_FS) $(LIB_SRV)


# ############################################################################
#  end of object directory rules
# ############################################################################
endif
