/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: rmdir.C,v 1.11 2004/06/14 20:32:56 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: delete a directory
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define rmdir __k42_linux_rmdir
#include <unistd.h>

int
rmdir(const char *pathname)
{
#undef rmdir
    SYSCALL_ENTER();
    SysStatus rc = FileLinux::Rmdir(pathname);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
