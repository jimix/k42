/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: symlink.C,v 1.12 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: make a new name (symbolic link) for a file
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/FileLinux.H>

#define symlink __k42_linux_symlink
#include <unistd.h>
int
symlink(const char *oldpath, const char *newpath)
{
#undef symlink
    SysStatus rc;
    SYSCALL_ENTER();
    rc = FileLinux::Symlink(oldpath, newpath);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return _SGETUVAL(rc);		// return length
}
