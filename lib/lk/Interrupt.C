/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Interrupt.C,v 1.14 2004/07/08 17:15:34 gktse Exp $
 *****************************************************************************/

#include <lk/lkIncs.H>
#include "modes.H"
#include "Interrupt.H"
#include <trace/traceLinux.h>

extern "C" {
#include <asm/hardirq.h>
#include <asm/system.h>
extern void do_softirq();


unsigned long
probe_irq_on (void)
{
	return 0;
}

int
probe_irq_off (unsigned long irqs)
{
	return 0;
}

}

char lockBuf[sizeof(LinuxGlobalLocks)] = {0,};
LinuxGlobalLocks * __lgl = (LinuxGlobalLocks*)lockBuf;

void
hardirq_enter(int cpu)
{
    __lgl->globalCLILock.acquireR();
}

void
hardirq_exit(int cpu)
{
    __lgl->globalCLILock.releaseR();
}



Thread* globalCLIHolder=NULL;

int
hardirq_trylock(int cpu)
{
    return globalCLIHolder==NULL;
}

void
__k42_global_cli()
{
    if (globalCLIHolder==Scheduler::GetCurThreadPtr()) return;


    if (getThreadMode()==Interrupt) {
	__lgl->globalCLILock.upgrade();
    } else {
	__lgl->globalCLILock.acquireW();
    }
    globalCLIHolder = Scheduler::GetCurThreadPtr();

}

/*
 * SMP flags value to restore to:
 * 0 - global cli -- global interrupts disabled
 * 1 - global sti -- global interrupts enabled
 * 2 - local cli  -- local interrupts disabled
 * 3 - local sti  -- local interrupts enabled
 */


unsigned long
__k42_global_save_flags()
{
    barrier();

    //FIXME this logic is horribly broken if we've got FACIST_LOCKING
    if (globalCLIHolder == Scheduler::GetCurThreadPtr()) {
	return 0;
    }
    return 1;
}

void
__k42_global_restore_flags(unsigned long flags)
{
    switch (flags) {
	case 0:
	    __k42_global_cli();
	    break;
	case 1:
	    __k42_global_sti();
	    break;

	case 2:
	case 3:
#ifdef FACIST_LOCKING2
	    __k42_restore_flags(flags);
#endif /* #ifdef FACIST_LOCKING2 */
	    break;
	default:
	{
	    printk("global_restore_flags: %08lx (%08lx)\n",
		   flags, (&flags)[-1]);
	}
    }
#if 0
    if (globalCLIHolder==Scheduler::GetCurThreadPtr()) {
	printk("global restore error: %i %p\n",flags,globalCLIHolder);
	breakpoint();
    }
#endif /* #if 0 */
}


void
release_irqlock(int cpu)
{
    if (getThreadMode() & SysCall) {
	__lgl->globalCLILock.releaseW();
    } else {
	__lgl->globalCLILock.downgrade();
    }
}

void
synchronize_irq(void)
{
    __k42_global_cli();
    __k42_global_sti();
}

void
__k42_global_sti()
{
    if (globalCLIHolder!=Scheduler::GetCurThreadPtr()) return;

    globalCLIHolder = NULL;
    if (getThreadMode()==Interrupt) {
	__lgl->globalCLILock.downgrade();
    } else {
	__lgl->globalCLILock.releaseW();
    }
}

void linuxLocksInit()
{
    uval i;
    for (i=0; i<NR_CPUS; ++i) {
	__lgl->cpuLock[i].init();
    }

    __lgl->globalCLILock.init();
}
