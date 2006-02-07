# ############################################################################
# K42: (C) Copyright IBM Corp. 2004.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.5 2005/06/28 19:47:51 rosnbrg Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

CLASSES += FileSystemExt2

fast::
	$(MAKE) install_targets
	cd ../.. && $(MAKE) install_targets

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
targets:: $(SERVERS) $(UTILS)

TARGET_CFLAGS += $($@_CFLAGS)
SERVERS += ext2Server
UTILS += 
SRCS += printk.C ext2.C Util.C $(SRVSRCS)

include $(SRC_DIR_PATH)/Make.files

# since not automatically part of OBJ, FIXME??
-include tst.d

#------------------------ stuff for linking to linux code ----------------
ifndef LINUXSRCROOT
FS_LKMOD =
linux_INCPATH = 
linux_fromBilge =
else
FS_LKMOD = $(LINUXSRCROOT)/fs.k42lib.o $(LINUXSRCROOT)/util.k42lib.o
#linux_INCPATH = -I$(LINUXSRCROOT)/include -I$(MKSRCTOP)/os/kernel
linux_INCPATH = -I$(LINUXSRCROOT)/include
#Bilge = $(MKANCHOR)/$(TARGET_MACHINE)/$(OPTIMIZATION)/os/kernel/bilge
#linux_Bilge = $(Bilge)/linux

endif

INCPATH := $(linux_INCPATH) $(INCPATH)

#------------------------- stuff needed for linking to glibc -------------
SERV_LIBS = $(FS_LKMOD) -lfs -llk26
SERV_DEPS = $(FS_LKMOD) $(LIB_FS) $(LIB_LK)

# How to install utilities
INST_PROGS = $(UTILS:%=$(MKKITCHROOT)/kbin/%)

# rule for installing a utility program in bin directory
$(MKKITCHROOT)/kbin/%: % $(MKKITCHROOT)/kbin
	$(INSTALL) --mode 0755 $< $(@D)

# ############################################################################
#  end of object directory rules
# ############################################################################

endif

