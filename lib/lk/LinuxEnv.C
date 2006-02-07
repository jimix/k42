/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxEnv.C,v 1.18 2005/07/27 21:09:24 dilma Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include "LinuxEnv.H"
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
#include <misc/linkage.H>
#include <sys/extRegs.H>
extern "C" {
#include <linux/thread_info.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <asm/hardirq.h>
#include <asm/hw_irq.h>
}

spinlock_t kernel_flag __cacheline_aligned_in_smp = { {0ULL,}};

#define gbit(val) (1ULL << uval((val)))

struct VPInfoHolder {
    struct thread_info thr_info;
    ThreadID softIRQThread;		// Thread spawned for softIRQ
    volatile uval activeSoftIRQ;	// Will thread check for work?
    ThreadID hardIRQSurvivor;	// Hard IRQ thread that will attempt softIRQ
#ifdef LINUXENV_DEBUG
    AutoListHead debugList;
    Thread*	hardBar;
    Thread*	softBar;
    Thread*	scBar;
#endif
    static void ClassInit(VPNum vp);
} __cacheline_aligned;

static char __vpInfoBuf[sizeof(VPInfoHolder[NR_CPUS])]
__cacheline_aligned = {0,};
struct VPInfoHolder *vpInfo = (VPInfoHolder*)&__vpInfoBuf;

struct VPInfoHolder &getVPIH() {
    VPNum vp = Scheduler::GetVP();
    return vpInfo[vp];
}

#ifdef LINUXENV_DEBUG
/* static */ void
LinuxEnv::DisabledBarGroup(Thread::Group grp)
{
    struct VPInfoHolder &vp = getVPIH();
    Thread *ptr = Scheduler::GetCurThreadPtr();
    Thread **pptr = NULL;
    LinuxEnv *env = getLinuxEnv();
    Scheduler::DisabledBarGroup(grp);
    switch (grp) {
    case Thread::GROUP_LINUX_SYSCALL:
	pptr = &vp.scBar;
	break;
    case Thread::GROUP_LINUX_SOFTIRQ:
	pptr = &vp.softBar;
	break;
    case Thread::GROUP_LINUX_INTERRUPT:
	pptr = &vp.hardBar;
	break;
    }

    env->bars |= 1<<((uval)grp);

    if (*pptr==NULL)
	*pptr = Scheduler::GetCurThreadPtr();
}

/* static */ void
LinuxEnv::DisabledUnbarGroup(Thread::Group grp)
{
    struct VPInfoHolder &vp = getVPIH();
    Thread *ptr = Scheduler::GetCurThreadPtr();
    Thread **pptr = NULL;
    LinuxEnv *env = getLinuxEnv();
    switch (grp) {
    case Thread::GROUP_LINUX_SYSCALL:
	tassertWrn(env->mode != Interrupt,
		   "Interrupt mode barring syscall %p\n",
		   Scheduler::GetCurThreadPtr());
	pptr = &vp.scBar;
	break;
    case Thread::GROUP_LINUX_SOFTIRQ:
	pptr = &vp.softBar;
	break;
    case Thread::GROUP_LINUX_INTERRUPT:
	pptr = &vp.hardBar;
	break;
    }


    env->bars &= ~(1 << ((uval)grp));

    if (*pptr == ptr) {
	*pptr = NULL;
    }
    Scheduler::DisabledUnbarGroup(grp);
}
#endif



static void
SoftIRQThread(VPNum vp)
{
    VPInfoHolder &vpih = getVPIH();
    while (Swap(&vpih.activeSoftIRQ,0)) {
	LinuxEnv(SoftIRQ);
    }
}

extern "C" void wakeup_softirqd(void);
void
wakeup_softirqd(void)
{
    VPInfoHolder &vpih = getVPIH();
    uval enable = 0;
    if (!Scheduler::IsDisabled()) {
	Scheduler::Disable();
	enable = 1;
    }
    vpih.activeSoftIRQ = 1;
    if (vpih.softIRQThread==Scheduler::NullThreadID) {
	SysStatus rc = Scheduler::DisabledScheduleFunction(SoftIRQThread,
							   Scheduler::GetVP(),
							   vpih.softIRQThread);
	tassertMsg(_SUCCESS(rc),"can't start softirq thread: %lx\n", rc);
    }
    if (enable) {
	Scheduler::Enable();
    }
}


