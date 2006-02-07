/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: setsockopt.C,v 1.10 2004/06/16 19:46:43 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define setsockopt __k42_linux_setsockopt
#include <sys/socket.h>
#undef setsockopt

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

int
__k42_linux_setsockopt(int s, int level, int optname, const void *optval,
                       socklen_t optlen)
{
    FileLinuxRef fileRef;
    SysStatusUval rc;

    SYSCALL_ENTER();

    fileRef = _FD::GetFD(s);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }
    rc = DREF(fileRef)->setsockopt(level, optname, optval, optlen);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return _SGETUVAL(rc);
    }
}
