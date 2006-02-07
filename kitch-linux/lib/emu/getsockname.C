/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getsockname.C,v 1.7 2004/06/14 20:32:54 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define getsockname __k42_linux_getsockname
#include <sys/socket.h>
#undef getsockname

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

int
__k42_linux_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    FileLinuxRef fileRef;
    SysStatusUval rc;

    SYSCALL_ENTER();

    fileRef = _FD::GetFD(s);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    uval addrLen = *namelen;
    rc = DREF(fileRef)->getsocketname((char*)name, addrLen);
    *namelen = (socklen_t)addrLen;

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return _SGETUVAL(rc);
    }
}
