/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shutdown.C,v 1.1 2005/05/04 01:29:27 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define shutdown __k42_linux_shutdown
#include <sys/socket.h>
#undef shutdown

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#if 0
extern "C" {
extern int __k42_linux_shutdown (int s, int how);
}
#endif

int
__k42_linux_shutdown(int s, int how)
{
    FileLinuxRef fileRef;
    SysStatusUval rc;

    SYSCALL_ENTER();

    fileRef = _FD::GetFD(s);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }
    rc = DREF(fileRef)->shutdown(how);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return _SGETUVAL(rc);
    }
}
