/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxPIC.C,v 1.9 2004/10/27 13:55:00 mostrows Exp $
 *****************************************************************************/
#define __KERNEL__
#include "lkKernIncs.H"
#include <scheduler/Scheduler.H>
#include <exception/HWInterrupt.H>
#include <trace/traceException.h>
#include "Utils.H"
#include <exception/ExceptionLocal.H>

#include <sync/RWLock.H>
#include <sync/RecLock.H>
#include <sync/Sem.H>
#include <lk/LinuxEnv.H>
#include <trace/traceLinux.h>
#include <sys/KernelInfo.H>
extern "C" {
#define private __C__private
#define typename __C__typename
#define new __C__new
#define class __C__class
#define virtual __C__virtual
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <asm/system.h>
#include <linux/threads.h>
#include <linux/interrupt.h>
#include <asm/hardirq.h>
#include <asm/smp.h>
#include <linux/irq.h>
#include <asm/machdep.h>
#undef private
#undef typename
#undef new
#undef class
#undef virtual

}

extern "C" {
    extern int open_pic_irq_offset;
    extern unsigned int openpic_vec_ipi;
    extern unsigned int openpic_vec_spurious;
    extern int openpic_get_irq(struct pt_regs *regs);
    extern void do_openpic_setup_cpu(void);
    extern void openpic_set_priority(unsigned int pri);
    extern void openpic_request_IPIs(void);
    extern void xics_request_IPIs(void);
    extern void openpic_cause_IPI(unsigned int ipi, unsigned int cpumask);
}
extern void killExceptionHandlers();

irqreturn_t
ipi_action(int a, void* b, struct pt_regs*c)
{
    err_printf("ipi_action called\n");
    breakpoint();
    return 0;
};

extern "C" {
    extern void openpic_disable_irq(unsigned int irq);
    extern void openpic_enable_irq(unsigned int irq);
    extern void openpic_eoi(void);
}

uval lastInt=0;

extern "C" void HWInterrupt_DeadPIC(uval interruptNum);
void
HWInterrupt_DeadPIC(uval interruptNum)
{
    //Generic intr handling routine, to run when we've shut
    //down the interrupt handler on a fast-reboot

    int intr = ppc_md.get_irq(NULL);

    irq_desc_t *desc = get_irq_desc(intr);
    lastInt = intr;

    if (desc->handler && desc->handler->ack) {
	desc->handler->ack(intr);
    }
    err_printf("Interrupt on dead PIC: %d\n", intr);

    if (intr == -1) return;

    if (desc->handler && desc->handler->end) {
	desc->handler->end(intr);
    }
}

//
// Exception mode interrupt handler for OpenPIC
//
void
HWInterrupt_OpenPIC(uval interruptNum)
{
    int intr = ppc_md.get_irq(NULL);

    irq_desc_t *desc = get_irq_desc(intr);
    lastInt = intr;
    if (!desc || intr == -1) return;

    if (desc->handler && desc->handler->ack) {
	desc->handler->ack(intr);
    }

    if (intr == (int)HWInterrupt::hwInterrupt.ipiVecID) {
	ExceptionLocal::HandleInterprocessorInterrupt();
	if (desc->handler && desc->handler->end) {
	    desc->handler->end(intr);
	}
    } else {
	HWInterrupt::ProcessInterrupt(intr);
    }
}

//
// Exception mode interrupt handler for XICS
//
void
HWInterrupt_XICS(uval interruptNum)
{
    int intr = ppc_md.get_irq(NULL);

    irq_desc_t *desc = get_irq_desc(intr);
    lastInt = intr;

    if (intr == -1) return;

    if (desc->handler && desc->handler->ack) {
	desc->handler->ack(intr);
    }

    if (intr == (int)HWInterrupt::hwInterrupt.ipiVecID) {
	// This is needed to correctly reconfigure the IPI interrupt, XICS only
	desc->action->handler(intr, NULL, 0);
	ExceptionLocal::HandleInterprocessorInterrupt();
	if (desc->handler && desc->handler->end) {
	    desc->handler->end(intr);
	}
    } else {
	HWInterrupt::ProcessInterrupt(intr);
    }
}

struct LinuxPIC: public HWInterrupt{
public:
    virtual ~LinuxPIC() {};
};

struct OpenPICCtrl: public LinuxPIC{
public:
    DEFINE_PINNEDGLOBALPADDED_NEW(OpenPICCtrl);
    virtual void init() {
	volatile uval x = (uval)ppc_md.get_irq;
	barrier();
	x=x+1;
	exceptionLocal.pendingIntrs = 0;
	exceptionLocal.intrHandlerThr = 0;
	entryPoint = HWInterrupt_OpenPIC;
	LinuxEnv sc(SysCall);
	HWInterrupt::init();
	hwInterrupt.ipiVecID = openpic_vec_ipi;

    }
    virtual void __prepareStartCPU(uval num) {
	LinuxEnv sc(SysCall);
	openpic_request_IPIs();
//	irq_desc_t *desc = get_irq_desc(openpic_vec_ipi);
//	desc->handler->enable(openpic_vec_ipi);
    };
    virtual void CPUReady(VPNum vp);
    virtual void sendIPI(VPNum vp);

    virtual void CPUDead(uval killGlobal);
};

