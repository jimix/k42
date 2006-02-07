/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWInterrupt.C,v 1.16 2003/06/04 14:17:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent interrupt functionality
 * **************************************************************************/

#include <kernIncs.H>
#include <scheduler/Scheduler.H>
#include "init/arch/amd64/APIC.H"

#include "exception/HWInterrupt.H"
#include "exception/KernelTimer.H"
#include <exception/KernelTimer.H>
#include "exception/ExceptionLocal.H"
#include "exception/ProcessAnnex.H"


#include "exception/HWInterrupt.H"
#include "exception/KernelTimer.H"


#if 0
/*virtual*/ void
HWInterrupt::sendIPI(VPNum vp)
{
    tassertMsg(Scheduler::GetVP() != vp, "Sending IPI to self!\n");

    APIC::sendIPI(exceptionLocal.sysFacts.procId(vp), IP_INTERRUPT);
}
#endif /* #if 0 */

/* static */ uval
HWInterrupt::PhysCPU(VPNum vp)
{
  passertMsg(0, "NYI");
  return(0);
}

extern "C" void
HWInterrupt_IOInterrupt(uval interruptNum)
{
    passertMsg(0, "NYI");

#ifndef CONFIG_SIMICS

    uval irq = interruptNum - HWInterrupt::IO_IRQ0_INTERRUPT;

    tassert((irq <= HWInterrupt::MAX_IRQ) && (irq > 0),
		err_printf("bad irq number %ld (0x%lx).\n", irq, irq));

    /* Raise NetBSD IPL to hardware interrupt level */
//    uval s = splhigh();					XXX use Task Priority Register here (amd64)

    HWInterrupt::HWInterruptHandler *ih;
    DREFGOBJK(TheHWInterruptRef)->getHandler(irq, ih);

    while (ih) {
        (*ih->ih_fun)(ih->ih_arg);
        (ih->ih_count[0])++;
        ih = ih->ih_next;
    }

    interrupt_eoi(irq);

    /* Restore NetBSD IPL but postpone checking for soft interrupts
       so that they are run on soft interrupts thread */
//    cpl = s;							XXX use Task Priority Register here (amd64)
#else /* #ifndef CONFIG_SIMICS */
   passertMsg(0, "NYI");
#endif /* #ifndef CONFIG_SIMICS */
}

extern "C" void
HWInterrupt_TimerInterrupt()
{
    exceptionLocal.kernelTimer.timerInterrupt();
    interrupt_eoi_nonspecific();
}

extern "C" void
HWInterrupt_IPInterrupt()
{
    passertMsg(0, "NYI");

#ifndef CONFIG_SIMICS
    DREFGOBJK(TheMPMsgHndlrExceptionRef)->processMsgs(MPMsgHndlr::MSG_QUEUE);
#else /* #ifndef CONFIG_SIMICS */
	APIC::ackIPI();
#endif /* #ifndef CONFIG_SIMICS */
}

#if 0
void
HWInterrupt::UnmaskInterrupt(uval vector)
{
    if (KernelInfo::OnSim()) {
	/* For now, the only thing that could have been masked was
	   Ethernet Level 7, so return to default unmask all. */
      tassertWrn(0, "HWInterrupt::UnmaskInterrupt");
      return;
    } else {
      tassertWrn(0, "HWInterrupt::UnmaskInterrupt");
      return;
    }
    return;
}

void
HWInterrupt::enableInterrupt(uval source, uval type)
{
    passertMsg(0, "should not register interrupts ???");
    return;
}
#endif /* #if 0 */


