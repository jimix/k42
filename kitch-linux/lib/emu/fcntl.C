/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fcntl.C,v 1.41 2005/04/11 00:11:55 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: manipulate file descriptor
 * **************************************************************************/

// Needed in order to support the superset of #defs
#include <sys/sysIncs.H>
#include <sys/fcntl.h>
#include <misc/baseStdio.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <unistd.h>
#include <sys/fcntl.h>
#include <stdarg.h>



static int
DupFileDescriptor(int fd, int newfd)
{
    FileLinuxRef fileRef;
    FileLinuxRef newFileRef;
    SysStatus rc;

    // We must check for a negative value here because after this
    // it is unsigned.
    if (newfd < 0) {
	return -EINVAL;
    }

    fileRef = _FD::GetFD(fd);
    if (!fileRef) {
	return -EBADF;
    }

    rc = DREF(fileRef)->dup(newFileRef);

    if (_FAILURE(rc)) {
	newfd = -_SGENCD(rc);
    } else {
	// use a new fd greater than or equal to newfd
	newfd = _FD::AllocFD(newFileRef, newfd);
	if (newfd == -1) {
	    newfd = -EMFILE;
	    DREF(newFileRef)->destroy();
	}
    }

    return (newfd);
}


static int
GetOpenFlags(int fd)
{
    int flags;
    FileLinuxRef fileRef;

    fileRef = _FD::GetFD(fd);
    if (fileRef) {
	flags = (int)DREF(fileRef)->getFlags();
    } else {
	flags = -EBADF;
    }

    return flags;
}

static int
SetOpenFlags(int fd, int flags)
{
    FileLinuxRef fileRef;
    uval set, clr;
    // Only O_APPEND, O_NONBLOCK and O_ASYNC may be set; the other
    // flags are unaffected.
    set = flags & (O_APPEND | O_NONBLOCK | O_ASYNC);
    clr = (~set) & (O_APPEND | O_NONBLOCK | O_ASYNC);
    fileRef = _FD::GetFD(fd);
    if (fileRef) {
	DREF(fileRef)->modFlags(set,clr);
	return 0;
    }

    return -EBADF;
}

static int
FileLocks(int fd, int cmd, struct flock &fileLock)
{
    int r = 0;
    FileLinuxRef fileRef;
    SysStatus rc;

    fileRef = _FD::GetFD(fd);
    if (fileRef) {
	switch (cmd) {
	default: // imposible, but tells compiler that rc will be assigned
	case F_GETLK:
	    rc = DREF(fileRef)->getLock(fileLock);
	    break;
	case F_SETLK:
	    rc = DREF(fileRef)->setLock(fileLock);
	    break;
	case F_SETLKW:
	    rc = DREF(fileRef)->setLockWait(fileLock);
	    break;
	}
	if (_FAILURE(rc)) {
	    r = -_SGENCD(rc);
	}
    } else {
	r = -EBADF;
    }
    return r;
}


/*
 * common routine to 32 bit and 64 bit 
 */
static int
baseFcntl(int fd, int cmd, va_list ap)
{
    switch (cmd) {
    case F_DUPFD: {
	int newfd = va_arg(ap, int);
	return DupFileDescriptor(fd, newfd);
    }
    case F_GETFD:
	return  _FD::GetCOE(fd);
    case F_SETFD: {
	int arg = va_arg(ap, int);
	_FD::SetCOE(fd, (arg & FD_CLOEXEC));
	return 0;
    }
    case F_GETFL:
	return GetOpenFlags(fd);
    case F_SETFL: {
	int flags = va_arg(ap, int);
	return SetOpenFlags(fd, flags);
    }
    case F_GETLK: case F_SETLK: case F_SETLKW:
    case F_GETLK64: case F_SETLK64: case F_SETLKW64: {
	struct flock *flockp;
	flockp = va_arg(ap, struct flock *);
	return FileLocks(fd, cmd, *flockp);
    }
    case F_GETOWN: case F_SETOWN:	// BSDisms no support under SVr4
    case F_GETSIG: case F_SETSIG:	// specific to Linux
	err_printf("%s: not yet implemented (%d)!\n", 
		   __PRETTY_FUNCTION__, cmd);
	return -ENOSYS;
	break;
    default:
	err_printf("%s: No such command (%d)!\n", 
		   __PRETTY_FUNCTION__, cmd);
	return -EINVAL;
    }
}

/*
 * We do NOT support all possibles
 */
int
__k42_linux_fcntl (int fd, int cmd, ...)
{
    int ret;

    SYSCALL_ENTER();

    va_list ap;

    if (!_FD::GetFD(fd)) {
	ret = -EBADF;
	SYSCALL_EXIT();
	return ret;
    }

    va_start(ap, cmd);
    ret = baseFcntl(fd, cmd, ap);
    va_end(ap);


    SYSCALL_EXIT();
    return ret;
}

extern "C" long
__k42_linux_fcntl_32 (int fd, int cmd, ...)
{
    int ret;
    SYSCALL_ENTER();
    va_list ap;

    if (!_FD::GetFD(fd)) {
	ret = -EBADF;
	SYSCALL_EXIT();
	return ret;
    }

    /* The fcntl call is variadic, and we want to catch the case where
     * the third argument is a struct flock, since we need to do some
     * 32-bit mangling there.
     */
    switch (cmd) {
    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK64:
    case F_SETLK64:
    case F_SETLKW64:
        /* Compatibility structure for 32-bit userspace.  */
        struct flock32 {
	  uval16 l_type;
	  uval16 l_whence;
	  uval32 l_start;
	  uval32 l_len;
	  uval32 l_pid;
	};

	struct flock32 *flockp;
	struct flock fileLock;

	/* Get the 32-bit userspace representation of the flock structure.  */
        va_start(ap, cmd);
	flockp = va_arg(ap, struct flock32 *);
	va_end(ap);

	/* Make 64-bit version of flock structure.  */
	fileLock.l_type = flockp->l_type;
	fileLock.l_whence = flockp->l_whence;
	fileLock.l_start = ZERO_EXT(flockp->l_start);
	fileLock.l_len = ZERO_EXT(flockp->l_len);
	fileLock.l_pid = flockp->l_pid;

	/* Fire off the 64-bit lock function.  */
	ret = FileLocks(fd, cmd, fileLock);
	break;
    default:
	va_start(ap, cmd);
	ret = baseFcntl(fd, cmd, ap);
	va_end(ap);
	break;
    }
    SYSCALL_EXIT();
    return ret;
}

strong_alias(__k42_linux_fcntl, __k42_linux_fcntl64);

