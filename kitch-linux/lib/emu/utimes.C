/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: utimes.C,v 1.1 2004/11/10 18:03:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: change access and/or modification times of an inode
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"
#include <sys/types.h>
#include <utime.h>

extern "C" int __k42_linux_utimes(const char *filename,
				  const struct timeval times[2]);

int __k42_linux_utimes(const char *filename, const struct timeval times[2])
{
    struct utimbuf buf;
    SysStatus rc;

    SYSCALL_ENTER();

    err_printf("<-- %s: %s\n", __FUNCTION__, filename);

    /* We give up anything but second resolution.  */
    if (times) {
	buf.actime = times[0].tv_sec;
	buf.modtime = times[1].tv_sec;

	rc = FileLinux::Utime(filename, &buf);
    } else {
	rc = FileLinux::Utime(filename, NULL);
    }

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
