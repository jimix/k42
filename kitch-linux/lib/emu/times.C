/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: times.C,v 1.6 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Return the current process times.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

#include <time.h>
#define times __k42_linux_times
#include <sys/times.h>

/* XXX FIXME: this should not be necessary */
#ifndef CLK_TCK
#define CLK_TCK 100
#endif

clock_t
times(struct tms *buf)
{
#undef times

    clock_t ret;
    uval now;
    uval tps;

    SYSCALL_ENTER();

    now = Scheduler::SysTimeNow();
    tps = Scheduler::TicksPerSecond();
    ret = now * CLK_TCK / tps;

    // FIXME: need to update buf
    memset (buf, 0, sizeof (*buf));

    SYSCALL_EXIT();

    return ret;
}

typedef sval32 clock_t_32;

struct tms_32 {
    clock_t_32 tms_utime;
    clock_t_32 tms_stime;
    clock_t_32 tms_cutime;
    clock_t_32 tms_cstime;
};

extern "C" clock_t_32
__k42_linux_times_32(struct tms_32 *buf)
{
    struct tms buf64;
    clock_t ret64;

    ret64 = __k42_linux_times(&buf64);

    if (ret64 != -1) {
	buf->tms_utime = (clock_t_32)buf64.tms_utime;
	buf->tms_stime = (clock_t_32)buf64.tms_stime;
	buf->tms_cutime = (clock_t_32)buf64.tms_cutime;
	buf->tms_cstime = (clock_t_32)buf64.tms_cstime;
    }
  
    return (clock_t_32)ret64;
}
