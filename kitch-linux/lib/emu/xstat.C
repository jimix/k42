/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: xstat.C,v 1.15 2004/07/08 17:15:28 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: xstat()  get file status without translation
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/FileLinux.H>

#include <sys/stat.h>
#include <errno.h>

int
__k42_linux_stat (const char *filename, struct stat *buf)
{
    SYSCALL_ENTER();

    SysStatus rc;

    TraceOSUserSyscallInfo((uval64)Scheduler::GetCurThreadPtr(),
		 0,0,filename);

    rc = FileLinux::GetStatus(filename, FileLinux::Stat::FromStruc(buf));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return 0;
}

strong_alias(__k42_linux_stat,__k42_linux_stat64);
