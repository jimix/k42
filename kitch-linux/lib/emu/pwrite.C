/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pwrite.C,v 1.5 2004/06/16 19:46:43 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: pwrite to a file descriptor
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define pwrite __k42_linux_pwrite
#include <unistd.h>
#undef pwrite // Must not interfere with the write method below

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#define pwrite __k42_linux_pwrite
ssize_t
pwrite(int fd, const void *buf, size_t nbytes, off_t offset)
{
#ifndef JIMIISADORK
    if (nbytes == 0) return 0;
#endif

    /* Do not accept negative offsets.  */
    if (offset < 0) {
	return -EINVAL;
    }

    SYSCALL_ENTER();

#undef pwrite // Must not interfere with the write method below
    SysStatus rc;
    FileLinuxRef fileRef = _FD::GetFD(fd);

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    int ret = 0;
    rc = DREF(fileRef)->pwrite((char *)buf, (uval)nbytes, (uval)offset);
    if (_FAILURE(rc)) {
	ret = -_SGENCD(rc);
    } else {
	ret = _SRETUVAL(rc);
    }

    SYSCALL_EXIT();
    return ret;
}

/* Both __libc_pwrite and __libc_pwrite64 are vectored here.  For the
 * latter, logical parameter 4 is a 64-bit value, so both functions
 * use the argument packing specified in the PPC32 ABI, where logical
 * parameter 4 is passed with its high part in parameter 5 and low
 * part in parameter 6.
 *
 * This function should properly go in arch/powerpc/pwrite.C, but that
 * would require some dependency and link modifications.
 * 
 * For parameter 5, __libc_pwrite passes (offset >> 31), where offset
 * is a 32-bit off_t, while __libc_pwrite64 passes (offset >> 32),
 * where offset is a 64-bit off64_t.  In C, right shifting a signed
 * value such as off_t is architecture dependent, and effects either
 * an arithmetic shift (sign bit fill) or a logical shift (0 fill).
 * PPC32 uses arithmetic shift, which has the effect of making
 * parameter 5 from __libc_pwrite a boolean flag, either 0xFFFFFFFF or
 * 0x00000000, indicating whether an application compiled without
 * large file support has overflowed offset (i.e. made it negative).
 * The glibc author relies upon the latter and the fact that kernel
 * implementations in sys_pwrite and here will bitwise OR parameters 5
 * and 6 together and reject if the result, whose sign bit is the most
 * significant bit of parameter 5, is negative.
 */
extern "C" sval32
__k42_linux_pwrite_ppc32 (sval32 fd, const void *buf, uval32 nbytes, 
                          uval32 ignored, uval32 hi, uval32 lo)
{
    sval32 ret;

    ret = __k42_linux_pwrite(fd, buf, ZERO_EXT(nbytes), 
                             ZERO_EXT(hi) << 32 | ZERO_EXT(lo));

    return ret;
}
