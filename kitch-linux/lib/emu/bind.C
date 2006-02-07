/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bind.C,v 1.16 2004/06/14 20:32:52 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define bind __k42_linux_bind
#include <sys/socket.h>
#undef bind

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

int
__k42_linux_bind(int fd, const sockaddr *saddr, socklen_t addrlen)
{
    SYSCALL_ENTER();

    SysStatus rc;

    FileLinuxRef fileRef = _FD::GetFD(fd);

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }
    rc = DREF(fileRef)->bind((char*)saddr, addrlen);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}
