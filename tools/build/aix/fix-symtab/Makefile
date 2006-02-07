# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.3 2001/06/19 17:21:56 peterson Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

ifdef IN_OBJ_DIR
TARGETS += fix-symtab

HOST_CFLAGS = -O -D__XCOFF64__

OBJS = \
        main.o \
        misc.o \
        strtab.o \
        symbols.o

fix-symtab: $(OBJS)
	$(HOST_CC) $^  -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)

clean::
	-$(RM) $(OBJS)
endif
