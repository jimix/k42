# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.32 2004/08/20 17:30:49 mostrows Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

INCPATH  += -I.
HOST_CFLAGS = -g $(INCPATH) -MD -Wall

# we support three object formats at this point
# 1) ELF32
# 2) ELF64
# 3) XCOFF


ifeq ($(PLATFORM_OS),AIX)
TARGETS = ext_xcoff ext_elf ext_ppc64 ext_elf64
HOST_CFLAGS += -I$(SRC_DIR_PATH)/../elf -DGCC_CLASS_VERSION=3
ext_xcoff: ext_xcoff.o
	$(HOST_CC) $(HOST_CFLAGS) $< -lld  -o $@
ext_elf: ext_elf.o
	$(HOST_CC) $(HOST_CFLAGS) -I$(SRC_DIR_PATH)/../elf  $< -L../elf -lelf -o $@
ext_ppc64: ext_ppc64.o
	$(HOST_CC) $(HOST_CFLAGS) -I$(SRC_DIR_PATH)/../elf  $< -L../elf -lelf64 -o $@
ext_elf64: ext_elf64.o ../elf/libelf64.a
	$(HOST_CC) $(HOST_CFLAGS) $< ../elf/libelf64.a -o $@
# See comments in ../elf about making every time
	$(RM) ext_elf64.o ../elf/libelf64.a
endif

ifeq ($(PLATFORM_OS),IRIX64)
TARGETS = ext_elf64
HOST_CFLAGS += -DGCC_CLASS_VERSION=2
ext_elf64: ext_elf64.o 
	$(HOST_CC) $(HOST_CFLAGS) $< -lelf -o $@
endif

ifeq ($(PLATFORM_OS),Linux)
TARGETS = ext_elf64
HOST_CFLAGS += -I$(SRC_DIR_PATH)/../elf -DGCC_CLASS_VERSION=3
ext_elf64: ext_elf64.o ../elf/libelf64.a
	$(HOST_CC) $(HOST_CFLAGS) $< ../elf/libelf64.a -o $@
# See comments in ../elf about making every time
	$(RM) ext_elf64.o ../elf/libelf64.a
endif

ifeq ($(PLATFORM_OS),Darwin)
TARGETS = ext_elf64
HOST_CFLAGS += -I$(SRC_DIR_PATH)/../elf -DGCC_CLASS_VERSION=3
ext_elf64: ext_elf64.o ../elf/libelf64.a
	$(HOST_CC) $(HOST_CFLAGS) $< ../elf/libelf64.a -o $@
endif

# This should really ask the target compiler
ifdef GCC3
HOST_CFLAGS += -DGCC_CLASS_VERSION=3
endif

$(MKTOOLBIN)/stubgen:
	$(INSTALL) -m 0775 -d $@

install_targets:: $(MKTOOLBIN)/stubgen $(TARGETS)
	$(INSTALL) -m 0555 $(TARGETS) $<


clean::
	-$(RM) $(TARGETS:%=%.o)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
