/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: time.C,v 1.4 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current time
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

#define time __k42_linux_time
#define gettimeofday __k42_linux_gettimeofday
#include <time.h>

time_t
time(time_t* t)
{
#undef time

    SYSCALL_ENTER();

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
    SYSCALL_EXIT();
    if (t) *t = time_t(epoch_sec);
    return epoch_sec;
}

typedef int __kernel_time_t32;

// FIXME: get prototype from somewhere
extern "C" int
__k42_linux_gettimeofday(struct timeval *tv, struct timezone *tz);


extern "C" long 
__k42_linux_time_32(__kernel_time_t32* tloc)
{
    __kernel_time_t32 secs;

    struct timeval tv;

    __k42_linux_gettimeofday(&tv, NULL);
    secs = tv.tv_sec;
    
    if (tloc) {
        *tloc = secs;
    }
    return secs;
}
