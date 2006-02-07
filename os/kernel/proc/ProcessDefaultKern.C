/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessDefaultKern.C,v 1.72 2004/09/20 18:53:00 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "kernIncs.H"
#include "proc/ProcessDefaultKern.H"
#include "exception/HWInterrupt.H"
#include "mem/PMKern.H"
#include "mem/PageAllocatorKernPinned.H"
#include <misc/hardware.H>
#include <meta/MetaProcessServer.H>
#include <sys/memoryMap.H>
#include <sys/Dispatcher.H>
#include <sys/entryPoints.H>
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>
#include <bilge/libksup.H>
#include <init/kernel.H>
#include <cobj/ObjectRefs.H>

/* static */ void
ProcessDefaultKern::InitKern(VPNum vp, uval ppCount, HATRef hatRef)
{
    SysStatus rc;
    InterruptState is;
    CPUDomainAnnex *cda;
    static ProcessDefaultKern *kernProc;

    if (vp == 0) {
	PMRef pmref;
	rc = PMKern::Create(pmref);

	tassertMsg(_SUCCESS(rc), "woops\n");

	kernProc = new ProcessDefaultKern(hatRef, pmref, "TheKern");

	CObjRootSingleRepPinned::Create(kernProc,
					(RepRef)GOBJK(TheProcessRef));

	kernProc->vpList.initKern(ppCount);

	passertMsg(
	    PageAllocatorKernPinned::realToVirt(0) >= KERNEL_REGIONS_END,
	    "Memory map is wrong - v maps r overlaps kernel virtual\n");

	kernProc->setRegionsBounds(KERNEL_REGIONS_START, KERNEL_REGIONS_START,
				   KERNEL_REGIONS_END);

	DREF(hatRef)->attachProcess(GOBJK(TheProcessRef));
	// NOTE, don't put in ProcessSet until later,
	// since paged memory not yet available
    }

    // Create the idle loop dispatcher for this vp.
    uval idleDspMem;
    Dispatcher *idleDsp;
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(idleDspMem, PAGE_ROUND_UP(sizeof(Dispatcher)));
    tassertMsg(_SUCCESS(rc), "allocPages for idleDspMem failed\n");
    idleDsp = (Dispatcher *) idleDspMem;
    idleDsp->init(SysTypes::DSPID(1, vp));
    idleDsp->storeProgInfo(0, "IdleLoop");
    idleDsp->hasWork = 1;	// idle loop always has "work" to do
    cda = new CPUDomainAnnex;
    cda->init(KernelScheduler::PRIORITY_CLASS_IDLE,
	      KernelScheduler::MIN_WEIGHT,
	      KernelScheduler::MAX_QUANTUM_MICROSECS,
	      KernelScheduler::MAX_PULSE_MICROSECS);
    rc = kernProc->vpList.createDispatcher(cda, SysTypes::DSPID(1, vp),
					   IdleLoopDesc, idleDspMem,
					   /*initMsgLength*/ 0,
					   /*initMsg*/ NULL,
					   GOBJK(TheProcessRef),
					   hatRef);
    tassertMsg(_SUCCESS(rc), "createDispatcher idle failed\n");

    // Create the main kernel-process dispatcher for this vp.
    cda = new CPUDomainAnnex;
    cda->init(KernelScheduler::PRIORITY_CLASS_KERNEL,
	      KernelScheduler::MAX_WEIGHT,
	      KernelInfo::OnSim() ? 10000 :
				    KernelScheduler::MIN_QUANTUM_MICROSECS,
	      KernelInfo::OnSim() ? 10000 :
				    KernelScheduler::MIN_PULSE_MICROSECS);
    EntryPointDesc entry;
    entry.nullify();
    rc = kernProc->vpList.createDispatcher(cda, SysTypes::DSPID(0, vp), entry,
					   uval(extRegsLocal.dispatcher),
					   /*initMsgLength*/ 0,
					   /*initMsg*/ NULL,
					   GOBJK(TheProcessRef),
					   hatRef);
    tassertMsg(_SUCCESS(rc), "createDispatcher main failed\n");

    /*
     * We're currently running on the "unrunnable" CDA created in
     * DispatchQueue::init().  Do a context switch to get onto the new kernel
     * CDA and dispatcher.
     */
    disableHardwareInterrupts(is);
    exceptionLocal.kernelProcessAnnex =
	exceptionLocal.dispatchQueue.getNextProcessAnnex();
    exceptionLocal.kernelProcessAnnex->switchContextKernel();
    enableHardwareInterrupts(is);

    // Create a CDA for servers to use on this processor.  Stash its pointer
    // in exceptionLocal where ProgExec::CreateFirstDispatcher() (in crtKernel.C)
    // can find it.
    cda = new CPUDomainAnnex;
    cda->init(KernelScheduler::PRIORITY_CLASS_TIME_SHARING,
	      KernelScheduler::MAX_WEIGHT,
	      KernelInfo::OnSim() ? 10000 :
				    KernelScheduler::MIN_QUANTUM_MICROSECS,
	      KernelInfo::OnSim() ? 10000 :
				    KernelScheduler::MIN_PULSE_MICROSECS);
    exceptionLocal.serverCDA = cda;
}

/* virtual */ SysStatusUval
ProcessDefaultKern::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
				PageFaultNotification *pn, VPNum vp)
{
    SysStatusUval retvalue;
    // for the kernel page faults handled on thread, not reflected
    passertMsg(pn == NULL, "non-disabled handleFault for kernel\n");
    retvalue = ProcessShared<AllocPinnedGlobalPadded>::handleFault(
	pfinfo, vaddr, pn, vp);
    return (retvalue);
}

/*virtual*/ SysStatus
ProcessDefaultKern::kill()
{
    KernelExit(/*killThinwire*/ 0, /*physProcs*/ 1, /*ctrlFlags*/ 0);
    /* NOTREACHED */
    return (0);
}
