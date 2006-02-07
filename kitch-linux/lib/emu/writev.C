/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: writev.C,v 1.9 2005/07/15 17:14:15 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define sys32_writev __k42_linux_sys32_writev
#include <sys/uio.h>

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#undef CHEESE_EMULATION
#ifdef CHEESE_EMULATION
#include <string.h>
extern "C" ssize_t __k42_linux_write(int, const void *, size_t);


extern "C" ssize_t
__k42_linux_writev(int fd, const struct iovec *vector, int count)
{
    char *buffer;
    register char *bp;
    size_t bytes, to_copy;
    int i;

    /* Find the total number of bytes to be written.  */
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

    /* Copy the data into BUFFER.  */
    to_copy = bytes;
    bp = buffer;
    for (i = 0; i < count; ++i) {
#define	min(a, b)	((a) > (b) ? (b) : (a))
	size_t copy = min (vector[i].iov_len, to_copy);
	bp = (char *)mempcpy(bp, vector[i].iov_base, copy);

	to_copy -= copy;
	if (to_copy == 0)
	    break;
    }

    ssize_t ret;
    ret = __k42_linux_write(fd, buffer, bytes);

    if (bytes >= PAGE_SIZE) {
	freeGlobal(buffer, bytes);
    }

    return ret;
}

//this is not a good idea #include <linux/compat.h>

struct compat_iovec {
	uval32	iov_base;
	uval32	iov_len;
};
extern "C" ssize_t
__k42_linux_sys32_writev(int fd, struct compat_iovec *vector, uval32 count)
{
    char *buffer;
    register char *bp;
    size_t bytes, to_copy;
    unsigned int i;

    /* Find the total number of bytes to be written.  */
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

    /* Copy the data into BUFFER.  */
    to_copy = bytes;
    bp = buffer;
    for (i = 0; i < count; ++i) {
	size_t copy = min (vector[i].iov_len, to_copy);
	bp = (char *)mempcpy(bp, (void *)((uval)vector[i].iov_base), copy);

	to_copy -= copy;
	if (to_copy == 0)
	    break;
    }

    ssize_t ret;
    ret = __k42_linux_write(fd, buffer, bytes);

    if (bytes >= PAGE_SIZE) {
	freeGlobal(buffer, bytes);
    }

    return ret;
}
#else // CHEESE THIS IS THE REAL IMP
extern "C" ssize_t
__k42_linux_writev(int fd, const struct iovec *vector, int count)
{
    SYSCALL_ENTER();

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
	rc=DREF(fileRef)->writev(vector, (uval)count, ptw, moreAvail);

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


//this is not a good idea #include <linux/compat.h>

struct compat_iovec {
	uval32	iov_base;
	uval32	iov_len;
};
extern "C" ssize_t
__k42_linux_sys32_writev(int fd, struct compat_iovec *vector, uval32 count)
{
    struct iovec *localVec;

    localVec = (iovec*)alloca(sizeof(struct iovec)*count);
    
    for (uval i = 0; i < count; ++i) {
	localVec[i].iov_base = (void *)(uval)vector[i].iov_base;
	localVec[i].iov_len = vector[i].iov_len;
    }
    return __k42_linux_writev(fd, localVec, count);
}
#endif /* NOT CHEESE */




