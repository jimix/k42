# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: Makefile,v 1.83 2005/08/31 13:55:59 dgurley Exp $
# ############################################################################

default:: libobjs

# set KERNEL=1 if for kernel
KERNEL=
-include Make.config

include $(MKKITCHTOP)/lib/Makerules.kitch

COMMINCS += FD.H SignalUtils.H sandbox.H stat.H

install_includes::
	$(INSTALL_INC)  $(INCDIR)/emu/            ./            $(COMMINCS)

#
# Linux headers First
#
INCPATH := $(GCCFIXINC) $(KITCH_GINC) $(INCPATH)

ifdef IN_OBJ_DIR
# ############################################################################
#  rules to be made in object directory
# ############################################################################

LIBEMUSRCS +=   getdents64.C	\
		ftruncate64.C	\
		truncate64.C

LIBEMUSRCS +=	FD.C 		\
		linuxLibInit.C	\
		linuxSpawn.C	\
		linuxEmul.C	\
		linuxFileIO.C	\
		linuxSocket.C	\
		_llseek.C	\
		accept.C	\
		access.C	\
		alarm.C		\
		bind.C		\
		brk.C		\
		chdir.C		\
		chmod.C		\
		chown.C		\
		chroot.C	\
		clone.C		\
		close.C		\
		connect.C	\
		dup.C		\
		dup2.C		\
		execve.C	\
		exit.C          \
		fchdir.C	\
		fchmod.C	\
		fchown.C	\
		fcntl.C		\
		fdatasync.C	\
		flock.C		\
		fork.C		\
		fsync.C		\
		ftruncate.C	\
		getcwd.C	\
		getpagesize.C	\
		getpeername.C	\
		getpid.C	\
		getrlimit.C	\
		getsockname.C	\
		gettimeofday.C	\
		ioctl.C		\
		ipc.C		\
		kill.C          \
		lchown.C	\
		link.C		\
		listen.C	\
		lseek.C		\
		mkdir.C		\
		mknod.C		\
		mmap.C		\
		msg.C		\
		nanosleep.C	\
		setrlimit.C	\
		open.C		\
		personality.C	\
		pipe.C		\
		poll.C		\
		pread.C		\
		ptrace.C	\
		pwrite.C	\
		read.C		\
		readlink.C	\
		readv.C		\
		recv.C		\
		recvfrom.C	\
		recvmsg.C	\
		rename.C	\
		rmdir.C		\
		sched_yield.C	\
		select.C	\
		semctl.C	\
		semget.C	\
		semop.C		\
		send.C		\
		sendfile.C	\
		sendto.C	\
		sendmsg.C	\
		sethostname.C	\
		setsockopt.C    \
		settimeofday.C	\
		setuid.C        \
		shmat.C		\
		shmctl.C	\
		shmdt.C		\
		shmget.C	\
		shutdown.C	\
		socket.C	\
		socketcall.C	\
		statfs.C	\
		stime.C		\
		symlink.C	\
		sync.C          \
		sysctl.C        \
		time.C		\
		times.C		\
		truncate.C	\
		umask.C		\
		uname.C		\
		unlink.C	\
		utimes.C        \
		utime.C		\
		vfork.C		\
		wait4.C		\
		write.C		\
		writev.C	\
		dostat.C	\
		dofstat.C	\
		dolstat.C	\
		stat.C          \
		getpmsg.C	\
		clock_gettime.C \
		mount.C

COMPILE_DATE='"$(shell date)"'
TARGET_CXXFLAGS+=-DCOMPILE_DATE=$(COMPILE_DATE)

# ############################################################################
#  end of object directory rules
# ############################################################################
endif
