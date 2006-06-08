/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPinit.C,v 1.79 2005/06/06 19:08:27 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *		initialize additional procesors beyond 0
 * **************************************************************************/

#include <kernIncs.H>
#include <sys/KernelInfo.H>
#include <mem/PageAllocatorKernPinned.H>
#include <kernel.H>
#include <bilge/LocalConsole.H>
#include <sys/thinwire.H>
#include <exception/ExceptionLocal.H>
#include <mem/SegmentTable.H>
#include <exception/HWInterrupt.H>
#include <bilge/libksup.H>
#include <mmu.H>
#include <sync/atomic.h>
#include <bilge/arch/powerpc/simos.H>
#include <bilge/arch/powerpc/BootInfo.H>
#include "bilge/HWPerfMon.H"
#include <mem/arch/powerpc/InvertedPageTable.H>
#include <scheduler/Scheduler.H>
#include <trace/traceBase.H>
#include "MPinit.H"

//#define PING

void secondaryStart(uval64);

extern void setHID();

#ifdef PING
extern volatile uval cpu_starter;
static inline void
__pingMsg(uval x) {
    cpu_starter = x;
    SyncBeforeRelease();
    while (cpu_starter != 0) {
	KillMemory();
	eieio();
    }
}
#define pingMsg(x) __pingMsg((uval)x);
#else
#define pingMsg(x)
#endif

/*
 * Memory use:
 * We use a single contiguous piece of memory for all information
 * passed to the secondary processor.
 * The layout is:
 *	initial stack
 *	alloc pool - area to be used by initial memory allocator
 *
 * New processor inherits this storage and must free it as appropriate.
 * If there is processor specific memory pooling, this should come from
 * the new processors pool.
 */

static const uval secondaryBootMemSize	 = 32*256*PAGE_SIZE;
static const uval secondaryBootStackSize = 11 * PAGE_SIZE;

extern code StartAdditionalCPU;

//FIXME barrier is used to sync secondary processors with the processor
//starting them.  But the secondaries check for barrier==0 after the
//stating cpu has finishied MPinit.  As long as we do this only once,
//putting barrier in static is good enough.  But we really need to
//clean up this whole mess so its machine independent etc.  This depends
//on cleaning up the idle process stuff in the dispatcher.

static volatile uval barrier;

