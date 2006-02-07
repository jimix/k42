/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fxstat.C,v 1.15 2004/06/14 20:32:53 apw Exp $
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
__k42_linux_fstat (int fd, struct stat *stat_buf)
{
    SYSCALL_ENTER();

    SysStatus rc;
    FileLinuxRef fileRef = _FD::GetFD(fd);

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    rc = ((FileLinux*)(DREF(fileRef)))->getStatus(
	FileLinux::Stat::FromStruc(stat_buf));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}

strong_alias(__k42_linux_fstat,__k42_linux_fstat64);
