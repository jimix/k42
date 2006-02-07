/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: truncate64.C,v 1.5 2004/06/18 04:29:33 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: truncate a file to a specified length
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define truncate64 __k42_linux_truncate64
#include <unistd.h>
int
truncate64(const char *path, off64_t length)
{
    SYSCALL_ENTER();
    //FIXME:
    // truncate(2) set's size to _at most_ length bytes in size.
    // Don't know what the ramifications of this with the IOMapped stuff is
    SysStatus rc = FileLinux::Truncate(path, length);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}

/* See comment above pwrite_ppc32 for why we are bitwise ORing parameters.  */
extern "C" sval32
__k42_linux_truncate64_ppc32 (const char *path,
                              uval32 ignored, uval32 hi, uval32 lo)
{
    sval32 ret;

    ret = __k42_linux_truncate64(path, ZERO_EXT(hi) << 32 | ZERO_EXT(lo));

    return ret;
}
