/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: readv.C,v 1.11 2005/07/15 17:14:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <sys/uio.h>

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

// #define CHEESE_EMULATION
#ifdef CHEESE_EMULATION
#include <string.h>
#define readv __k42_linux_readv
extern "C" ssize_t __k42_linux_read(int, void *, size_t);

ssize_t
readv(int fd, const struct iovec *vector, int count)
{
#undef readv

    char *buffer;
    register char *bp;
    size_t bytes, to_copy;
    ssize_t bytes_read;
    int i;

    /* Find the total number of bytes to be read.  */
    bytes = 0;
    for (i = 0; i < count; ++i) {
	bytes += vector[i].iov_len;
    }

    /* Allocate a temporary buffer to hold the data.  */
    if (bytes < PAGE_SIZE) {
	// Use the stack.
	buffer = (char *) alloca (bytes);
    } else {
	buffer = (char *) allocGlobal(bytes);
    }

    /* Read the data.  */
    bytes_read = __k42_linux_read(fd, buffer, bytes);

    if (bytes_read > 0) {
	/* Copy the data from BUFFER into the memory specified by VECTOR.  */
	to_copy = bytes_read;
	bp = buffer;
	for (i = 0; i < count; ++i) {
	    size_t copy = MIN(vector[i].iov_len, to_copy);

	    (void) memcpy((void *) vector[i].iov_base, bp, copy);

	    bp += copy;
	    to_copy -= copy;
	    if (to_copy == 0)
		break;
	}
    }

    if (bytes >= PAGE_SIZE) {
	freeGlobal(buffer, bytes);
    }

    return bytes_read;
}

struct compat_iovec {
    uval32 iov_base;
    uval32 iov_len;
};

/* The 64-bit implementation above chooses to allocate a single large buffer,
 * which could easily be exhausted by the very cases which readv is used for.
 * Here, we choose to make a read system call for each array element.  This
 * is of course slower, but at least it will not run out of memory.
 */
extern "C" sval32
__k42_linux_sys32_readv(sval32 fd, const struct compat_iovec *iov, sval32 cnt)
{
    int i;
    void *ptr;
    size_t len;
    ssize_t ret;
    sval32 bytes = 0;

    for (i = 0; i < cnt; i++) {
	ptr = (void *)ZERO_EXT(iov[i].iov_base);
	len = iov[i].iov_len;

	ret = __k42_linux_read(fd, ptr, len);

	if (ret < 0 || (size_t)ret != len) {
	    return ret;
	}

	bytes += ret;
    }

    return bytes;
}

#else // CHEESE

extern "C" ssize_t
__k42_linux_readv(int fd, const struct iovec *vector, int count)
{
    SYSCALL_ENTER();

    if (count<0) {
	// POSIX says if count<=0 it may also return EINVAL
	// but linux only does this for count<0 so we'll do the same
	SYSCALL_EXIT();
	return -EINVAL;
    } else if (count==0) {
	SYSCALL_EXIT();
	return 0;
    }


    // Get reference to file descriptor
    SysStatus rc;
    FileLinuxRef fileRef = _FD::GetFD(fd);

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    int ret = 0;
    uval doBlock = ~(DREF(fileRef)->getFlags()) & O_NONBLOCK;
    ThreadWait *tw = NULL;
    ThreadWait **ptw = NULL;
    if (doBlock) {
	ptw = &tw;
    }
    while (1) {
	GenState moreAvail;
	rc=DREF(fileRef)->readv((struct iovec *)vector, (uval)count, ptw, 
				moreAvail);

	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	    goto abort;
	}

	if (_SUCCESS(rc)) {
	    if ((_SGETUVAL(rc)>0)
	       || (moreAvail.state & FileLinux::ENDOFFILE)) {
		ret = _SRETUVAL(rc);
		break;
	    }
	}
	if (!tw) {
	    ret = -EWOULDBLOCK;
	    goto abort;
	}
	while (tw && !tw->unBlocked()) {
	    SYSCALL_BLOCK();
	    if (SYSCALL_SIGNALS_PENDING()) {
		ret = -EINTR;
		goto abort;
	    }
	}
	tw->destroy();
	delete tw;
	tw = NULL;

    }
 abort:
    if (tw) {
	tw->destroy();
	delete tw;
	tw = NULL;
    }


    SYSCALL_EXIT();
    return ret;
}

struct compat_iovec {
    uval32 iov_base;
    uval32 iov_len;
};


extern "C" sval32
__k42_linux_sys32_readv(sval32 fd, const struct compat_iovec *iov, 
			uval32 count)
{
    struct iovec *localVec;

    localVec = (iovec*)alloca(sizeof(struct iovec)*count);
    
    for (uval i = 0; i < count; ++i) {
	localVec[i].iov_base = (void *)(uval)iov[i].iov_base;
	localVec[i].iov_len = iov[i].iov_len;
    }
    return __k42_linux_readv(fd, localVec, count);
}
#endif



