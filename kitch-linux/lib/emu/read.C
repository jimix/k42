/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: read.C,v 1.30 2005/07/15 17:14:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: read from a file descriptor
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define read __k42_linux_read
#include <unistd.h>
#undef read	// Must not interfere with the method below

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include <trace/traceFS.h>
#define read __k42_linux_read
ssize_t
read(int fd, void *buf, size_t nbytes)
{
    SysStatusUval rc;
#ifndef JIMIISADORK
    if (nbytes == 0) return 0;
#endif
    SYSCALL_ENTER();

#undef read	// Must not interfere with the method below
    FileLinuxRef fileRef = _FD::GetFD(fd);
    GenState moreAvail;

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
	rc = DREF(fileRef)->read((char *)buf, (uval)nbytes, ptw, moreAvail);

	if (_FAILURE(rc)) {
	    tassertMsg(_SGENCD(rc)!=EWOULDBLOCK || !doBlock,
		       "EWOULDBLOCK for blocking fd\n");
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

