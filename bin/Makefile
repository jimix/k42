# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.31 2005/07/21 13:57:06 dilma Exp $
# ############################################################################

SUBDIRS = sysctl # moduleloader

# set KERNEL=1 if for kernel

KERNEL=0
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

USRTARGETS = kernelControl traceControl \
		thinenv runprog ppCount traceDump traceTool traceProfile \
		bindFS discardCachedPages dqmon kmount kfsControl longexec \
		modifyBootParms tracedStop

SCRIPTS = sysinit

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS += $(USRTARGETS)

#these are potentially temporary and are used to make a linux sandbox .o
TARGETS += crtTrace.o pemMemWrappers.o

OBJS = $(USRTARGETS:%=%.o)

traceDump_OBJS = traceUtils.o
OBJS += $(traceDump_OBJS)
traceDump: $(traceDump_OBJS)

traceProfile_OBJS = traceUtils.o traceCommon.o
OBJS += $(traceProfile_OBJS)
traceProfile: $(traceProfile_OBJS)

traceTool_OBJS = traceUtils.o traceCommon.o
OBJS += $(traceTool_OBJS)
traceTool: $(traceTool_OBJS)

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

# the tracing tools are built from K42 from source in the tools tree, so
# we need these explicit rules to build them
%.o: $(MKSRCTOP)/tools/misc/kmon/%.C
	$(TARGET_CXX) -c $(TARGET_CXXFLAGS) $(CPPFLAGS) $< -o $@ $(CLEANUP)

traceUtils.o: $(MKSRCTOP)/os/kernel/trace/traceUtils.C
	$(TARGET_CXX) -c $(TARGET_CXXFLAGS) $(CPPFLAGS) $< -o $@ $(CLEANUP)

INST_PROGS = $(USRTARGETS:%=$(MKKITCHROOT)/kbin/%) \
		$(SCRIPTS:%=$(MKKITCHROOT)/kbin/%)

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
