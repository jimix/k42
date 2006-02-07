/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelTimer.C,v 1.42 2004/10/19 18:31:13 butrico Exp $
 *****************************************************************************/
#include <kernIncs.H>
#include "KernelTimer.H"
#include "ExceptionLocal.H"
#include "ProcessAnnex.H"
#include <sys/Dispatcher.H>
#include <exception/KernelInfoMgr.H>
#include <sys/time.h>
#include <bilge/ThinIP.H>
#include "trace/traceException.h"
#include <bilge/SysEnviron.H>

void
KernelTimer::init()
{
    anchor = 0;
    when = _SYSTIME_MAX;
    dispatchTime = _SYSTIME_MAX;
    kernelClock.init();
    exceptionLocal.kernelInfoPtr->systemGlobal.ticksPerSecond =
	kernelClock.getTicksPerSecond();
}

//FIXME MAA  - this will become machine specific once we replace
//        using thinwire with reading hardware clocks
/*
 * On the first VP we initialize the timer values.
 * Later, these will be used to initialize the KernelInfoMgr master
 * copy.
 * On other vp's, KernelInfoMgr will initialize the systemGlobal values
 * from the current master copy.
 */
void
KernelTimer::initTOD(VPNum vp)
{
    if (vp != 0) return;
    struct timeval tv;
    SysStatus rc;
#ifndef CONFIG_SIMICS

    rc = SysEnviron::GetTimeOfDay(tv);
    if (_FAILURE(rc)) {
	rc = 0;
	tv.tv_sec = 99;
	tv.tv_usec = 17;
    }

#else
    rc = 0;
    tv.tv_sec = 99;
    tv.tv_usec = 17;
#endif
    passert(_SUCCESS(rc), err_printf("can't read time of day\n"));
    setTOD(&(exceptionLocal.kernelInfoPtr->systemGlobal), uval(tv.tv_sec),
	   uval(tv.tv_usec));
    return;
}

void
KernelTimer::setTOD(KernelInfo::SystemGlobal* sgp, uval sec, uval usec)
{
    uval epoch_sec, epoch_usec, now, tps;
    disableHardwareInterrupts();
    now = kernelClock.getClock();
    enableHardwareInterrupts();
    tps = exceptionLocal.kernelInfoPtr->systemGlobal.ticksPerSecond;
    epoch_sec = now/tps;
    epoch_usec = ((now%tps)*1000000)/tps;
    if (epoch_usec > usec) {
	epoch_sec += 1;
	epoch_usec -= 1000000;
    }
    epoch_sec = sec-epoch_sec;
    epoch_usec = usec-epoch_usec;
    sgp->epoch_sec = epoch_sec;
    sgp->epoch_usec = epoch_usec;
}

extern "C"
SysTime KernelTimer_TimerRequest(SysTime whenTime,
				 TimerEvent::Kind kind)
{
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
    ProcessAnnex* source;
    source = exceptionLocal.currentProcessAnnex;
    return exceptionLocal.kernelTimer.timerRequest(whenTime, kind, source);
}

