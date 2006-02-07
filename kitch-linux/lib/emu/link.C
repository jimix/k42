/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: link.C,v 1.12 2004/06/14 20:32:54 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: make a new name (hard link) for a file
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define link __k42_linux_link
#include <unistd.h>

int
link(const char *oldpath, const char *newpath)
{
#undef link
    SYSCALL_ENTER();
    SysStatus rc = FileLinux::Link(oldpath, newpath);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
