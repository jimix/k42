/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWInterrupt.C,v 1.54 2005/06/06 19:08:24 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent interrupt functionality
 * **************************************************************************/

#include <kernIncs.H>
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
#include "exception/HWInterrupt.H"
#include "exception/ExceptionLocal.H"
#include "exception/KernelTimer.H"
#include "trace/traceException.h"
#include "bilge/arch/powerpc/BootInfo.H"
#include "bilge/HWPerfMon.H"
#include <misc/arch/powerpc/simSupport.H>
#include "mem/PageAllocatorKernPinned.H"
#include <misc/arch/powerpc/bits.h>
/* static */ struct HWInterrupt::hwInterrupt_t HWInterrupt::hwInterrupt;

extern "C" void
HWInterrupt_PerfInterrupt()
{
    HWPerfMon *rep = exceptionLocal.getHWPerfMonRep();
    tassert(rep != NULL,
            err_printf("Performance interrupt occured prior to rep"
                       " establishment\n"));
    rep->HWPerfInterrupt();
}

extern "C" void
HWInterrupt_DecInterrupt()
{
    exceptionLocal.kernelTimer.timerInterrupt();
}

extern "C" void
HWInterrupt_IOInterrupt(uval intr)
{
    (HWInterrupt::entryPoint)(intr);
}
#include <misc/arch/powerpc/simSupport.H>
void
HWInterrupt::Init()
{
    uval vec;
    uval phys = 0;
    hwInterrupt.availPhys = _BootInfo->availCPUs;
    vec = hwInterrupt.availPhys;
    hwInterrupt.totalPhys = 0;
    // count the processors
    while (vec) {
	if (vec & 1) {
	    hwInterrupt.ppToPhys[hwInterrupt.totalPhys] = phys;
	    hwInterrupt.totalPhys += 1;
	}
	vec = vec >> 1;
	++phys;
    }
    hwInterrupt.availPhys &= ~(1L<<_BootInfo->masterCPU);
    hwInterrupt.cpuMask = 1L << _BootInfo->masterCPU;
}

// For passing arg to newly started PMAC cpus
// Assembly code looks at this to the the address of the spin-loop code
// in BootInfo that is to be used
volatile uval cpu_starter;

SysStatus
HWInterrupt::StartCPU(VPNum newvp, uval iar, uval r3)
{
    exceptionLocal.pendingIntrs = 0;

    //FIXME - who's locking?
    // for now we start one at a time
    uval newphys;
#if 1

    for (newphys=0;newphys<64;newphys++) {
	if (hwInterrupt.availPhys&(1L<<newphys)) break;
    }
    //FIXME what if none
    tassert(newphys<64,err_printf("no more cpus\n"));
    if (newphys==64) {
	err_printf("no more cpus\n");
	return _SERROR(1530, 0, 16);
    }
#else
    //examples of forcing an order
    if (hwInterrupt.availPhys&(1L<<(newphys=6))) goto marc;
    if (hwInterrupt.availPhys&(1L<<(newphys=8))) goto marc;
    //FIXME only for testing, but assumes initial phys is zero
    for (newphys=63;newphys>0;newphys--) {
	if (hwInterrupt.availPhys&(1L<<newphys)) break;
    }
    //FIXME what if none
    tassert(newphys>0,err_printf("no more cpus\n"));
    if (newphys==0) {
	err_printf("no more cpus\n");
	return _SERROR(1238, 0, 16);
    }
  marc:
#endif

    hwInterrupt.availPhys &= ~(1L<<newphys);
    hwInterrupt.ppToPhys[newvp] = newphys;
    _BootInfo->startCPU[newphys].startR3 = r3;
    eieio();			// store barrier to force ordering
    _BootInfo->startCPU[newphys].startIAR = iar;
    eieio();
    cpu_starter = 0;
    SyncBeforeRelease();

    return 0;
}

/*static*/ void
HWInterrupt::KickAllCPUs(uval maxNumCPUs)
{
    if (_BootInfo->platform == PLATFORM_POWERMAC) {
	for (uval cpu = 1; cpu < maxNumCPUs; cpu++) {
	    if (_BootInfo->startCPU[cpu].startIAR == 1) {
		// This cpu has been flagged by the boot-program as needing
		// kicking.  Note this will only be done on hard-boots, not
		// fast-reboot.

		cpu_starter = (uval)&_BootInfo->startCPU[cpu];
		cpu_starter = PageAllocatorKernPinned::virtToReal(cpu_starter);
		extern void KickCPU(uval cpu);

		uval loopCode = (uval)&_BootInfo->spinLoopCode;
		loopCode = PageAllocatorKernPinned::virtToReal(loopCode);
		_BootInfo->startCPU[cpu].startR3 = loopCode;
		SyncBeforeRelease();
		eieio();

		KickCPU(cpu);

		eieio();		// store barrier to force ordering
		// Wait for a sign from secondary CPU that it is up
		while (_BootInfo->startCPU[cpu].startIAR != 0) {
		    KillMemory();
		    eieio();
		}

		/*
		 * Pass our timebase to the newly-started processor.
		 * FIXME:  This is just a temporary timebase fixup.  We'll be
		 *         off by at least the cost of a cache miss.  We should
		 *         adopt Linux code for freezing and thawing the
		 *         timebase on these machines when we move to a Linux
		 *         version that has it.
		 */
		uval64 now;
		__asm __volatile("mftb %0" : "=r" (now));
		_BootInfo->startCPU[cpu].startR3 = now;
	    }
	}
    }
}

/*
 * we'd like to inline these simple static functions but
 * we can't.  If we put the definitions in the class, O0 compiles
 * which don't inline fail!
 */

/* static */ uval
HWInterrupt::PhysCPU(VPNum vp)
{
    return hwInterrupt.ppToPhys[vp];
}

/* static */ uval
HWInterrupt::TotalPhys()
{
    return hwInterrupt.totalPhys;
}


/* static */ void
HWInterrupt::ProcessInterrupt(uval intr)
{
    uval idx;

    tassertMsg(!hardwareInterruptsEnabled(),
	       "Interrupts should be disabled\n");

    TraceOSExceptionIOInterrupt(intr,
		    (uval)exceptionLocal.intrHandlerThr);

    // K42 FindFirstZero returns values starting with 1, not 0
    idx = FindFirstZero(exceptionLocal.pendingIntrs) - 1;
    tassertMsg(idx<64, "Too many simultaneous interrupts\n");

    exceptionLocal.pendingIntrs |= 1ULL<<idx;
    exceptionLocal.interrupts[idx] = intr;
    if (!exceptionLocal.intrHandlerThr) {
	ExceptionLocal::SoftIntrKernel(SoftIntr::LINUX_SOFTINTR);
    }
}
