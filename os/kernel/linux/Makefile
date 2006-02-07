# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.2 2005/06/28 19:44:43 rosnbrg Exp $
# ############################################################################

# SUBDIRS =
# set KERNEL=1 if for kernel
KERNEL=1

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

OLD_KITCH_GINC := $(KITCH_GINC)
#KITCH_GINC := $(OLD_KITCH_GINC)
#KITCH_GINC := \
TARGET_CFLAGS  += -D__KERNEL__ -D__powerpc__ -D__linux__ -O -w
TARGET_CXXFLAGS += $(TARGET_CFLAGS)

#CPPFLAGS   += -U__KERNEL__
default:: libobjs


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

linux_INCPATH:= -I$(LINUXSRCROOT)/arch/k42/include \
		-I$(LINUXSRCROOT)/include -I$(MKSRCTOP)/os/kernel \
		-I$(MKSRCTOP)/os/kernel/linux

INCPATH  = $(GCCFIXINC) $(linux_INCPATH) -I. -I$(INC_TOP) 
INCPATH += -I$(INC_GCC) -I$(INC_STBG) $(KITCH_GINC) 

LIBKERNSRCS+=StdIOKern.C MemoryK.C Configure.C Interrupt.C \
		LinuxSock.C LinuxBlockDev.C LinuxCharDev.C

SERVE+= 



# ############################################################################
#  end of object directory rules
# ############################################################################
endif
