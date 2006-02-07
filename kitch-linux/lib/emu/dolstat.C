/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dolstat.C,v 1.1 2004/03/18 17:36:42 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: lxstat() get file status without translation
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#include <sys/stat.h>
#include <errno.h>

int
dolstat(const char *filename, void *buf)
{
    SYSCALL_ENTER();

    SysStatus rc;
    rc = FileLinux::GetStatus(filename,
		FileLinux::Stat::FromStruc((struct stat *) buf), 0);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return 0;
}
