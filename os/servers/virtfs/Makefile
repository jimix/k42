# ############################################################################
# K42: (C) Copyright IBM Corp. 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.9 2005/06/28 19:48:22 rosnbrg Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

CLASSES += 

fast::
	$(MAKE) install_targets
	cd ../.. && $(MAKE) install_targets

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
targets:: $(UTILS) $(SERVERS)

TARGET_CFLAGS += $($@_CFLAGS)
SERVERS += 
SRCS += 
include $(SRC_DIR_PATH)/Make.files

#------------------------- stuff needed for linking to glibc -------------

SERV_LIBS = -lfs
SERV_DEPS = $(LIB_FS)

#------------------------- stuff needed for linking to glibc -------------

# ############################################################################
#  end of object directory rules
# ############################################################################
endif


