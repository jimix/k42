/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Scheduler.C,v 1.4 2004/09/14 16:58:40 mostrows Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include "LinuxEnv.H"
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
extern "C" {
#include <linux/thread_info.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <asm/hardirq.h>
#include <asm/hw_irq.h>
}



extern "C" void yield(void);

void
yield(void)
{
    Scheduler::Yield();
}


extern "C" long kernel_thread(int (*fn)(void *), void *arg,
			      unsigned long flags);


struct ThreadArgs {
    void* arg;
    typedef int (*fn_t)(void *);
    fn_t fn;
    volatile ThreadID thread;
    ThreadArgs(int (*_fn)(void*), void *_arg):
	arg(_arg), fn(_fn), thread(Scheduler::GetCurThread()) {};
    DEFINE_NOOP_NEW(ThreadArgs);
    void wait() {
	while (thread != Scheduler::NullThreadID) {
	    Scheduler::Yield();
	}
    }
    void complete() {
	SyncBeforeRelease();
	thread = Scheduler::NullThreadID;
	SyncAfterAcquire();
    }

};

void ThreadStarter(uval arg) {
    struct ThreadArgs* ta = (struct ThreadArgs*)arg;
    ThreadArgs::fn_t fn = ta->fn;
    void *x = ta->arg;
    ta->complete();
    LinuxEnv sc(LongThread);
    (*fn)(x);

}

long
kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
    ThreadArgs ta(fn, arg);

    Scheduler::ScheduleFunction(ThreadStarter, (uval)&ta);

    ta.wait();
    return 1;
}

long
io_schedule_timeout(long timeout)
{
    return schedule_timeout(timeout);
}

extern "C" void msleep(unsigned long msecs);

void
msleep(unsigned long msecs)
{
    uval end = Scheduler::SysTimeNow() +
	SchedulerTimer::TicksPerSecond() * msecs / 1000;
    uval now;
    while ((now = Scheduler::SysTimeNow())<end) {
	Scheduler::BlockWithTimeout(end, TimerEvent::absolute);
    }
}

signed long
schedule_timeout(signed long timeout)
{
    const uval TicksPerJiffie = SchedulerTimer::TicksPerSecond() / HZ;
    const uval MaxTimeOut = MAX_SCHEDULE_TIMEOUT / TicksPerJiffie;
    uval scaledTimeout = TicksPerJiffie * timeout;
    uval startJiffies = __get_jiffies();
    uval diffJiffies;
    LinuxEnv *sc = getLinuxEnv();
    uval old = 0;

    if (timeout>MaxTimeOut) {
	timeout = MaxTimeOut;
	scaledTimeout = MaxTimeOut;
    }

    if (sc->mode == LongThread) {
	Scheduler::DeactivateSelf();
    }

    if (sc->mode & SysCall) {
	old = preempt_count() & PREEMPT_MASK;
	preempt_count() -= old;
	if (old) {
	    __k42enable_preempt();
	}
    }


    if (softirq_count()==0 && softirq_pending(smp_processor_id())) {
	do_softirq();
    }

    if ((sval)timeout > (sval)scaledTimeout) {
	breakpoint();
	Scheduler::Block(); //overflow
    } else {
	Scheduler::BlockWithTimeout(scaledTimeout, TimerEvent::relative);
    }

    if (sc->mode & SysCall) {
	tassertMsg((preempt_count() & PREEMPT_MASK) == 0,
		   "Scheduling syscalls while preempt sc disabled\n");
	if (old) {
	    __k42disable_preempt();
	    preempt_count() += old;
	}
    }

    if (sc->mode == LongThread) {
	Scheduler::ActivateSelf();
    }

    diffJiffies = __get_jiffies() - startJiffies;
    timeout -= diffJiffies;

    return timeout < 0 ? 0 : timeout;
}

extern "C" void
schedule(void)
{
    LinuxEnv *sc = getLinuxEnv();
    uval old;
    if (sc->mode & LongThread) {
	Scheduler::DeactivateSelf();
    }

    if (sc->mode & SysCall) {
	old = preempt_count() & PREEMPT_MASK;
	preempt_count() -= old;
	if (old) {
	    __k42enable_preempt();
	}
    }

    if (softirq_count()==0 && softirq_pending(smp_processor_id())) {
	do_softirq();
    }
    Scheduler::Block();

    if (sc->mode & SysCall) {
	tassertMsg((preempt_count() & PREEMPT_MASK) == 0,
		   "Scheduling syscalls while preempt sc disabled\n");
	if (old) {
	    __k42disable_preempt();
	    preempt_count() += old;
	}
    }

    if (sc->mode & LongThread) {
	Scheduler::ActivateSelf();
    }
}

extern "C" int
kthread_should_stop(void)
{
    LinuxEnv *sc = getLinuxEnv();
    return (int)sc->msgVal;
}
