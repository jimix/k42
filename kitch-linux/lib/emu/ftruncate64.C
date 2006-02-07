/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ftruncate64.C,v 1.5 2004/06/21 00:15:34 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: truncate  a  file  to a specified length
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
extern "C" int
__k42_linux_ftruncate64(int fd, off64_t length);

int
__k42_linux_ftruncate64(int fd, off64_t length)
{
    SYSCALL_ENTER();

    SysStatus rc;

    FileLinuxRef fileRef = _FD::GetFD(fd);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    /* we have verified during initializetion that these two types
     * have the same mapping*/

    rc = ((FileLinux*)(DREF(fileRef)))->ftruncate(length);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}

/* See comment above pwrite_ppc32 for why we are bitwise ORing parameters.  */
extern "C" sval32
__k42_linux_ftruncate64_ppc32 (sval32 fd, uval32 ignored, uval32 hi, uval32 lo)
{
    sval32 ret;

    ret = __k42_linux_ftruncate64(fd, ZERO_EXT(hi) << 32 | ZERO_EXT(lo));

    return ret;
}
