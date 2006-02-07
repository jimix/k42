# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
# All Rights Reserved
#
#  $Id: Makefile,v 1.125 2005/08/19 01:40:40 butrico Exp $
# ############################################################################

default:: install_targets

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config
include $(MKKITCHTOP)/lib/Makerules.kitch

TESTDIR=linux

SRCS=		copy.c	\
		dev.c \
		zval.c \
		hello.c \
		forktest.c \
		jmptest.c \
		stat.c \
		args.c \
		socket.c \
		dir.c \
		select.c \
		pipe_select.c \
		chmod.c \
		chown.c \
		truncate.c \
		pipepoll.c \
		segv.c \
		traptest.c \
		access.c \
		readModFile.c \
		link.c \
		unlink.c \
		appendLseek.c \
		openUnlink.c \
		openUnlink2.c \
		netperf.c \
		openFlags.c \
		openWriteFork.c \
		openReadForkRead.c \
		disktest.c \
		shmop.c	\
		fcntl.c	\
		mmap.c \
		rename.c \
		nfsCaching.c \
		fileSharing.c \
		smallFiles.c \
		marc.c \
		shmattest.c \
		fileStream.c \
		memScan.c \
		rmdir.c \
		staticSyscall.c \
		preadwrite.c \
		utime.c \
                nanosleep.c \
		daytime-client.c \
		daytime-server.c \
		echo-client.c \
		echo-server-udp.c \
		echo-client-udp.c \
                statfs.c \
                sethostname.c \
		pwrite.c \
		ptrace.c \
		lstat.c \
		readwritev.c \
		select_simple.c \
		truncate_largefile.c \
		socket2.c \
		clock_gettime.c	\
		faults.c \
		daemon.c \
		simple_sendrecvmesg.c \
		send_socket_test.c \
		alarm.c \
		send_file_socket_test.c \
		select_files_devices.c	\
		send_socket_test2.c \
		gettimeofday.c \
		fsync.c


SRCS32=		access.c \
                nanosleep.c \
		preadwrite.c \

CXXSRCS=	netperf2.C \
		memtouch.C

CTARGETS = $(basename $(SRCS))
CTARGETS32 = $(SRCS32:%.c=%32)
CXXTARGETS = $(basename $(CXXSRCS))
USRTARGETS=	$(CTARGETS) $(CXXTARGETS) $(CTARGETS32) multiseg


#marc \
#		fileStream memScan #my-postmark-1_5 

ifdef IN_OBJ_DIR
SCRIPTS=	regress.sh virtfs-regress.sh fileutils-test.sh unionfs.sh \
		zemit zemit-start zemit-stop ltp.sh micro_bench.sh
#		runpm runpm1 \
#		runpm1-sm-25times runpm1-sm-5times runpm1-25times runpm1-5times\
#		runpm-25times runpm-5times

# Targets that need pthreads
LINK_PTHREAD = select pipe_select pipepoll netperf netperf2

include $(SRC_DIR_PATH)/../Makefile.tests

$(LINK_PTHREAD): TEST_LDLIBS += -lpthread

clock_gettime clock_gettime32: TEST_LDLIBS += -lrt
traptest: TEST_LDLIBS += -lm

OPT_FLAGS+=-MD

%: %.c
	$(TARGET_CC) $< $(OPT_FLAGS) $(WARN_FLAGS) $(TEST_LDLIBS) -o $@
	$(STRIP.dbg)

%32: %.c
	$(TARGET32_CC) $< $(OPT_FLAGS) $(WARN_FLAGS) \
		-static $(TEST_LDLIBS) -o $@
	$(STRIP32.dbg)

%: %.C
	$(TARGET_CXX) $< $(OPT_FLAGS) $(WARN_FLAGS) $(TEST_LDLIBS) -o $@
	$(STRIP.dbg)

multiseg:	multiseg.c
	$(TARGET_CC) $< $(OPT_FLAGS) $(WARN_FLAGS) \
		-Wl,-Tdata,0x20000000 $(TEST_LDLIBS) -o $@

my-postmark.o: my-postmark.C
	$(TARGET_CC) $(TFLAGS) -c $(TARGET_CXXFLAGS) $(CPPFLAGS) $< -o $@


clean::
	-$(RM) $(TRACE_OBJS) $(FAKETARGETS:%=%.d)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
