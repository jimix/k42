/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: connect.C,v 1.30 2005/07/15 17:14:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define connect __k42_linux_connect
#include <sys/socket.h>
#undef connect

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include <io/Socket.H>

int
__k42_linux_connect(int fd, const sockaddr *saddr, socklen_t addrlen)
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
	rc = DREF(fileRef)->connect((const char*)saddr, addrlen,
				    moreAvail, ptw);

	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	    goto abort;
	}

	// Check for error conditions on the socket by trying again
	if (_SUCCESS(rc)) {
	    if (moreAvail.state & FileLinux::EXCPT_SET) {
		continue;
	    } else if (moreAvail.state & FileLinux::WRITE_AVAIL) {
		ret = _SRETUVAL(rc);
		break;
	    }
	}

	if (!tw) {
	    ret = _SGENCD(rc);
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
