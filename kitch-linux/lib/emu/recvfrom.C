/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: recvfrom.C,v 1.35 2005/07/15 17:14:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define recvfrom __k42_linux_recvfrom
#include <sys/socket.h>
#undef recvfrom

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
__k42_linux_recvfrom(int s, void *buf, size_t len, int flags,
                     struct sockaddr *from, socklen_t *fromlen)
{
    SYSCALL_ENTER();
    SysStatus rc =0;

    FileLinuxRef fileRef = _FD::GetFD(s);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }


    uval doBlock = (~(DREF(fileRef)->getFlags()) & O_NONBLOCK)
			| ((~flags) & MSG_DONTWAIT);

    int ret = 0;
    ThreadWait *tw = NULL;
    ThreadWait **ptw = NULL;
    if (doBlock) {
	ptw = &tw;
    }
    while (1) {
	GenState moreAvail;
	uval addrLen = *fromlen;
	if (addrLen > 0x800) {
	    addrLen = 0x800;
	}
	rc=DREF(fileRef)->recvfrom((char *)buf, len, flags, (char*)from,
				   addrLen, ptw, moreAvail);

	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	    goto abort;
	}

	*fromlen = addrLen;
	if ((_SGETUVAL(rc)>0)
	    || (moreAvail.state & FileLinux::ENDOFFILE)) {
	    ret = _SRETUVAL(rc);
	    break;
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

