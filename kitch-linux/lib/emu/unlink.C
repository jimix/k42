/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: unlink.C,v 1.11 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: delete a name and possibly the file it refers to
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define unlink __k42_linux_unlink
#include <unistd.h>

int
unlink(const char *pathname)
{
#undef unlink
    SYSCALL_ENTER();
    SysStatus rc = FileLinux::Unlink(pathname);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
