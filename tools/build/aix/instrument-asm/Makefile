# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.3 2001/06/19 17:21:58 peterson Exp $
# ############################################################################

default:: targets

-include Make.config
include $(MKKITCHTOP)/lib/Makerules.tools

ifdef IN_OBJ_DIR
TARGETS += instrument-asm

HOST_CFLAGS = -O

OBJS = instrument-asm.o

instrument-asm: $(OBJS)
	$(HOST_CC) $^  -o $@

install_targets:: $(TARGETS)
	$(INSTALL) --mode 0555 $(TARGETS) $(MKTOOLBIN)

clean::
	-$(RM) $(OBJS)
endif


