/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Interrupt.C,v 1.4 2004/07/08 17:15:36 gktse Exp $
 *****************************************************************************/
#define __KERNEL__
#include "lkKernIncs.H"

extern "C" {
#include <asm/bitops.h>
}
#include <scheduler/Scheduler.H>
#include <exception/HWInterrupt.H>
#include <trace/traceException.h>
#include "Utils.H"
#include <exception/ExceptionLocal.H>

#include <lk/LinuxEnv.H>
#include <trace/traceLinux.h>
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

extern SysStatus PrintStatusAllMsgHandler(uval x);

extern "C" void traceExceptionLinux() { /* empty body */ }

extern "C" {

    void linuxCheckSoftIntrs(uval calledFromInterrupt);

} // extern "C"

#define k42_lock(linuxLock) ((FairBLock*)&(linuxLock)->lock[0])

void
linuxCheckSoftIntrs(uval calledFromInterrupt)
{
    uval64 vector=0;
    VPNum currVP = Scheduler::GetVP();
    uval pending = 0;

    LinuxEnv le(Interrupt); //Linux environment object
    while (1) {
	uval soft = softirq_count();
	disableHardwareInterrupts();
	if (!pending) {

	    if (exceptionLocal.intrHandlerThr !=
		(uval)Scheduler::GetCurThreadPtr()&&
		exceptionLocal.intrHandlerThr) {
		enableHardwareInterrupts();
		break;
	    }
	    exceptionLocal.intrHandlerThr = (uval)Scheduler::GetCurThreadPtr();
	    pending = exceptionLocal.pendingIntrs;
	    exceptionLocal.pendingIntrs = 0ULL;
	    if (!pending) {
		exceptionLocal.intrHandlerThr = 0ULL;
		enableHardwareInterrupts();
		break;
	    }
	}
	enableHardwareInterrupts();

	//Note: 64-bit version of ffs needed
	// this is equivalent to that
	uval bit = ffz(~pending);

	vector = exceptionLocal.interrupts[bit];
	exceptionLocal.interrupts[bit] = ~0;

       	TraceOSLinuxInt(currVP, softirq_pending(currVP), pending);

	pending &= ~(1ULL<<bit);

	tassertMsg(soft == softirq_count(), "softirq change\n");

	//Run the real handlers
	irq_desc_t *desc = get_irq_desc(vector);
	k42_lock(&desc->lock)->acquire();
	int status = desc->status & ~(IRQ_REPLAY | IRQ_WAITING);
	if (!(status & IRQ_PER_CPU))
		status |= IRQ_PENDING; /* we _want_ to handle it */

	struct irqaction* action = NULL;

	if (likely(!(status & (IRQ_DISABLED | IRQ_INPROGRESS)))) {
	    action = desc->action;
	    if (!action || !action->handler) {
		desc->status |= IRQ_DISABLED;
	    }
	    status &= ~IRQ_PENDING; /* we commit to handling */
	    if (!(status & IRQ_PER_CPU))
		status |= IRQ_INPROGRESS; /* we are handling it */
	}
	desc->status = status;

	if (action && action->handler) {
	    action->handler(vector, action->dev_id, 0);
	}

	// Sometimes we may be left with interrupts disabled
	local_irq_enable();


	desc->status &= ~IRQ_INPROGRESS;


	if (desc->handler) {
	    if (desc->handler->end) {
		desc->handler->end(vector);
	    } else {
		desc->handler->enable(vector);
	    }
	}
	tassertMsg(soft == softirq_count(), "softirq change\n");

	k42_lock(&desc->lock)->release();

    }

    //Softirq handler called by "le" dtor
}
