# ############################################################################
# K42: (C) Copyright IBM Corp. 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: stdhdr.make,v 1.19 2001/10/31 07:57:25 okrieg Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)lib/Makerules.kitch

# for installing include files
INCDIR   = $(MKKITCHTOP)include/
INCFILES = 
ARCHINCS = 

install_includes::
	$(INSTALL_INC) $(INCDIR)            ./        ${INCFILES}

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

LIBCSRCS +=
LIBKSRCS +=
CLASSES  +=
SRCS     +=
TARGETS  +=

# for kernel modules under the os/kernel tree
LIBKERNSRCS +=

# for server modules under the os/server tree
SERVERS     +=


# ############################################################################
#  end of object directory rules
# ############################################################################
endif



