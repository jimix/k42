/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dostat.C,v 1.3 2004/07/08 17:15:28 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: stat()  get file status Linux Kernel version
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#include <sys/stat.h>
#include <errno.h>


extern int dostat(const char *filename, void *buf);
int
dostat(const char *filename, void *buf)
{
    SYSCALL_ENTER();

    SysStatus rc;

    TraceOSUserSyscallInfo((uval64)Scheduler::GetCurThreadPtr(),
		 0,0,filename);

    /* type cheat the output buffer to a glibc, 64 bits (large file
       offsets, misc?) stat structure, further extended into k42' Stat
     */
    rc = FileLinux::GetStatus(filename,
			 FileLinux::Stat::FromStruc((struct stat *) buf));
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return 0;
}

