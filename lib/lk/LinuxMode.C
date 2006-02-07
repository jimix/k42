/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxMode.C,v 1.17 2004/07/08 17:15:34 gktse Exp $
 *****************************************************************************/


#include <lk/lkIncs.H>
#include "modes.H"

extern "C" {
#include <asm/bitops.h>
}
#include <trace/traceException.h>

#include <sync/SLock.H>
#include <sys/ProcessSet.H>
#include <sys/ProcessWrapper.H>

extern "C" {
#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <asm/hardirq.h>
#include <asm/smp.h>
#include <asm/system.h>
#include <linux/threads.h>
#include <linux/interrupt.h>

#define typename __C__typename
#include <linux/irq.h>
#undef typename

}

#include <sync/Sem.H>

extern "C" int smp_num_cpus;

uval availCPU;
Semaphore *cpuControl;

//void LinuxMode::ClassInit(uval procCount)
//void LinuxMode::freeCPU() __attribute__((weak));
//void LinuxMode::initTimer() __attribute__((weak));
//void LinuxMode::setCPU() __attribute__((weak));
//void LinuxMode::setPID(ProcessID pid) __attribute__((weak));
//struct task_struct* getCurrent() __attribute__((weak))


void LinuxMode::ClassInit(uval procCount)
{
    smp_num_cpus = procCount;
    cpuControl = new Semaphore;
    cpuControl->init(NR_CPUS);
    availCPU = (0x1ULL<<NR_CPUS)-1;
}

LinuxMode::LinuxMode(ThreadMode m, TaskInfo* task):
    old_thread_data(Scheduler::GetThreadSpecificUvalSelf()),
    mode(m),
    flags(0),
    vpid(Scheduler::GetVP()),
    ti(task),
    global_lock_depth(0)
{
    Scheduler::SetThreadSpecificUvalSelf((uval)this);
    cpu = INVALID_CPU;
    setCPU();
};


void
LinuxMode::bhDisable() {
    LinuxMode* sc = getLinuxMode();
    sc->flags |= LinuxMode::bhDisabled;
    if (sc->mode == Undefined) {
	sc->mode = SoftIRQ;
    }
}

void
LinuxMode::bhEnable(uval checkSoftIRQ) {
    flags &= ~LinuxMode::bhDisabled;
}

uval
LinuxMode::localIRQSave()
{
    return 0;
}

void
LinuxMode::localIRQRestore(uval x)
{
}

volatile uval killInt = 0;

uval
LinuxMode::inBH()
{
    return mode==SoftIRQ;
}

void
LinuxMode::setCPU()
{
    uval local;
    cpuControl->P();

    do {
	local = availCPU;
	tassertMsg(local!=0, "All cpus used: %lx\n",local);
	cpu = ffz(~local);
	tassertMsg(cpu<NR_CPUS, "Bad cpu: %lx %ld\n",local,cpu);
    } while (!CompareAndStoreSynced(&availCPU, local, local & ~(1ULL<<cpu)));
}


void
LinuxMode::freeCPU()
{

    TraceOSLinuxEnd(cpu, softirq_pending(cpu), 0);

    uval local;
    do {
	local = availCPU;
    } while (!CompareAndStoreSynced(&availCPU, local, local | (1ULL<<cpu)));

    cpuControl->V();
}

void
LinuxMode::doSoftIRQ(uval disabled)
{
    uval old = mode;
    if (disabled) Scheduler::Enable();
    while (softirq_pending(cpu)) {
	mode = Undefined;  // Thread mode is undefined to allow
			   // do_softirq to set thread mode via a
			   // local_bh_disable

	//bhDisabled should not be set --- it could be set accidentally
	// by syscalls on loopback device.
	flags &= ~LinuxMode::bhDisabled;

	TraceOSLinuxBH(cpu, softirq_pending(cpu), 0);
	// Now we know we're the only thread running on this thing
	do_softirq();
    };
    mode = old;
    if (disabled) Scheduler::Disable();
}



extern "C" struct task_struct* getCurrent();
struct task_struct *k42_current;


struct task_struct*
getCurrent()
{
    LinuxMode *sc = getLinuxMode();
    return &k42_current[sc->cpu];
};


extern "C" {
    int in_irq();
    int in_softirq();
    int smp_processor_id();
    int in_interrupt();
    void local_bh_disable();
    void local_bh_enable();
    void __local_bh_enable();
    void local_irq_disable();
    void local_irq_restore(unsigned long flags);
    unsigned long __local_irq_save();
}


int
in_irq()
{
    tassertMsg(getThreadMode()!=Undefined,"Bad thread mode\n");
    return getThreadMode()==Interrupt;
}

int
in_softirq()
{
    return getLinuxMode()->inBH();
}

int
smp_processor_id()
{

    LinuxMode* sc = getLinuxMode();
    return sc->cpu;
}

int
in_interrupt()
{
    int retvalue;
    return in_softirq() || getThreadMode()==Interrupt;
    LinuxMode *sc = getLinuxMode();
    retvalue = (sc->mode & (SoftIRQ|Interrupt)) ||
	(sc->flags & LinuxMode::bhDisabled);
    return (retvalue);
}


void
local_bh_disable()
{
    getLinuxMode()->bhDisable();
}

void
local_bh_enable()
{
    getLinuxMode()->bhEnable(1);
}

void
__local_bh_enable()
{
    getLinuxMode()->bhEnable(0);
}

void
local_irq_disable()
{
    getLinuxMode()->localIRQSave();
}

void local_irq_enable()
{
    getLinuxMode()->localIRQRestore(0);
}

void
local_irq_restore(unsigned long flags)
{
    getLinuxMode()->localIRQRestore(flags);
}

unsigned long
__local_irq_save()
{
    return getLinuxMode()->localIRQSave();
}