void
MPinit(VPNum vp)
{
    uval64 *entryFunc;
    codeAddress iar;
    uval stackAddr;
    uval stackTop;
    SysStatus rc;
    static uval numbCPUToStart = 0;
    uval riar;
    uval secondaryBootMem;
    uval numToStart;
    KernelInitArgs kernelInitArgs;
    uval rKernelInitArgsP;
    uval pageTableSize;

    if (vp!=0) return;

    HWInterrupt::KickAllCPUs(KernelInfo::MaxPhysProcs());

    exceptionLocal.logicalCPU = 0;

    numToStart = KernelInfo::CurPhysProcs() - 1;

    if (numToStart == 0) return;

    err_printf("MaxPhys %ld, numToStart %ld\n",KernelInfo::MaxPhysProcs(),
	       numToStart);

    barrier = numToStart;

    HWInterrupt::PrepareStartCPU(numToStart+1);

    while (numToStart--) {
	//N.B. code designed so decision can be dynamic rather than
	if (KernelInfo::ControlFlagIsSet(KernelInfo::SHARED_PAGE_TABLE)) {
	    pageTableSize = 0;
	    // pass the vmapsr address of our exception local page table object
	    kernelInitArgs.sharedIPT = (InvertedPageTable*)(
		uval(&exceptionLocal.pageTable) - exceptionLocal.kernelPSRDelta
		+ exceptionLocal.vMapsRDelta);
	    exceptionLocal.pageTable.prepareToShare();
	} else {
	    pageTableSize =
		1<<(LOG_PTE_SIZE+exceptionLocal.pageTable.getLogNumPTEs());
	    kernelInitArgs.sharedIPT = 0;
	}
	//FIXME should allocate from new processors memory if NUMA
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPagesAligned(secondaryBootMem,
			      pageTableSize+secondaryBootMemSize,
			      pageTableSize, 0,
			      PageAllocator::PAGEALLOC_NOBLOCK);
	tassert(_SUCCESS(rc),
		err_printf("failed to allocate secondary boot mem.\n"));

	// only used if not sharedipt.
	kernelInitArgs.pageTable = secondaryBootMem;

	// F&Add returns old value, so add 1 to get new value
	kernelInitArgs.vp =
	    1 + FetchAndAddVolatile((&numbCPUToStart), 1);

	err_printf("Going to start: %ld\n",kernelInitArgs.vp);
	uval pageTableOrigin = secondaryBootMem;
	uval truePageTableSize = pageTableSize;
	char *sharedText = "";
	if (kernelInitArgs.sharedIPT) {
	    pageTableOrigin = kernelInitArgs.sharedIPT->getPageTableVaddr();
	    truePageTableSize =
		1<<(LOG_PTE_SIZE+exceptionLocal.pageTable.getLogNumPTEs());
	    sharedText = "shared";
	}
	err_printf("Page table: 0x%lx[0x%lx] %s\n",
		   pageTableOrigin, truePageTableSize, sharedText);

	stackTop = secondaryBootMem + pageTableSize;

        // high end of stack (grows downward)
	stackAddr = stackTop + secondaryBootStackSize;

	kernelInitArgs.memory.init(
	    0, _BootInfo->physEnd, PageAllocatorKernPinned::realToVirt(0),
	    stackAddr,
	    secondaryBootMem + pageTableSize + secondaryBootMemSize);

	// we will eventually free the space occupied by the boot stack
	kernelInitArgs.memory.rememberChunk(stackTop, stackAddr);

	InitKernelMappings(kernelInitArgs);

	entryFunc = (uval64 *)secondaryStart;
	kernelInitArgs.iar = entryFunc[0];
	kernelInitArgs.toc = entryFunc[1];
	kernelInitArgs.msr = (uval64) PSL_KERNELSET;
	kernelInitArgs.sdr1 = kernelInitArgs.elocal->pageTable.getSDR1();

        // leave space at bottom for prev minimum frame
	kernelInitArgs.stackAddr = stackAddr - 24;

	rKernelInitArgsP = PageAllocatorKernPinned::virtToReal(
	    uval(&kernelInitArgs));

	iar = (codeAddress)StartAdditionalCPU;
	riar = PageAllocatorKernPinned::virtToReal(uval(iar));
	kernelInitArgs.barrierP = &barrier;

#ifdef PING
	xferArea->rcpu_starter =
	    PageAllocatorKernPinned::virtToReal(uval(&cpu_starter));
	cpu_starter = 1;
#endif
// lolita.s has commented out code which will stop on a trap and
// record useful values - this can be used to debug early failures
// in the secondary processor.  Uncomment here and in lolita.
//#define LOLITA_TRAP
#ifdef LOLITA_TRAP
	*(uval *)0xc000000000000180 = 0;
	*(uval *)0xc0000000000001a8 = 0;
#endif
	HWInterrupt::StartCPU(numbCPUToStart, riar, rKernelInitArgsP);

#ifdef PING
	while (((uval)-1) != cpu_starter) {
	    cpu_starter = 0;
	    SyncBeforeRelease();
	    while (cpu_starter==0);
	    err_printf("pinged: %lx %lx %lx %lx %lx\n",cpu_starter,
		       *(uval*)0xc000000000000188,
		       *(uval*)0xc000000000000190,
		       *(uval*)0xc000000000000198,
		       *(uval*)0xc0000000000001a0);
	}
	cpu_starter = 0;
	*(uval*)0xc000000000000180 = 0;
#endif
#ifdef LOLITA_TRAP
	while (*(volatile uval*)0xc000000000000180 == 0);
	err_printf("LOLITA_TRAP: %lx %lx %lx %lx %lx %lx\n",
		   *(uval*)0xc000000000000180,
		   *(uval*)0xc000000000000188,
		   *(uval*)0xc000000000000190,
		   *(uval*)0xc000000000000198,
		   *(uval*)0xc0000000000001a0,
		   *(uval*)0xc0000000000001a8);
#undef pingMsg
#define pingMsg(x) *(volatile uval*)0xc0000000000001a8 = x;
#endif
	while (barrier != numToStart) {
	    //Scheduler::Yield();
	    Scheduler::DelayMicrosecs(100000);
	}

	err_printf("finished starting %ld %lx\n",
		   numbCPUToStart,HWInterrupt::hwInterrupt.availPhys);
    }
}

void
secondaryStart(uval64 kernelInitArgsP)
{
    // need private copy since passed copy is in MPinit stack
    KernelInitArgs kernelInitArgs = *(KernelInitArgs*)kernelInitArgsP;
    pingMsg(__LINE__);
    //MemoryMgrPrimitiveKern *memory = &kernelInitArgs.memory;
    pingMsg(__LINE__);
#if 0
    setHID();
#endif

    /*
     * Initialize Performance Monitoring Hardware to a known state
     */
    pingMsg(__LINE__);
    HWPerfMon::VPInit();
    pingMsg(8);

    pingMsg(__LINE__);
    HWInterrupt::SecondaryPreInit(kernelInitArgs.vp);

    pingMsg(__LINE__);
    exceptionLocal.hwInterrupt->CPUReady(kernelInitArgs.vp);

    pingMsg(__LINE__);
    /*
     * Hardware interrupts have been disabled up this point.  We enable now
     * to avoid assertions in subsequent boot code.  We're not yet ready for
     * timer or external interrupts, so null handlers have been established
     * for them.
     */
    enableHardwareInterrupts();

    pingMsg(__LINE__);
#ifdef PING
    cpu_starter = (uval)-1;
#endif

    err_printf("Started CPU: p:%ld v:%ld\n",
	       exceptionLocal.physCPU,
	       exceptionLocal.logicalCPU);

    err_printf("Proceeding with Kernel Initialization:\n");
    KernelInit(kernelInitArgs);
    /* NOTREACHED */
}

