/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: accept.C,v 1.24 2004/06/14 20:32:52 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define accept __k42_linux_accept
#include <sys/socket.h>
#undef accept

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

int
__k42_linux_accept(int fd, struct sockaddr *saddr, socklen_t *addrlen)
{
    SYSCALL_ENTER();

    SysStatus rc = 0;
    FileLinuxRef fileRef = _FD::GetFD(fd);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }
    FileLinuxRef newFileRef;
    int ret = 0;

    ThreadWait *tw = NULL;
    ThreadWait **ptw = NULL;
    uval doBlock = ~(DREF(fileRef)->getFlags()) & O_NONBLOCK;
    if (doBlock) {
	ptw =  &tw;
    }
    while (1) {
	rc = DREF(fileRef)->accept(newFileRef, ptw);

	if (_FAILURE(rc) && !tw) {
	    ret = _SGENCD(rc);
	    goto abort;
	}

	if (_SUCCESS(rc)) {
	    ret = _SRETUVAL(rc);
	    break;
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

    if (ret>=0) {
	ret = _FD::AllocFD(newFileRef);
	if (ret>0 && saddr != NULL) {
	    uval addrLen = *addrlen;
	    DREF(newFileRef)->getpeername((char*)saddr,addrLen);
	    *addrlen = addrLen;
	}

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