/* virtual */ void
OpenPICCtrl::CPUReady(VPNum vp)
{
    extern cpumask_t cpu_online_map;
    extern cpumask_t cpu_possible_map;
    extern cpumask_t cpu_available_map;
    extern cpumask_t cpu_present_at_boot;

    hwInterrupt.cpuMask |= 1L << PhysCPU(vp);
    cpu_set(vp, cpu_online_map);
    cpu_set(vp, cpu_possible_map);
    cpu_set(vp, cpu_available_map);
    cpu_set(vp, cpu_present_at_boot);

    openpic_set_priority(0);
}

extern "C" int get_hard_processor_id(int cpu);
static inline u32 physmask(u32 cpumask)
{
	int i;
	u32 mask = 0;

	for (i = 0; i < NR_CPUS; ++i, cpumask >>= 1)
		mask |= (cpumask & 1) << get_hard_smp_processor_id(i);
	return mask;
}

/* virtual */ void
OpenPICCtrl::sendIPI(VPNum vp)
{
    openpic_cause_IPI(0, 1ULL<<vp);
};

/* virtual */ void
OpenPICCtrl::CPUDead(uval killGlobal)
{
    openpic_set_priority(0xf);

    if (!killGlobal) {
	return;
    }

    entryPoint = HWInterrupt_DeadPIC;
    err_printf("CPUDead: pendingIntrs 0x%llx.\n", exceptionLocal.pendingIntrs);

    enableHardwareInterrupts();
    disableHardwareInterrupts();

    for (uval i=0; i<openpic_vec_ipi; ++i) {
	irq_desc_t *desc = get_irq_desc(i);
	if (desc->handler && desc->handler->end) {
	    desc->handler->end(i);
	}
	if (desc->handler && desc->handler->disable) {
	    desc->handler->disable(i);
	}
    }

    enableHardwareInterrupts();
    disableHardwareInterrupts();

    // Enable and disable interrupts to purge and interrupts that
    // may have been queued since we entered the reboot sequence
    // but before we disabled the individual vectors.
    killExceptionHandlers();
    enableHardwareInterrupts();
    disableHardwareInterrupts();
}




#define XICS_IPI		2
#define XICS_IRQ_OFFSET		0x10
#define XICS_IRQ_SPURIOUS	0

extern "C" {
    void xics_setup_cpu(void);
    void xics_cause_IPI(int cpu);
}

struct XICSCtrl: public LinuxPIC{
public:
    DEFINE_PINNEDGLOBALPADDED_NEW(XICSCtrl);
    virtual void init() {
	exceptionLocal.pendingIntrs = 0;
	exceptionLocal.intrHandlerThr = 0;
	entryPoint = HWInterrupt_XICS;
	LinuxEnv sc(SysCall);
	HWInterrupt::init();
	hwInterrupt.ipiVecID = XICS_IPI + XICS_IRQ_OFFSET;
    }
    virtual void __prepareStartCPU(uval num) {
	LinuxEnv sc(SysCall);

	xics_request_IPIs();
#if 0
	irq_desc_t *desc = get_irq_desc(hwInterrupt.ipiVecID);

	if (desc->handler && desc->handler->startup) {
	    desc->handler->startup(hwInterrupt.ipiVecID);
	}
	if (desc->handler && desc->handler->enable) {
	    desc->handler->enable(hwInterrupt.ipiVecID);
	}
#endif
    };
    virtual void CPUReady(VPNum vp) {
	extern cpumask_t cpu_online_map;
	hwInterrupt.cpuMask |= 1L << PhysCPU(vp);
	cpu_set(vp, cpu_online_map);

	xics_setup_cpu();
    };
    virtual void sendIPI(VPNum vp) {
	xics_cause_IPI(vp);
    };
    virtual void CPUDead(uval killGlobal) {
	irq_desc_t *desc = get_irq_desc(XICS_IPI + XICS_IRQ_OFFSET);

	if (!killGlobal) {
	    desc->handler->end(XICS_IPI + XICS_IRQ_OFFSET);
	    return;
	}

	LinuxEnv sc(Interrupt);
	entryPoint = HWInterrupt_DeadPIC;
	err_printf("CPUDead: pendingIntrs 0x%llx.\n",
		   exceptionLocal.pendingIntrs);

	enableHardwareInterrupts();
	disableHardwareInterrupts();

 	for (uval i=0; i<NR_IRQS; ++i) {
	    desc = get_irq_desc(i);
	    if (i==hwInterrupt.ipiVecID) continue;
	    if (!desc->action) continue;
	    if (desc->handler && desc->handler->end) {
		desc->handler->end(i);
	    }
	    if (desc->handler && desc->handler->disable) {
		desc->handler->disable(i);
	    }
	}

	enableHardwareInterrupts();
	disableHardwareInterrupts();

	// Enable and disable interrupts to purge and interrupts that
	// may have been queued since we entered the reboot sequence
	// but before we disabled the individual vectors.

	killExceptionHandlers();
	enableHardwareInterrupts();
	disableHardwareInterrupts();
    }
};

HWInterrupt* linuxInitIntrCtrl()
{
    if (_BootInfo->naca.interrupt_controller == IC_OPEN_PIC) {
	err_printf("Using OpenPIC interrupt controller interface\n");
	return new OpenPICCtrl;
    }
    err_printf("Using XICS interrupt controller interface\n");
    return new XICSCtrl;
}


extern "C" int get_hard_smp_processor_id(int cpu);
int
get_hard_smp_processor_id(int cpu)
{
    return HWInterrupt::PhysCPU(cpu);
}






