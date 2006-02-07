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
 * $Id: SegmentHATPrivate.H,v 1.51 2004/09/22 20:17:40 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * In PwrPC a SegmentHAT "owns" one or more SID's
 * **************************************************************************/

#include "mem/SegmentHAT.H"
#include <mem/Access.H>
#include <misc/hardware.H>
#include <defines/optimizations.H>

class SegmentHATPrivate : public SegmentHAT {
protected:
    uval logPageSize;
    uval vpCount;			// number of vp's attached
    enum {MappedNodes=32};
    union Mapped {
	uval8 bits[256];
	Mapped *mapped[MappedNodes];
    };
       
    struct SegStruct {
	BLock        lock;
	SegDesc      segDesc;
	VPNum        pp;		// physical processor
	uval32       maxPageOffset;	// all seen less than this
	uval32       mappedIsTree;	// mapped is tree form
	Mapped       mapped;
    };

    SegStruct seg0;			// allocate the first one in object
    SegStruct *seg[Scheduler::VPLimit];

    // abandon segment on this vp - alternative to lots of unmaps
    // return 1 if can't do it - we never unmap kernel segments this way
    virtual SysStatus lockedUnmapSegment(
	HATRef hatRef, uval virtAddr, VPNum vp);
    
    static uval NoAbandonVsid;		// see comments in C file
    virtual void* allocMaybePinned(uval size) {
	return allocGlobalPadded(size);
    }
    virtual void freeMaybePinned(void* addr, uval size) {
	freeGlobalPadded(addr, size);
    }
public:
    // UnMapShared true means shared segments need to be unmapped
    // from each region to fix up tlb.  Not true on all architectures.
    enum {UnMapShared = 0};

    static SysStatus Create(SegmentHATRef& ref);

    DEFINE_GLOBALPADDED_NEW(SegmentHATPrivate);

    virtual SysStatus mapSegment(SegmentTable* segp, uval virtAddr, 
				 uval pageSizeArg, VPNum vp);

    virtual SysStatus mapPage(uval physAddr, uval virtAddr, uval pageSizeArg,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp,
			      uval wasMapped);

    // remove this segment from this vp - if last really detach
    virtual SysStatus detachHAT(HATRef hatRef, uval segmentAddr, VPNum vp);

    // unmap range on this vp
    virtual SysStatus unmapRange(
	HATRef hatRef, uval segmentAddr, uval segmentEnd,
	uval regionAddr, uval regionEnd, VPNum vp);

    // unmap range on this vp
    SysStatus lockedUnmapRange(
	HATRef hatRef, uval segmentAddr, uval segmentEnd,
	uval regionAddr, uval regionEnd, VPNum vp);

   // unmap virtAddr from all vp's on this pp
    virtual SysStatus unmapPage(HATRef hatRef, uval virtAddr);

    virtual SysStatus unmapSharedPage(HATRef hatRef, uval virtAddr) {
	return unmapPage(hatRef, virtAddr);
    }

    // destroy - called only if whole address space is being blown away
    virtual SysStatus destroy();

    // used in migration of vp to new pp 
    virtual SysStatus changePP(VPNum vp);

    // used in debugging
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

    virtual SysStatus lockedUnmapSegment(
	HATRef hatRef, uval virtAddr, VPNum vp) {
	return 1;
    }

    virtual void* allocMaybePinned(uval size) {
	return allocPinnedGlobalPadded(size);
    }
    virtual void freeMaybePinned(void* addr, uval size) {
	freePinnedGlobalPadded(addr, size);
    }

public:
    //FIXME - only until mapping fault handler stops mapping normal
    //        kernel segments
    virtual SysStatus mapSegment(SegmentTable* segp, uval virtAddr,
				 uval pageSizeArg, VPNum vp);

    static SysStatus Create(SegmentHATRef& ref);

    virtual SysStatus initSegmentHAT(SegDesc segDesc, VPNum vp);

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

    /*
     * never abandone the segid in shared segment
     */
    virtual SysStatus lockedUnmapSegment(
	HATRef hatRef, uval virtAddr, VPNum vp) {
	return 1;
    }

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
				 uval pageSizeArg, VPNum vp) {
	return SegmentHATPrivate::mapSegment(
	    segp, virtAddr, pageSizeArg, Scheduler::GetVP());
    }
					     

    virtual SysStatus mapPage(uval physAddr, uval virtAddr, uval pageSizeArg,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp,
			      uval wasMapped) {
	return SegmentHATPrivate::mapPage(
	    physAddr, virtAddr, pageSizeArg, pfinfo, access, Scheduler::GetVP(),
	    wasMapped);
    }

    virtual SysStatus unmapSharedPage(HATRef hatRef, uval virtAddr) {
	passertMsg(0, "should not be called on powerpc\n");
	return 0;
    }

    // unmap range on this vp
    virtual SysStatus unmapRange(
	HATRef hatRef, uval segmentAddr, uval segmentEnd,
	uval regionAddr, uval regionEnd, VPNum vp) {
	return SegmentHATPrivate::unmapRange(
	    hatRef, segmentAddr, segmentEnd, regionAddr, regionEnd,
	    Scheduler::GetVP());
    }

    /*
     * end of kludge methods.
     */
    
    // remove this segment from this vp - if last really detach
    // we provide new detach/destroy methods to deal with sharing issues
    virtual SysStatus detachHAT(HATRef hatRef, uval segmentAddr, VPNum vp);

    // unmap virtAddr from all vp's on this pp
    virtual SysStatus unmapPage(HATRef hatRef, uval virtAddr);

    // if everything is right, this should never be called, but we provide
    // dummy implementation anyway so we can trap any errors
    virtual SysStatus destroy();

    // owner of segmentHAT, fcm, uses this instead of other destroy, so that
    // accidental destroys will be caught
    virtual SysStatus sharedSegDestroy();

    // used in migration of vp to new pp - shared segments ignore this
    virtual SysStatus changePP(VPNum vp) { return 0;};

    // used in debugging
    virtual SysStatus privateSegment() { return 0;};
};
