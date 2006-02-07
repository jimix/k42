/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: nanosleep.C,v 1.17 2004/06/16 19:46:43 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: go to sleep for a specified time period
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#define nanosleep __k42_linux_nanosleep
#include <time.h>

int  nanosleep(const struct timespec *req, struct timespec *rem)
{
    SYSCALL_ENTER();

    /* Range checks: */
    if (req->tv_nsec < 0 ||
       req->tv_nsec > 999999999 ||
       req->tv_sec < 0) {
	SYSCALL_EXIT();
	return -EINVAL;
    }

    /* FIXME: (Once we support realtime scheduling of Linux
     * processes.)  If a process is scheduled under a realtime policy,
     * we should be giving real microsecond precision with busy-wait
     * loops. */

    uval nspart, interval, when, tpsf, tpsr;
    const uval gig = 1000000000ul;
    const uval tps = Scheduler::TicksPerSecond();
    
    /* We must sleep for _at least_ the time specified. */

    /*
     * computation
     * tickpersec = tpsf*gig + tpsr
     * nspart/gig & tickpersec = nspart/gig*tpsf*gig + nspart/gig*tpsr
     *                         = nspart*tpsf + nspart*tpsr/gig
     *
     * nspart < gig
     * 2**29<gig<2**30
     * assume tps < 2**50
     * tpsf < 2**21
     * tpsr < gig < 2**30
     * nspart * tpsf < 2**51
     * nspart * tpsr < 2**60
     * so no overflow
     */
    nspart = req->tv_nsec;
    tpsf = tps/gig;
    tpsr = tps % gig;
    interval = nspart*tpsf + (nspart*tpsr)/gig;
    interval += req->tv_sec * tps;

    SYSCALL_DEACTIVATE();
    when = Scheduler::BlockWithTimeout(interval, TimerEvent::relative);
    SYSCALL_ACTIVATE();

    while(when>0 && !SYSCALL_SIGNALS_PENDING()) {
	SYSCALL_DEACTIVATE();
	when = Scheduler::BlockWithTimeout(when, TimerEvent::absolute);
	SYSCALL_ACTIVATE();
    }
	
    SYSCALL_EXIT();
    if (when > 0) {
	if (rem) {
	    interval = Scheduler::SysTimeNow();
	    if(interval > when) interval = when;
	    interval = interval - when;
	    rem->tv_sec = interval / tps;
	    interval = interval - rem->tv_sec * tps;
	    rem->tv_nsec =
		(interval * gig) / tps;
	}
	return -EINTR;
    }
    return 0;
}

/* We are passed a timespec structure from 32-bit apps which is
 * composed of two 32-bit long integers, representing seconds and
 * nanoseconds.  Our 64-bit nanosleep implementation cannot properly
 * dereference members of a timespec structure, because it thinks that
 * such a structure is composed of two 64-bit longs.  So here we
 * define a compatability structure.
 */
struct timespec32 {
    uval32 tv_sec;
    uval32 tv_nsec;
};

/* The 32-bit syscall vector points here for the nanosleep syscall.
 * Here we make 64-bit copies of the 32-bit structures being passed
 * from a 32-bit app, pass the copies to the real 64-bit nanosleep
 * function, then update and return the 32-bit structures to the app.
 */
extern "C" int
__k42_linux_nanosleep_32 (const struct timespec32 * req,
                          struct timespec32 * rem) {
    int ret;
    struct timespec req64, rem64, *ptr;

    /* First make a 64-bit copy of the 32-bit compatibility structure.  */
    req64.tv_sec = req->tv_sec;
    req64.tv_nsec = req->tv_nsec;

    /* Now decide what the second argument should point to.  */
    ptr = (rem == NULL) ? NULL : &rem64;

    /* Fire off the real syscall with 64-bit arguments.  */
    ret = nanosleep(&req64, ptr);

    /* Update the 32-bit remainder structure if appropriate.  */
    if (ret != 0 && rem != NULL) {
	rem->tv_sec = rem64.tv_sec;
	rem->tv_nsec = rem64.tv_nsec;
    }

    return ret;
}
