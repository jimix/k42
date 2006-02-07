# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.34 2004/06/28 17:01:32 rosnbrg Exp $
# ############################################################################

default:: libobjs

SUBDIRS =

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

STDINCS  =  string.h ctype.h limits.h float.h
ARCHINCS =

install_includes::
	$(INSTALL_INC) $(MKKITCHTOP)/include/           ./           $(STDINCS)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

# so that I can get definitions from io directory
INCPATH   += -I$(SRC_DIR_PATH)/..

COMMONSRCS = baseSprintf.C

LIBNATVSRCS += isctype.C memccpy.C memcmp.C string.C baseAtoi.c baseStrtol.c baseFgets.C 
LIBCSRCS += $(COMMONSRCS) chario.C
LIBKSRCS += $(COMMONSRCS) $(LIBNATVSRCS)
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
