/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fdatasync.C,v 1.1 2005/08/16 14:55:16 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: synchronize a file's data buffer state with
 *		       that on disk. Synchronizing meta-data is not required.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define fdatasync __k42_linux_fdatasync
#include <unistd.h>

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

/*
 * FIXME!
 *
 * The current implementation is a cut'n'paste copy of the fsync(2)
 * implementaion.
 *
 * To fix fdatasync so that it only syncs data buffers, it would be necessary
 * to alter the FileLinux class to export a fdatasync() type interface, as
 * well as the current flush().
 */

int
fdatasync(int fd)
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
