# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.1 2005/08/22 19:43:36 bob Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel

KERNEL=0
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

OBJSTARGETS += hello

USRTARGETS+=$(OBJSTARGETS)


ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################
CPPFLAGS += -isystem $(KITCH_LINUX)

#TARGETS  += $(USRTARGETS)
#this is potentially temporary and is used to make a linux sandbox .o
TARGETS  += $(USRTARGETS) crtTrace.o
TARGETS  += $(USRTARGETS) pemMemWrappers.o

DYNAMIC_LINK+= $(TARGETS)

CLASSES += UsrTst

SERVE	+= RemoteIPCTst

# need to relink if any of these change
DEPLIBS= $(KITCH_GLIBC)
NORM_LIBS= $(K42_LDFLAGS) $(K42LIBS)

# testDisabledIPC is a program with more than one .o
DISABLED_IPC_OBJS= testDisabledIPC.o UsrTst.o

# testRemoteIPC is a program with more than one .o
REMOTE_IPC_OBJS= testRemoteIPC.o XRemoteIPCTst.o

# dcotest is a program with more than one .o
DCO_OBJS= dcotest.o intctr.o intctrSharedNotLocked.o
DCOL_OBJS= dcotest.o intctr.o intctrSharedLocked.o

# miptest also needs more than one .o
#MIP_OBJS=miptest.o libmip.o k42mip.o

# ...and the tracing tools
TRACETOOL_OBJS = traceTool.o traceUtils.o traceCommon.o
TRACEPROF_OBJS = traceProfile.o traceUtils.o traceCommon.o

OBJS=$(OBJSTARGETS:%=%.o) $(DISABLED_IPC_OBJS) $(REMOTE_IPC_OBJS) \
	$(INTERPOSER_OBJS) $(DCO_OBJS) $(DCOL_OBJS) $(TRACETOOL_OBJS) \
	$(TRACEPROF_OBJS) # $(MIP_OBJS)

# Most of the OBJS are intermediate and will be removed after building,
# we want to keep them so relinking will not cause the source to be
# recompiled.
.SECONDARY: $(OBJS)

# Catch ALL for one object per executable
%: %.o $(DEPLIBS)	
	$(LINK.cc) $< $(NORM_LIBS) -o $@
	$(STRIP.dbg)

newmemclone: newmemclone.o $(DEPLIBS)	
	$(LINK.cc) $< $(NORM_LIBS) -lpthread -o $@
	$(STRIP.dbg)

testDisabledIPC: $(DISABLED_IPC_OBJS) $(DEPLIBS)
	$(LINK.c) $(DISABLED_IPC_OBJS) $(NORM_LIBS) -o $@
	$(STRIP.dbg)

testRemoteIPC: $(REMOTE_IPC_OBJS) $(DEPLIBS)
	$(LINK.c) $(REMOTE_IPC_OBJS) $(NORM_LIBS) -o $@
	$(STRIP.dbg)

dcotest: $(DCO_OBJS) $(DEPLIBS)
	$(LINK.c) $(DCO_OBJS) $(NORM_LIBS) -o $@
	$(STRIP.dbg)

dcotestl: $(DCOL_OBJS) $(DEPLIBS)
	$(LINK.c) $(DCOL_OBJS) $(NORM_LIBS) -o $@
	$(STRIP.dbg)

#miptest: $(MIP_OBJS) $(DEPLIBS)
#	$(LINK.c) $(MIP_OBJS) $(NORM_LIBS) -o $@
#	$(STRIP.dbg)

# the tracing tools are built from K42 from source in the tools tree, so
# we need these explicit rules to build them
%.o: $(MKSRCTOP)/tools/misc/kmon/%.C
	$(TARGET_CXX) -c $(TARGET_CXXFLAGS) $(CPPFLAGS) $< -o $@ $(CLEANUP)

traceUtils.o: $(MKSRCTOP)/os/kernel/trace/traceUtils.C
	$(TARGET_CXX) -c $(TARGET_CXXFLAGS) $(CPPFLAGS) $< -o $@ $(CLEANUP)

traceTool: $(TRACETOOL_OBJS) $(DEPLIBS)
	$(LINK.c) $(TRACETOOL_OBJS) $(NORM_LIBS) -o $@
	$(STRIP.dbg)

traceProfile: $(TRACEPROF_OBJS) $(DEPLIBS)
	$(LINK.c) $(TRACEPROF_OBJS) $(NORM_LIBS) -o $@
	$(STRIP.dbg)


ROOT_FILES = hithere
BIN_FILES = abin regression-script pmu.sh pmu_run.sh
TEST_FILES = atest

INST_PROGS =	$(USRTARGETS:%=$(MKKITCHROOT)/kbin/%) \
		$(BIN_FILES:%=$(MKKITCHROOT)/kbin/%)

INST_FILES =	$(ROOT_FILES:%=$(MKKITCHROOT)/kbin/%)		

INST_TEST_FILES = $(TEST_FILES:%=$(MKKITCHROOT)/kbin/test/%)

# Different patterns for installing script files.
$(MKKITCHROOT)/kbin/test:
	$(INSTALL) --mode 0775 -d $@

$(INST_TEST_FILES): $(MKKITCHROOT)/kbin/test/%: $(SRC_DIR_PATH)/% $(MKKITCHROOT)/kbin/test
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(MKKITCHROOT)/kbin/%: $(SRC_DIR_PATH)/%
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(INST_FILES): $(MKKITCHROOT)/kbin/%: %
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(INST_PROGS): $(MKKITCHROOT)/kbin/%: %
	$(INSTALL) --mode 0755 $< $(@D)
	$(INSTALL_DSK)

install_targets:: $(INST_PROGS) $(INST_FILES) $(INST_TEST_FILES)

XTRA_CLEAN=	$(CLASSES:%=%.o)	\
		$(CLASSES:%=%.d)	\
		$(USRTARGETS:%=%.dbg)

clean::
	-$(RM) $(XTRA_CLEAN)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
