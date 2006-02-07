/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: settimeofday.C,v 1.6 2004/06/16 19:46:43 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current time
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

#define settimeofday __k42_linux_settimeofday
#include <sys/time.h>
#include <unistd.h>

int
settimeofday(const struct timeval *tv, const struct timezone *tz)
{
#undef settimeofday
    SysStatus rc;

    SYSCALL_ENTER();

    if (tz != NULL) {
	SYSCALL_EXIT();
	return -EINVAL;
    }

    if (tv == NULL) {
	// Perhaps we should check for all bad addresses?
	SYSCALL_EXIT();
	return -EFAULT;
    }

    rc = DREFGOBJ(TheProcessLinuxRef)->
	setTimeOfDay(uval(tv->tv_sec), uval(tv->tv_usec));

    if (_FAILURE(rc)) {
	rc = -_SGENCD(rc);
    } else {
	rc = 0;
    }

    SYSCALL_EXIT();
    return rc;
}

struct timeval_32 {
    sval32 tv_sec;
    sval32 tv_usec;
};

extern "C" sval32
__k42_linux_settimeofday_32(const struct timeval_32 *tv, 
                            const struct timezone *tz)
{
    sval32 rc;
    struct timeval tv64;

    tv64.tv_sec = tv->tv_sec;
    tv64.tv_usec = tv->tv_usec;

    rc = __k42_linux_settimeofday(&tv64, tz);

    return rc;
}
