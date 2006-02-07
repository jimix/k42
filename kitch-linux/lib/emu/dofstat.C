/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dofstat.C,v 1.1 2004/03/18 17:36:42 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: get file status from file descriptor
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

int
dofstat(int fd,  void *buf)
{
    SYSCALL_ENTER();

    SysStatus rc;
    FileLinuxRef fileRef = _FD::GetFD(fd);

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    rc = ((FileLinux*)(DREF(fileRef)))->getStatus(
	FileLinux::Stat::FromStruc((struct stat *) buf));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}
