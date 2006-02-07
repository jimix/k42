# ############################################################################
# K42: (C) Copyright IBM Corp. 2001, 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

SERVE += FileSystemK42RamFS

fast::
	$(MAKE) install_targets
	cd ../.. && $(MAKE) install_targets

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
targets:: $(SERVERS) $(UTILS)

TARGET_CFLAGS += $($@_CFLAGS)
SERVERS += k42RamfsServer
UTILS += 
SRCS += main.C $(SRVSRCS)
#SRCS += mainWithoutForking.C

include $(SRC_DIR_PATH)/Make.files

# since not automatically part of OBJ, FIXME??
-include tst.d

#------------------------- stuff needed for linking to glibc -------------
SERV_LIBS = -lfs -llk26
SERV_DEPS = $(LIB_FS) $(LIB_LK)

# How to install utilities
INST_PROGS = $(UTILS:%=$(MKKITCHROOT)/kbin/%)

$(MKKITCHROOT)/kbin/%: % $(MKKITCHROOT)/kbin
	$(INSTALL) --mode 0755 $< $(@D)

# ############################################################################
#  end of object directory rules
# ############################################################################

endif

