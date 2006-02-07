# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.29 2005/07/18 23:44:19 awaterl Exp $
# ############################################################################

#SUBDIRS = moduleloader

# set KERNEL=1 if for kernel

KERNEL=0
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

USRTARGETS = sysctl

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS  += $(USRTARGETS)

OBJS = $(USRTARGETS:%=%.o)

sysctl_OBJS = console.o swapdevs.o breakpoint.o proccount.o
OBJS += $(sysctl_OBJS)
sysctl: $(sysctl_OBJS)
sysctl_INIT_DECLS = ${sysctl_OBJS:%.o=DECLARE_INIT(%_init);}
sysctl_INIT_FUNCS = $(sysctl_OBJS:%.o=%_init,) NULL
sysctl_CXXFLAGS = -DINIT_FUNCS='$(sysctl_INIT_FUNCS)' \
			-DINIT_DECLS='$(sysctl_INIT_DECLS)'
sysctl.o: TARGET_CXXFLAGS += $(sysctl_CXXFLAGS)
sysctl_LDFLAGS = -L$(LIBDIR) -lvirtfs -lfs
sysctl: $(LIB_VFS) $(LIB_FS)

# Most of the OBJS are intermediate and will be removed after building,
# we want to keep them so relinking will not cause the source to be
# recompiled.
.SECONDARY: $(OBJS)

%: %.o
	$(LINK.cc) $< $($@_OBJS) \
		-Wl,-dynamic-linker,/usr/klib/ld64.so.1 \
		-Wl,-rpath,/klib:/usr/klib \
		-L$(K42_LIBDIR) \
		-L$(KITCH_GLIB) \
		-Wl,-rpath-link,$(KITCH_GLIB) \
		$($@_LDFLAGS) -lk42sys -o $@
	$(STRIP.dbg)

INST_PROGS = $(USRTARGETS:%=$(MKKITCHROOT)/kbin/%)

$(MKKITCHROOT)/kbin/%: %
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

# scripts get installed directly from the src tree
$(MKKITCHROOT)/kbin/%: $(SRC_DIR_PATH)/%
	$(INSTALL) --mode 0755 $< $@
	$(INSTALL_DSK)

install_targets:: $(INST_PROGS)

XTRA_CLEAN = $(USRTARGETS:%=%.dbg)

clean::
	-$(RM) $(XTRA_CLEAN)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
