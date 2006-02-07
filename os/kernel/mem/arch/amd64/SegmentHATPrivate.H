#ifndef __SEGMENT_HATPRIVATE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __SEGMENT_HATPRIVATE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentHATPrivate.H,v 1.13 2003/01/13 22:36:54 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * amd64 version of "page tables"
 * **************************************************************************/

#include "mem/SegmentHAT.H"
#include "scheduler/Scheduler.H"
#include <mem/Access.H>
#include <mmu.H>

/* for amd64 a segment is associated to one pde entry worth of virtual storage, i.e. 2MB,
 * and its pte page for a given address space (process).
 *The pte page and underpinning pde, pdp are allocated on demand at fault time.
 * The pde and pdp pages are allocated pinned, the pte page itself is pageable. XXX
 * A SegmentHATPrivate is a singly replicated clustered object, i.e.
 * acts like a global object but still faults and goes thru the misshandler
 * to access the single representative.  It contains pointers to each PTE
 * page table in all virtual processor(s) on which it is currently mapped
 * for this process.
 */
class SegmentHATPrivate : public SegmentHAT {

protected:

    /* needs to be protected (or public) to be accessed by the kernel amd shared version of SegmentHAT.
     */
    typedef PTE L2_PageTable[PDP::EntriesPerPage];	// PDP
    typedef PTE L3_PageTable[PDE::EntriesPerPage];	// PDE
    typedef PTE L4_PageTable[PTE::EntriesPerPage];	// PTE

    /* to avoid cache line bouncing due to false sharing we place those locks in
     * separate cache lines (32 bytes on this machine).
     * (PtrBLock<L2_PageTable> L2PageTableP[Scheduler::VPLimit];)
     */
    struct padded {
	uval8                  y1[16];
	PtrBLock<L4_PageTable> x;
	uval8                  y2[16-8];
    };
    struct padded L4PageTableP[Scheduler::VPLimit];
    VPNum vpToPP[Scheduler::VPLimit];		// phys proc this is on
    uval  vpCount;

    SegmentHATPrivate() : SegmentHAT()
    {
	uval i;
	for (i=0; i<Scheduler::VPLimit; i++) {
	    L4PageTableP[i].x.init(NULL);
	    vpToPP[i] = VPNum(-1);
	}
	vpCount = 0;
    }

public:
    // UnMapShared true means shared segments need to be unmapped
    // from each region to fix up tlb.  Not true on all architectures.
    enum {UnMapShared = 1};
    static SysStatus Create(SegmentHATRef& ref);

    /* this and the ROOT allocation do not need to be PINNED
     * for user level: we use pageable allocation.
     * we have already allocated
     * one cache line per page table pointer/lock, hence not even padded.
     */
    DEFINE_GLOBAL_NEW(SegmentHATPrivate);

    virtual SysStatus mapSegment(SegmentTable* segp, uval virtAddr,
				 VPNum vp);

    virtual SysStatus mapPage(uval physAddr, uval virtAddr,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp, 
			      uval wasMapped);

    virtual SysStatus detachHAT(HATRef hatRef, uval virtAddr, VPNum vp);

    virtual SysStatus unmapRange(HATRef hatRef,
				 uval segmentAddr, uval segmentEnd,
				 uval regionAddr, uval regionEnd,
				 VPNum vp );

    virtual SysStatus unmapPage(HATRef hatRef, uval virtAddr);

    virtual SysStatus unmapSharedPage(HATRef hatRef, uval virtAddr) {
	return unmapPage(hatRef, virtAddr);
    }

    // used in migration of vp to new pp 
    virtual SysStatus changePP(VPNum vp) {
	vpToPP[vp] = Scheduler::GetVP();
	return 0;
    }

    // destroy - called only if whole address space is being blown away
    virtual SysStatus destroy();
    // for debugging
    virtual SysStatus privateSegment() { return 1;};
};

/*
 * Derived from SegmentHATPrivate for making
 * pinned kernel SegmentHAT's * These objects look like clustered
 * objects but can't be in the (pageable) object translation table -
 * so we make private refs.
 */

class SegmentHATKernel : public SegmentHATPrivate {
protected:
    DEFINE_PINNEDGLOBALPADDED_NEW(SegmentHATKernel);

public:
    static SysStatus Create(SegmentHATRef& ref);

    // during kernel init, same page tables already exist, and need
    // segment hats retrofitted to them.  This call completes that
    virtual SysStatus initSegmentHAT(uval virtAddr,
				     uval framePhysAddr, VPNum vp);

    // used in migration of vp to new pp - illegal in kernel
    virtual SysStatus changePP(VPNum vp) {
	passertMsg(0, "Kernel segment asked to migrate to new pp\n");
	return 0;
    }
};

// Derived SegmentHAT for shared FCMs; shared across multiple apps
class SegmentHATShared : public SegmentHATPrivate {
protected:
    DEFINE_GLOBALPADDED_NEW(SegmentHATShared);

public:
    static SysStatus Create(SegmentHATRef& ref);

    /*
     * Shared segments create mappings for each processor, not each VP
     * of a particular client.  This means that shared segments cannot
     * support per-processor memory.
     * FIXME - this implementation is probably wrong.  We defer a rework
     * until we need to deal with process migration.
     *
     * For now, we use the kludge of using the current pp as a vp on
     * shared segment calls.
     */

    /*
     * methods to which pp is vp kludge applies
     */
    virtual SysStatus mapSegment(SegmentTable* segp, uval virtAddr,
				 VPNum vp) {
	return SegmentHATPrivate::mapSegment(
	    segp, virtAddr, Scheduler::GetVP());
    }
					     

    virtual SysStatus mapPage(uval physAddr, uval virtAddr,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp,
			      uval wasMapped) {
	return SegmentHATPrivate::mapPage(
	    physAddr, virtAddr, pfinfo, access, Scheduler::GetVP(),
	    wasMapped);
    }

    /*
     * end of kludge methods.
     */
    

    // not supported for shared segments
    virtual SysStatus unmapRange(HATRef hatRef,
				 uval segmentAddr, uval segmentEnd,
				 uval regionAddr, uval regionEnd,
				 VPNum vp );

    // not supported for shared segments
    virtual SysStatus unmapPage(HATRef hatRef, uval virtAddr);

    virtual SysStatus unmapSharedPage(HATRef hatRef, uval virtAddr);

    // we provide new detach/destroy methods to deal with sharing issues
    virtual SysStatus detachHAT(HATRef hatRef, uval virtAddr, VPNum vp);

    // if everything is right, this should never be called, but we provide
    // dummy implementation anyway so we can trap any errors
    virtual SysStatus destroy();

    // owner of segmentHAT, fcm, uses this instead of other destroy, so that
    // accidental destroys will be caught
    virtual SysStatus sharedSegDestroy();

    // used in migration of vp to new pp - shared segments ignore this
    virtual SysStatus changePP(VPNum vp) { return 0;};
    // for debugging
    virtual SysStatus privateSegment() { return 0;};
};