void
LinuxEnvInit(VPNum vp) {
    VPInfoHolder::ClassInit(vp);
}

void
VPInfoHolder::ClassInit(VPNum vp)
{
    vpInfo[vp].thr_info.cpu = vp;
    vpInfo[vp].hardIRQSurvivor = Scheduler::NullThreadID;
    AutoListHead alh;
#ifdef LINUXENV_DEBUG
    memcpy(&vpInfo[vp].debugList, &alh, sizeof(alh));
    vpInfo[vp].debugList.init();
#endif
}




extern "C" void __k42disable_preempt(void);
extern "C" void __k42enable_preempt(void);
void
__k42disable_preempt(void)
{
    if (getLinuxEnv()->mode == Interrupt) return;

    Scheduler::Disable();

    ASSERTENV;

    tassertMsg((preempt_count() & PREEMPT_MASK) == 0,
	       "Cannot re-disable preempt\n");

    Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SYSCALL);
    LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_SYSCALL);

    ASSERTENV;

    Scheduler::Enable();

}

extern "C" void __enable_preempt(void);
void
__k42enable_preempt(void)
{
    if (getLinuxEnv()->mode == Interrupt) return;

    Scheduler::Disable();

    ASSERTENV;

    tassertMsg((preempt_count() & PREEMPT_MASK) == 0,
	       "non-zero preempt_count() %lx\n", preempt_count());

    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_SYSCALL);
    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SYSCALL);

    ASSERTENV;

    Scheduler::Enable();
}

extern "C" void __k42_local_bh_disable(void);
void
__k42_local_bh_disable(void)
{
    if (getLinuxEnv()->mode == Interrupt) return;

    Scheduler::Disable();

    ASSERTENV;

    tassertMsg(softirq_count() == 0, "Cannot re-disable bh\n");

    Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SOFTIRQ);
    LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_SOFTIRQ);

    ASSERTENV;

    Scheduler::Enable();
}

extern "C" void __k42_local_bh_enable(void);
void
__k42_local_bh_enable(void)
{
    if (getLinuxEnv()->mode == Interrupt) return;

    Scheduler::Disable();

    ASSERTENV;

    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_SOFTIRQ);
    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SOFTIRQ);

    ASSERTENV;

    Scheduler::Enable();
}

extern "C" void __local_irq_disable(void);
void
__local_irq_disable(void)
{
    Scheduler::Disable();

    tassertMsg(getLinuxEnv()->mode != Interrupt,
	       "Interrupts mode shouldn't be here\n");

    ASSERTENV;

    Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
    LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_INTERRUPT);

    ASSERTENV;

    Scheduler::Enable();
}

extern "C" void __local_irq_enable(void);
void
__local_irq_enable(void)
{
    Scheduler::Disable();

    tassertMsg(getLinuxEnv()->mode != Interrupt,
	       "Interrupts mode shouldn't be here\n");

    ASSERTENV;

    tassertMsg(Scheduler::GetBarredGroups() &
	       (1<<Thread::GROUP_LINUX_INTERRUPT),
	       "Interrupts already unbarred\n");

    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_INTERRUPT);
    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
    tassertMsg(hardirq_count() == 0, "interrupts should be disabled\n");

    ASSERTENV;

    Scheduler::Enable();
}


LinuxEnv::LinuxEnv(ThreadMode m, struct task_struct *t)
    :msgVal(0), suspendGroups(0)
#ifdef LINUXENV_DEBUG
    ,bars(0),disp(extRegsLocal.dispatcher),attach(this)
