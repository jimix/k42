/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fsync.C,v 1.16 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: synchronize a file's complete in-core state with
 *		       that on disk
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define fsync __k42_linux_fsync
#include <unistd.h>

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

int
fsync(int fd)
{
    SysStatusUval rc;
    FileLinuxRef fileRef;
    SYSCALL_ENTER();

    fileRef = _FD::GetFD(fd);

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }
    rc=DREF(fileRef)->flush();
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return _SGETUVAL(rc);
    }
}
