/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: write.C,v 1.30 2005/07/15 17:14:15 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: write to a file descriptor
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define write __k42_linux_write
#include <unistd.h>
#undef write // Must not interfere with the write method below

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#define write __k42_linux_write
ssize_t
write(int fd, const void *buf, size_t nbytes)
{
#ifndef JIMIISADORK
    if (nbytes == 0) return 0;
#endif
    SYSCALL_ENTER();

#undef write // Must not interfere with the write method below
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
	rc=DREF(fileRef)->write((char *)buf, (uval)nbytes, ptw, moreAvail);

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