#endif // LINUXENV_DEBUG
{
    mode = m;
    Thread* thr = Scheduler::GetCurThreadPtr();
    oldAttachment = thr->attachment;

    VPInfoHolder &vpih = getVPIH();
    thr->attachment = (void*)&vpih.thr_info;

    oldTVal = Scheduler::GetThreadSpecificUvalSelf();
    Scheduler::SetThreadSpecificUvalSelf((uval)this);

    if (t==NULL) t = &init_task;
    task = t;
    task->thread_info = &vpih.thr_info;

    uval groups = (Thread::Group)0;
    Scheduler::Disable();

#ifdef LINUXENV_DEBUG
    vpih.debugList.append(&attach);
#endif // LINUXENV_DEBUG


    switch (m) {
    case LongThread:
    case SysCall:
	groups = gbit(Thread::GROUP_LINUX_SYSCALL) |
		 gbit(Thread::GROUP_LINUX_SOFTIRQ) |
		 gbit(Thread::GROUP_LINUX_INTERRUPT);

	Scheduler::DisabledSetGroupsSelf(groups);
	tassertMsg((preempt_count() & PREEMPT_MASK) ||
		   ~(groups & gbit(Thread::GROUP_LINUX_SYSCALL)),
		   "preempt_count() asserts preempt disabled\n");
	tassertMsg(!(preempt_count() & (SOFTIRQ_MASK|HARDIRQ_MASK)),
		   "preempt_count() asserts hard/soft disabled\n");
#ifdef LINUXENV_DEBUG
	oldGroups = Scheduler::GetBarredGroups();
	oldCount = preempt_count();
#endif // LINUXENV_DEBUG
	break;
    case Interrupt:
	//This will causes us to wait for others to finish
	Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
	Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_INTERRUPT);

#ifdef LINUXENV_DEBUG
	oldGroups = Scheduler::GetBarredGroups();
	oldCount = preempt_count();
#endif // LINUXENV_DEBUG

	LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_INTERRUPT);
	tassertMsg(hardirq_count() == 0ULL,
		   "Bad preempt count at interrupt time\n");

	tassertMsg(!(vpih.thr_info.flags & K42_IN_INTERRUPT),
		   "Interrupt already running\n");

	tassertMsg(!(vpih.thr_info.flags & K42_IRQ_DISABLED),
		   "Interrupts are disabled\n");

	vpih.thr_info.flags |= K42_IN_INTERRUPT;
	break;

    case SoftIRQ:
	tassertMsg(m!=SoftIRQ,
		   "Should not be instantiatin SoftIRQ environment\n");
	break;
    }

    ASSERTENV;

    Scheduler::Enable();
}

extern "C" void assertLinuxEnv(void);
void
assertLinuxEnv(void)
{
#ifdef LINUXENV_DEBUG
    getLinuxEnv()->assertClean();
#endif
}


#ifdef LINUXENV_DEBUG
void
LinuxEnv::assertClean()
{
    uval enable = 0;
    if (!Scheduler::IsDisabled()) {
	Scheduler::Disable();
	enable = 1;
    }

    struct VPInfoHolder &vp = getVPIH();
    uval barred = Scheduler::GetBarredGroups();
    uval groups = Scheduler::GetGroupsSelf();

    passertMsg( extRegsLocal.dispatcher == disp,
		"On wrong dispatcher\n");

    passertMsg( hardirq_count() == 0 || irqs_disabled(),
		"preempt_count/flags mismatch\n"); // Apr 6 p; May 3 p

    passertMsg( (current_thread_info()->flags & K42_IN_INTERRUPT) == 0 ||
		mode == Interrupt,
		"flags imply interrupt mode\n");

    passertMsg(mode!= Interrupt || barred & (1<<Thread::GROUP_LINUX_INTERRUPT),
	       "interrupts not barred\n");

    passertMsg( hardirq_count() == 0 ||
		barred & (1<<Thread::GROUP_LINUX_INTERRUPT),
		"interrupts disabled, group not barred\n");

    if (mode != Interrupt) {
	if (preempt_count() & SOFTIRQ_MASK) {
	    passertMsg(barred & (1<<Thread::GROUP_LINUX_SOFTIRQ),
		       "softirq not barred\n");
	    passertMsg((~groups) & (1<<Thread::GROUP_LINUX_SOFTIRQ),
		       "running in barred group\n");
	}
	if (preempt_count() & PREEMPT_MASK) {
	    passertMsg(barred & (1<<Thread::GROUP_LINUX_SYSCALL),
		       "syscall not barred\n"); // Apr 21 p May 4 f
	    passertMsg((~groups) & (1<<Thread::GROUP_LINUX_SYSCALL),
		       "running in barred group\n");
	    passertMsg(vp.scBar != NULL, "No barring thread\n");
	}
    }

    passertMsg(vp.scBar==NULL || !(barred & Thread::GROUP_LINUX_SYSCALL),
	       "sc group barr mismatch\n");

    if (enable) {
	Scheduler::Enable();
    }
}
#endif // LINUXENV_DEBUG

