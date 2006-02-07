/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: rename.C,v 1.12 2004/06/14 20:32:55 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: change the name or location of a file
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/FileLinux.H>

#define rename __k42_linux_rename
#include <stdio.h>

int
rename(const char *oldpath, const char *newpath)
{
    SYSCALL_ENTER();
    SysStatus rc = FileLinux::Rename(oldpath, newpath);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
