/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: utime.C,v 1.12 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: change access and/or modification times of an inode
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define utime __k42_linux_utime
#include <sys/types.h>
#include <utime.h>

int
utime(const char *filename, const struct utimbuf *buf)
{
    SYSCALL_ENTER();

    SysStatus rc = FileLinux::Utime(filename, buf);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
