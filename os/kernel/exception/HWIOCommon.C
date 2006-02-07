/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWIOCommon.C,v 1.32 2004/02/27 17:14:25 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interrupt dispatching mechanism
 * **************************************************************************/

#include "kernIncs.H"
#include "exception/ExceptionLocal.H"
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>

#include "HWInterrupt.H"

extern "C"
int
fakeintr(void *arg)
{
    return 0;
}

void
LinuxHandleSoftIntr(SoftIntr::IntrType)
{
#if defined(TARGET_powerpc) //K42_LINUX
    SysStatus rc = Scheduler::DisabledScheduleFunction(linuxCheckSoftIntrs, 1);
    tassertSilent(_SUCCESS(rc), BREAKPOINT );
#endif /* #if defined(TARGET_powerpc) //K42_LINUX */
    return;
}


void
HWInterrupt::init()
{
    // initialize lock
    lock.init();

    // initialize interrupt count
    interruptCount = 0;

}

uval nullIntr = 0;
void nullHandler(uval intr) {
    ++nullIntr;
}

// The interrupt entry point function, defined by each controller type
void (*HWInterrupt::entryPoint)(uval intr) = nullHandler;

/*
 * For now we have just one HWInterrupt instance, but by accessing
 * strictly via a pointer in exceptionLocal, we retain the option of
 * distributing the implementation in the future.
 */
HWInterrupt *HWInterrupt::theHWInterrupt=NULL;

/* static */ void
HWInterrupt::SecondaryPreInit(VPNum vp)
{
    exceptionLocal.hwInterrupt = theHWInterrupt;

    exceptionLocal.physCPU = HWInterrupt::PhysCPU(vp);
    exceptionLocal.logicalCPU = vp;
}

/* static */ void
HWInterrupt::ClassInit(VPNum vp)
{
    if (vp == 0) {
	// initialize the interrupt handling mechanism

	exceptionLocal.physCPU = HWInterrupt::PhysCPU(vp);
	exceptionLocal.logicalCPU = vp;

#if defined(TARGET_powerpc) //K42_LINUX
	extern HWInterrupt* linuxInitIntrCtrl();
	theHWInterrupt = linuxInitIntrCtrl();
#else
	theHWInterrupt = new HWInterrupt;
#endif
	theHWInterrupt->init();

	exceptionLocal.hwInterrupt = theHWInterrupt;
    }
    // Inform Scheduler which function to call for Linux soft interrupts
    Scheduler::SetSoftIntrFunction(SoftIntr::LINUX_SOFTINTR,
				   LinuxHandleSoftIntr);

}

/* static */ void
HWInterrupt::HandlerStats()
{
#if 0//K42_LINUX
#if defined(TARGET_powerpc)
    extern uval softnetcnt;

    cprintf("%ld total hardware interrupts, %ld softnet interrupts\n",
	    exceptionLocal.hwInterrupt->interruptCount, softnetcnt);
#endif /* #if defined(TARGET_powerpc) */
    return;
#endif /* #if 0//K42_LINUX */
}

/* static */ void
HWInterrupt::HandlerStats(uval vector)
{
#if 0
    uval n=0;

    HWInterrupt::HWInterruptHandler *ih;
    exceptionLocal.hwInterrupt->getHandler(vector, ih);

    while (ih) {
	cprintf("Vector 0x%02lx Handler %ld called "
		"%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld times\n",
		vector, n,
		ih->ih_count[0],
		ih->ih_count[1],
		ih->ih_count[2],
		ih->ih_count[3],
		ih->ih_count[4],
		ih->ih_count[5],
		ih->ih_count[6],
		ih->ih_count[7]);
	ih = ih->ih_next;
	++n;
    }

    return;
#endif
}

/* static */
void HWInterrupt::SendIPI(VPNum vp)
{
    exceptionLocal.hwInterrupt->sendIPI(vp);
}

/*virtual */ void
HWInterrupt::CPUDead(uval killGlobal) {
    /* empty body */
};
