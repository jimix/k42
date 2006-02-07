/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dup2.C,v 1.13 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: duplicate an FD
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int
__k42_linux_dup2 (int oldfd, int newfd)
{
    SYSCALL_ENTER();

    SysStatus rc;
    FileLinuxRef newFileRef;
    FileLinuxRef fileRef = _FD::GetFD(oldfd);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    rc = DREF(fileRef)->dup(newFileRef);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    rc = _FD::ReplaceFD(newFileRef, newfd);
    if (_FAILURE(rc)) {
	DREF(newFileRef)->destroy();
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    if (newFileRef) DREF(newFileRef)->destroy();

    SYSCALL_EXIT();
    return  _SRETUVAL(rc);
}
