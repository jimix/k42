/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: close.C,v 1.22 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: close a file descriptor
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define close __k42_linux_close
#include <unistd.h>

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

int
close(int fd)
{
    SysStatus rc;
    // Ensure that fd is within range.
    // This test weeds out negative values (e.g., -1) as well.
    if ((unsigned int)fd >= _FD::FD_MAX) {
	return -EBADF;
    }

    SYSCALL_ENTER();

    rc = _FD::CloseFD(fd);
    if (!_SUCCESS(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return 0;
}
