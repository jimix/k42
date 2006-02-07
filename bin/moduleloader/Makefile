# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.4 2005/07/18 23:44:19 awaterl Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel

KERNEL=0
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

USRTARGETS = ModuleLoader

BIN_FILES = Object.so

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS  += $(USRTARGETS) $(BIN_FILES)

CLASSES += 

SERVE	+= 

ROOT_FILES = boot_image.map

boot_image.map: $(MKANCHOR)/$(TARGET_MACHINE)/$(OPTIMIZATION)/os/boot_image.dbg 
	 $(MKTOOLBIN)/genmap $< > $@

MODULE_OBJS = Object.o

Object.so: $(MODULE_OBJS)
	$(LINK.c) -nostdlib -shared $< -fno-exceptions -o $@

OBJS = $(USRTARGETS:%=%.o) $(MODULE_OBJS)

ModuleLoader_OBJS = Loader.o SymbolResolver.o
OBJS += $(ModuleLoader_OBJS)
ModuleLoader: $(ModuleLoader_OBJS)

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
		-lk42sys -o $@
	$(STRIP.dbg)

INST_PROGS =	$(USRTARGETS:%=$(MKKITCHROOT)/kbin/%) \
		$(BIN_FILES:%=$(MKKITCHROOT)/kbin/%)

INST_FILES =	$(ROOT_FILES:%=$(MKKITCHROOT)/kbin/%)		

# Different patterns for installing script files.
$(MKKITCHROOT)/kbin/test:
	$(INSTALL) --mode 0775 -d $@

$(MKKITCHROOT)/kbin/%: $(SRC_DIR_PATH)/%
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(INST_FILES): $(MKKITCHROOT)/kbin/%: %
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(INST_PROGS): $(MKKITCHROOT)/kbin/%: %
	$(INSTALL) --mode 0755 $< $(@D)
	$(INSTALL_DSK)

install_targets:: $(INST_PROGS) $(INST_FILES)

XTRA_CLEAN=	$(CLASSES:%=%.o)	\
		$(CLASSES:%=%.d)	\
		$(USRTARGETS:%=%.dbg)

clean::
	-$(RM) $(XTRA_CLEAN)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
