/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: clock_gettime.C,v 1.1 2004/08/12 16:19:20 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current time
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

#include <time.h>
#include <unistd.h>

extern "C" int __k42_linux_clock_gettime(clockid_t clock_id, struct timespec *tp);

int __k42_linux_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    if (clock_id != CLOCK_REALTIME) {
	return -EINVAL;
    }

    SYSCALL_ENTER();
	
    uval epoch_sec, epoch_nsec, now, tps;
    now = Scheduler::SysTimeNow();
    tps = Scheduler::TicksPerSecond();
    epoch_sec = now/tps;
    epoch_nsec = ((now%tps)*1000000000)/tps;
    epoch_sec += kernelInfoLocal.systemGlobal.epoch_sec;
    epoch_nsec += kernelInfoLocal.systemGlobal.epoch_usec*1000;
    if (epoch_nsec >= 1000000000) {
	epoch_sec += 1;
	epoch_nsec -= 1000000000;
    }
    tp->tv_sec = (long)epoch_sec;
    tp->tv_nsec = (long)epoch_nsec;

    SYSCALL_EXIT();
    return 0;
}

struct compat_timeval {
    sval32 tv_sec;
    sval32 tv_nsec;
};

extern "C" int __k42_linux_sys32_clock_gettime(clockid_t clock_id, struct compat_timeval *tp);

int __k42_linux_sys32_clock_gettime(clockid_t clock_id, struct compat_timeval *tp)
{
    struct timespec tp64;
    int rc;
    rc = __k42_linux_clock_gettime(clock_id, &tp64);
    tp->tv_sec = tp64.tv_sec;
    tp->tv_nsec = tp64.tv_nsec;
    return rc;
}
