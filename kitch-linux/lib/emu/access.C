/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: access.C,v 1.11 2004/06/14 20:32:52 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: check user's permissions for a file
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define access __k42_linux_access
#include <unistd.h>

int
access(const char *pathname, int mode)
{
    SysStatus rc;
    SYSCALL_ENTER();
    rc = FileLinux::Access(pathname, mode);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return 0;
    }
}
