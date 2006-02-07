# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.10 2005/07/10 15:46:30 dilma Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=0
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

fast::
	$(MAKE) install_targets
	cd ../.. && $(MAKE) install_targets

#
# Literals used in the code
#
COMPILER_VERSION='"gcc version 3.3.3 20031029 (prerelease)"'

CHOST:=$(shell hostname)
FileSystemProc_FLAGS+=-DLINUX_COMPILE_HOST='"$(CHOST)"'
FileSystemProc_FLAGS+=-DCOMPILER_VERSION=$(COMPILER_VERSION)


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

SERVERS += procfsServer
SRCS += main.C FileSystemProc.C ProcFileMeminfo.C ProcFileMounts.C

SERV_LIBS = -lvirtfs -lfs
SERV_DEPS = $(LIB_VFS) $(LIB_FS)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
