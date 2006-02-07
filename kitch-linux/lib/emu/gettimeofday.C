/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: gettimeofday.C,v 1.21 2004/07/13 20:16:07 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current time
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

#define gettimeofday __k42_linux_gettimeofday
#include <sys/time.h>
#include <unistd.h>

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
#undef gettimeofday
    SYSCALL_ENTER();

#if 0
    if (tz != NULL) {
	SYSCALL_EXIT();
	// sets errorno
	return __k42_linux_emulNoSupport(__PRETTY_FUNCTION__, ENOSYS);
    }
#endif

    if (tv == NULL) {
	// Perhaps we should check for all bad addresses?
	SYSCALL_EXIT();
	return -EFAULT;
    }

    uval epoch_sec, epoch_usec, now, tps;
    now = Scheduler::SysTimeNow();
    tps = Scheduler::TicksPerSecond();
    epoch_sec = now/tps;
    epoch_usec = ((now%tps)*1000000)/tps;
    epoch_sec += kernelInfoLocal.systemGlobal.epoch_sec;
    epoch_usec += kernelInfoLocal.systemGlobal.epoch_usec;
    if (epoch_usec >= 1000000) {
	epoch_sec += 1;
	epoch_usec -= 1000000;
    }
    tv->tv_sec = (long)epoch_sec;
    tv->tv_usec = (long)epoch_usec;

    SYSCALL_EXIT();
    return 0;
}

struct compat_timeval {
	sval32	tv_sec;
	sval32  tv_usec;
};

extern "C"
int
__k42_linux_sys32_gettimeofday(struct compat_timeval *tv, struct timezone *tz);

int
__k42_linux_sys32_gettimeofday(struct compat_timeval *tv, struct timezone *tz)
{
    struct timeval tv64;
    int rc;
    rc = __k42_linux_gettimeofday(&tv64, tz);
    tv->tv_sec = tv64.tv_sec;
    tv->tv_usec = tv64.tv_usec;
    return rc;
}
