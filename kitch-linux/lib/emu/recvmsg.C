/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: recvmsg.C,v 1.17 2005/07/15 17:14:15 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define recvmsg __k42_linux_recvmsg
#include <sys/socket.h>
#undef recvmsg

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/Socket.H>

#if !defined(GCC3) && !defined(TARGET_amd64) /* actually PLATFORM_Linux */
int
#else /* TARGET_amd64 */
ssize_t
#endif /* TARGET_amd64 */
__k42_linux_recvmsg(int s, struct msghdr *msg, int flags)
{
    SYSCALL_ENTER();
    SysStatus rc =0;

    FileLinuxRef fileRef = _FD::GetFD(s);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    int ret = 0;
    uval doBlock = (~(DREF(fileRef)->getFlags()) & O_NONBLOCK)
			| ((~flags) & MSG_DONTWAIT);

    ThreadWait *tw = NULL;
    ThreadWait **ptw = NULL;
    if (doBlock) {
	ptw = &tw;
    }
    while (1) {
	GenState moreAvail;
	rc=DREF(fileRef)->recvmsg(*msg, flags, ptw, moreAvail);

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
	    ret = -_SGENCD(rc);
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

