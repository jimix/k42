/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pread.C,v 1.5 2004/07/08 17:15:28 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: pread from a file descriptor
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define pread __k42_linux_pread
#include <unistd.h>
#undef pread	// Must not interfere with the method below

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include <trace/traceFS.h>
#define pread __k42_linux_pread

ssize_t
pread(int fd, void *buf, size_t nbytes, off_t offset)
{
    SysStatusUval rc;
#ifndef JIMIISADORK
    if (nbytes == 0) return 0;
#endif

    /* Do not accept negative offsets.  */
    if (offset < 0) {
	return -EINVAL;
    }

    SYSCALL_ENTER();

#undef pread	// Must not interfere with the method below

    FileLinuxRef fileRef = _FD::GetFD(fd);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    int ret = 0;

    rc = DREF(fileRef)->pread((char *)buf, (uval)nbytes, (uval)offset);
    if (_FAILURE(rc)) {
	ret = -_SGENCD(rc);
    } else {
	ret = _SRETUVAL(rc);
    }

    SYSCALL_EXIT();
    return ret;
}

/* See comment above pwrite_ppc32 for why we are bitwise ORing parameters.  */
extern "C" sval32
__k42_linux_pread_ppc32 (sval32 fd, void *buf, uval32 nbytes, 
                         uval32 ignored, uval32 hi, uval32 lo)
{
    sval32 ret;

    ret = __k42_linux_pread(fd, buf, ZERO_EXT(nbytes), 
                         ZERO_EXT(hi) << 32 | ZERO_EXT(lo));

    return ret;
}