void
LinuxEnv::destroy()
{
    VPInfoHolder &vpih = getVPIH();
    Scheduler::Disable();
    ASSERTENV;

    tassertMsg(!irqs_disabled(), "irq state not restored\n");

    uval groups = Scheduler::GetBarredGroups();

    if (mode == Interrupt) {

	tassertMsg(current_thread_info()->flags & K42_IN_INTERRUPT,
		   "flags should include K42_IN_INTERRUPT\n");
#ifdef LINUXENV_DEBUG
	tassertMsg((preempt_count()==oldCount),
		   "Interrupt mode change: %lx %lx %lx\n",
		   oldCount, preempt_count(), oldGroups);
#endif // #ifdef LINUXENV_DEBUG
	vpih.thr_info.flags &= ~K42_IN_INTERRUPT;

	LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_INTERRUPT);

	// only one hardIRQ thread may go on to try soft irq
	// avoids build-up of threads trying to get into softirq code

	if (vpih.hardIRQSurvivor != Scheduler::NullThreadID) {
	    goto skip_softirq;
	}
	vpih.hardIRQSurvivor = Scheduler::GetCurThread();

	Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_INTERRUPT);

	Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SOFTIRQ);
	Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SYSCALL);
    }

    mode = SoftIRQ;

#ifdef LINUXENV_DEBUG
    oldGroups = Scheduler::GetBarredGroups();
    oldCount = preempt_count();
#endif // LINUXENV_DEBUG

    tassertMsg(preempt_count()==0,"preempt_count: %lx\n",preempt_count());

    if (vpih.hardIRQSurvivor == Scheduler::GetCurThread()) {
	vpih.hardIRQSurvivor = Scheduler::NullThreadID;
    }

    Scheduler::Enable();

    do_softirq();

    Scheduler::Disable();
#ifdef LINUXENV_DEBUG
    tassertMsg(preempt_count()==oldCount,
	       "Shouldn't be running: %lx %lx %lx %lx\n",
	       preempt_count(), oldCount, oldGroups,
	       Scheduler::GetBarredGroups());
#endif

  skip_softirq:
    Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SYSCALL);
    Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SOFTIRQ);
    Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_INTERRUPT);


#ifdef LINUXENV_DEBUG
    attach.detach();
#endif // LINUXENV_DEBUG

    Scheduler::Enable();
    Scheduler::GetCurThreadPtr()->attachment = oldAttachment;
    Scheduler::SetThreadSpecificUvalSelf(oldTVal);
}

void
LinuxEnvSuspend()
{
    Scheduler::Disable();
    uval x = Scheduler::GetGroupsSelf();
    uval y = 1;
    uval z = 0;
    getLinuxEnv()->suspendGroups = x;
    while (x) {
	if (y & x) {
	    Scheduler::DisabledLeaveGroupSelf((Thread::Group)z);
	}
	x = x & ~y;
	y <<= 1;
	++z;
    }
    Scheduler::Enable();
}

void
LinuxEnvResume()
{
    Scheduler::Disable();
    uval x = getLinuxEnv()->suspendGroups;
    uval y = 1;
    uval z = 0;
    while (x) {
	if (y & x) {
	    Scheduler::DisabledJoinGroupSelf((Thread::Group)z);
	}
	x = x & ~y;
	y <<= 1;
	++z;
    }
    Scheduler::Enable();
}


extern "C" struct task_struct *get_current(void);
struct task_struct *
get_current(void)
{
    extern struct task_struct init_task;
    return getLinuxEnv()->task;
}

extern "C" struct task_struct* current_thr_id(void);
struct task_struct*
current_thr_id(void)
{
    return (struct task_struct*)Scheduler::GetCurThread();
}
