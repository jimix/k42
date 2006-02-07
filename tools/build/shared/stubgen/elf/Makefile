#
############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.17 2004/06/28 17:02:13 rosnbrg Exp $
# ############################################################################

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools
HOST_CFLAGS = -I. -g -MD

SRCS = elflib.c elflib64.c

OBJS = $(SRCS:%.c=%.o)

ifdef IN_OBJ_DIR

$(OBJS)::

#We should make these tools in platform specific directories
#For now, just build them every time we build tools and
#Through away everything after its used.
#See ../extract
#MAA Jan 18, 2004
libs:: $(OBJS)
	$(RM) libelf.a libelf64.a
	$(AR) cruvs libelf.a elflib.o
	$(AR) cruvs libelf64.a elflib64.o
	$(RM) $(OBJS)

clean::
	-$(RM) libelf.a libelf64.a $(OBJS)

install_targets:: libs

install_includes::

endif