SysTime
KernelTimer::timerRequest(SysTime whenTime, TimerEvent::Kind kind,
			  ProcessAnnex* source)
{
    ProcessAnnex *cur, *prev;
    SysTime now, newWhen;

    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
    now = kernelClock.getClock();

    switch (kind) {
    default:
	return 0;

    case TimerEvent::queryTicksPerSecond:
	return kernelInfoLocal.systemGlobal.ticksPerSecond;

    case TimerEvent::queryNow:
	break;

    case TimerEvent::reset:
	if (source->timerEvent.when == (SysTime) -1) {
	    /* not active */
	    break;
	}
	prev = 0;
	cur = anchor;
	while (cur && (cur != source)) {
	    prev = cur;
	    cur = cur->timerEvent.next;
	}
	// test protects agains reset from process that doesn't have
	// an outstanding request - then do nothing
	if (cur) {
	    cur->timerEvent.when = (SysTime)-1;
	    if (prev) {
		// removing later element - can't affect next int time
		prev->timerEvent.next = cur->timerEvent.next;
	    } else {
		// removing first element - time may change
		anchor = cur->timerEvent.next;
		if (anchor && (anchor->timerEvent.when < dispatchTime)) {
		    newWhen = anchor->timerEvent.when;
		} else {
		    newWhen = dispatchTime;
		}
		if (when < newWhen) {
		    when = newWhen;
		    /* Don't request an interrupt in the past.  In this
		     * case, there is a pending timer pop and the timer
		     * interrupt handler will straighten things out.
		     */
		    if (when > now) {
			kernelClock.setInterval(when-now);
		    }
		}
	    }
	}
	break;

    case TimerEvent::relative:
	whenTime += now;
	if (source->timerEvent.when < whenTime) {
	    /* if relative request is later in time then
	     * currently outstanding request, don't change
	     * This is part of strategy to minimize the number
	     * of calls when the user can't get current time cheaply
	     */
	    break;
	}
	// fall through

    case TimerEvent::absolute:
	if (source->timerEvent.when != (SysTime) -1) {
	    timerRequest(0,TimerEvent::reset,source);
	}
	source->timerEvent.when = whenTime;
	prev = 0;
	cur = anchor;
	while (cur && (cur->timerEvent.when <= whenTime)) {
	    prev = cur;
	    cur = cur->timerEvent.next;
	}

	source->timerEvent.next = cur;
	if (prev) {
	    // inserted down the chain - can't be first int needed
	    prev->timerEvent.next = source;
	} else {
	    anchor = source;
	    if (whenTime < dispatchTime) {
		when = whenTime;
		/* if already expired, just request 0 interval
		 * which will cause an immediate pop
		 * We could try to handle it here, but its a low
		 * probability path and thus won't get debugged
		 */
		if (when < now) when = now;
		kernelClock.setInterval(when-now);
	    }
	}
	break;
    }
    return now;
}

/*static*/ SysTime
KernelTimer::TimerRequestTime(ProcessAnnex *pa)
{
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
    return pa->timerEvent.when;
}

void
KernelTimer::setDispatchTime(SysTime now, SysTime quantum)
{
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
    dispatchTime = now + quantum;
    if (anchor && (anchor->timerEvent.when < dispatchTime)) {
	if (when != anchor->timerEvent.when) {
	    when = anchor->timerEvent.when;
	    if (when < now) when = now;
	    kernelClock.setInterval(when - now);
	}
    } else {
	when = dispatchTime;
	kernelClock.setInterval(quantum);
    }
}

/*
 * this is always run hardware disabled
 */
void
KernelTimer::timerInterrupt()
{
    SysTime now;
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
    TraceOSExceptionTimerInterrupt(
		    (uval) exceptionLocal.currentProcessAnnex->
					    excStatePtr()->codeAddr(),
		    (uval) exceptionLocal.currentProcessAnnex->
					    excStatePtr()->branchReg());

    kernelClock.debug();
    now = kernelClock.getClock();
    ProcessAnnex* cur;
    cur = anchor;
    while (cur && now >= cur->timerEvent.when) {
	cur->deliverInterrupt(SoftIntr::TIMER_EVENT);
	cur->timerEvent.when = (SysTime) -1; // mark inactive
	cur = cur->timerEvent.next;
    }
    /*N.B. we have removed all the expired timer events
     *from the queue by storing cur below
     */
    anchor = cur;

    if (now >= dispatchTime) {
	exceptionLocal.dispatchQueue.dispatchTimeout();
	dispatchTime = (SysTime) -1;
    }

    if (cur && (cur->timerEvent.when < dispatchTime)) {
	when = cur->timerEvent.when;
    } else {
	when = dispatchTime;
    }
    //Here, when > now is known, since we removed smaller when's above
    kernelClock.setInterval(when - now);
}

void
KernelTimer::print()
{
    SysTime now;
    ProcessAnnex* cur;
    tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
    now = exceptionLocal.kernelTimer.kernelClock.getClock();
    cur = exceptionLocal.kernelTimer.anchor;
    err_printf("Timers (now %lld):\n", now);
    while (cur) {
	err_printf("pa %p pid %ld when %lld (delta %lld)\n",
		   cur, SysTypes::PID_FROM_COMMID(cur->commID),
		   cur->timerEvent.when, cur->timerEvent.when - now);
    	cur = cur->timerEvent.next;
    }
}
