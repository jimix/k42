# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.140 2005/08/22 21:49:00 bob Exp $
# ############################################################################

SUBDIRS =

# set KERNEL=1 if for kernel

KERNEL=0
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

default:: targets

USRTARGETS = hello mmap-ex echosrv UDPTest yieldtest vptest testPageOut \
	dcofcm mtprog traceAddEvent threadtest while1 craig printStat \
	semTest testMigrateVP resMgrStats testMountPointMgr testSymLink \
	testTrace multiDispatcher testIOMapping diskTest \
	sockTestSrv sysCmd testAlignment dcotest dcotestl testDisabledIPC \
	testRemoteIPC printtime awaittime spread hwPerf\
	# miptest

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

TARGETS  += $(USRTARGETS)

CLASSES += UsrTst

SERVE	+= RemoteIPCTst

OBJS = $(USRTARGETS:%=%.o)

testDisabledIPC_OBJS = UsrTst.o
OBJS += $(testDisabledIPC_OBJS)
testDisabledIPC: $(testDisabledIPC_OBJS)

testRemoteIPC_OBJS = XRemoteIPCTst.o
OBJS += $(testRemoteIPC_OBJS)
testRemoteIPC: $(testRemoteIPC_OBJS)

dcotest_OBJS = intctr.o intctrSharedNotLocked.o
OBJS += $(dcotest_OBJS)
dcotest: $(dcotest_OBJS)

dcotestl_OBJS = intctr.o intctrSharedLocked.o
OBJS += $(dcotestl_OBJS)
dcotestl: $(dcotestl_OBJS)
dcotestl.o: dcotest.o		# dcotestl uses dcotest's main program
	cp -p dcotest.o dcotestl.o

#miptest_OBJS = libmip.o k42mip.o
#OBJS += $(miptest_OBJS)
#miptest: $(miptest_OBJS)

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

ROOT_FILES = hithere
BIN_FILES = abin regression-script pmu.sh pmu_run.sh
TEST_FILES = atest

INST_PROGS =	$(USRTARGETS:%=$(MKKITCHROOT)/tests/native/%) \
		$(BIN_FILES:%=$(MKKITCHROOT)/tests/native/%)

INST_FILES =	$(ROOT_FILES:%=$(MKKITCHROOT)/tests/native/%)		

INST_TEST_FILES = $(TEST_FILES:%=$(MKKITCHROOT)/tests/native/test/%)

# Different patterns for installing script files.
$(MKKITCHROOT)/tests/native:
	$(INSTALL) --mode 0775 -d $@

$(MKKITCHROOT)/tests/native/test:
	$(INSTALL) --mode 0775 -d $@

$(INST_TEST_FILES): $(MKKITCHROOT)/tests/native/test/%: \
			$(SRC_DIR_PATH)/% $(MKKITCHROOT)/tests/native/test
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(MKKITCHROOT)/tests/native/%: $(SRC_DIR_PATH)/% $(MKKITCHROOT)/tests/native
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(INST_FILES): $(MKKITCHROOT)/tests/native/%: % $(MKKITCHROOT)/tests/native
	$(INSTALL_INC) $(@D) $(<D) $(<F)
	$(INSTALL_DSK)

$(INST_PROGS): $(MKKITCHROOT)/tests/native/%: % $(MKKITCHROOT)/tests/native
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
