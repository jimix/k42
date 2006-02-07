/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HATKernel.C,v 1.48 2004/11/05 16:24:01 marc Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Miscellaneous place for early address transation
 * stuff.
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/HATKernel.H"
#include "mem/PageAllocatorKern.H"
#include "mem/SegmentHATPrivate.H"
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>

#include __MINC(HATKernel.C)

SysStatus
HATKernel::ClassInit(VPNum vp)
{
    static HATKernel *hd;
    if (vp==0) {
	// on first processor allocate new clustered object
	hd = new HATKernel;
	tassert(hd, err_printf("alloc of HATKernel failed!!!!\n"));
    }
    SysStatus rc;
    rc = hd->initSegmentHATs();
    tassert(_SUCCESS(rc),
	    err_printf("kernel HATKernel::Init failed on vp %ld\n", vp));
    if (vp==0) {
	CObjRootSingleRepPinned::Create(
	    hd, (RepRef)GOBJK(TheKernelHATRef));
    }
    return rc;
}

SysStatus
HATKernel::createSegmentHATPrivate(SegmentHATRef& segmentHATRef)
{
    return SegmentHATKernel::Create(segmentHATRef);
}
/*
 * called holding the vp lock
 */
/*
 *FIXME
 * this copy is to support segment mapping kludge
 * which will be removed when we remove automatic mapping
 * of kernel segments
 */
SysStatus
HATKernel::findSegment(uval virtAddr, SegmentHATRef &result,
				   VPNum vp, uval createIfNoSeg)
{
    SysStatus rc = 0;
    VPSet *dummy;
    // find segment hat for this address
    result = byVP[vp].segmentList.findSegment(virtAddr);
    if (result) return 0;

    // get the truth; also keeps track of new cached ref to segment on proc
    rc = findSegmentGlobal(virtAddr, result, createIfNoSeg);
    if (_FAILURE(rc)) return rc;
    // map segment here since kernel does not get seg missing faults
    DREF(result)->mapSegment(byVP[vp].segps, virtAddr, PAGE_SIZE, vp);
    byVP[vp].pp = Scheduler::GetVP();	// physical processor
    rc = byVP[vp].segmentList.addSegment(
	virtAddr&(~(SEGMENT_SIZE-1)), SEGMENT_SIZE, result, dummy);
    return rc;
}



// this is just here for assertion, could call base class directly
SysStatus
HATKernel::getSegmentTable(VPNum vp, SegmentTable *&st)
{
    tassert(byVP[vp].segps,err_printf("woops, not allocated seg in kernel\n"));
    return HATDefaultBase<AllocPinnedGlobalPadded>::getSegmentTable(vp,st);
}

/*
 * For kernel, we are already running on a segment table on this
 * processor, and we just want to build up first class data structures
 * for it, and initialize segment table pointer in HAT
 */
SysStatus
HATKernel::initSegmentHATs(void)
{
    VPNum vp = Scheduler::GetVP();
    // execute this exactly once on initialization per-processor,
    // don't need to worry about locks

    byVP[vp].segps = exceptionLocal.kernelSegmentTable;

    switchToKernelAddressSpace();

    return 0;
}

void
HATKernel::switchToKernelAddressSpace()
{
    disableHardwareInterrupts();
    byVP[Scheduler::GetVP()].segps->switchToAddressSpace();
    exceptionLocal.currentSegmentTable = byVP[Scheduler::GetVP()].segps;
    enableHardwareInterrupts();
}
