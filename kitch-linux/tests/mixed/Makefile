# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.33 2005/07/18 23:44:19 awaterl Exp $
# ############################################################################

SUBDIRS =

default:: targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

USRTARGETS =	newmemclone spin nullProg micro3_child \
		runchild IOCPUMigTst micro micro2 micro3 migrate contend \
		multiProc multiProcForkJoin multiProcMigrate

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

OBJS = $(USRTARGETS:%=%.o)

TARGETS  += $(USRTARGETS)


%: %.o
	$(LINK.c) $<  -Wl,-rpath,/klib \
		-L$(K42_LIBDIR) \
		-lk42sys -lpthread -o $@
	$(STRIP.dbg)

INST_PROGS =	$(USRTARGETS:%=$(MKKITCHROOT)/tests/mixed/%)

$(MKKITCHROOT)/tests/mixed/%: %
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

$(MKKITCHROOT)/tests/mixed:
	$(INSTALL) --mode 0775 -d $@

install_targets:: $(MKKITCHROOT)/tests/mixed
install_targets:: $(INST_PROGS)

clean::
	-$(RM) $(USRTARGETS:%=%.dbg)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
