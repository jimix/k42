#ifndef __HATDEFAULT_H_
#define __HATDEFAULT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HATDefault.H,v 1.44 2004/01/23 19:50:30 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * Include correct machine dependent version
 * **************************************************************************/

#include "mem/HAT.H"
#include "mem/SegmentHAT.H"
#include "mem/SegmentList.H"
#include <defines/optimizations.H>


template<class ALLOC> class HATDefaultBase : public HAT {
protected:
    HATDefaultBase(){ init(); }

    /*
     * because of a mips compiler internal error, we apparently
     * can't define the constructor outside of the class definition.
     * As a workaround, we define it as above and put the logic
     * inside an init function
     */
    void init();

    typedef BLock LockType;
    LockType      glock;
    SegmentList<ALLOC> segmentList;	// real stuff - guarded by glock
    ProcessRef    processRef;           // process we are attached to
    /*
     * the hat must keep per client vp information anyhow
     * so we also partition the setmentList per client vp
     * instead of per physical processor
     * If this implementation is made into a multi-rep clustered
     * object, that decision should be revisited
     * (One virtue of this approach is that a single lock servers to
     * protect both the segps and the segmentList).
     */
#ifdef PAD_HATDEFAULT
    uval64        filler1[16];
#endif /* #ifdef PAD_HATDEFAULT */
    struct {
	LockType      lock;
	SegmentTable* segps;		// hardware segment table equivalent
	SegmentList<ALLOC> segmentList;	// per vp cache
	VPNum         pp;		// pp for this vp
#ifdef PAD_HATDEFAULT
	uval64        filler2[16];
#endif /* #ifdef PAD_HATDEFAULT */
    } byVP[Scheduler::VPLimit];
    virtual SysStatus createSegmentHATPrivate(SegmentHATRef& segmentHATRef);

    virtual SysStatus findSegmentGlobal(uval virtAddr, SegmentHATRef &result,
				  uval createIfNoSeg);

    virtual SysStatus findSegment(uval virtAddr, SegmentHATRef &result,
				  VPNum vp, uval createIfNoSeg);

    SysStatus unmapRangeLocal(uval rangeAddr, uval rangeSize);
    struct UnmapRangeMsg;
    static SysStatus UnmapRangeMsgHandler(uval msgUval);

public:

    static SysStatus Create(HATRef& ref);

    virtual SysStatus destroy();

    virtual SysStatus mapPage(uval physAddr, uval virtAddr, uval len,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp,
			      uval wasMapped,
			      uval createIfNoSeg=1);

    // returns/creates a segment table for this vp
    virtual SysStatus getSegmentTable(VPNum vp, SegmentTable *&st);

    virtual SysStatus attachRegion(uval regionAddr, uval regionSize,
				    SegmentHATRef segmentHATRef);

    // can only be called when no more accesses are possible to range
    virtual SysStatus unmapRange(uval rangeAddr, uval rangeSize, VPSet ppset);

    // returns SegmentHAT covering virtAddr if one exists
    virtual SysStatus getSegmentHAT(uval virtAddr, SegmentHATRef &shr);

    virtual SysStatus attachProcess(ProcessRef processRef);

    // returns process this hat is attached to
    virtual SysStatus getProcess(ProcessRef &processRef);

    // remove a mapping from all vp's on this pp
    virtual SysStatus unmapPage(uval vaddr);

    // remove all mappings and other pp specific info for a vp
    // preliminary to vp migration on old pp
    virtual SysStatus detachVP(VPNum vp);
    // does post migration work on new pp
    virtual SysStatus attachVP(VPNum vp);

    // used in debugging - returns true iff vaddr is backed by
    // a private segment
    virtual SysStatus privateSegment(uval vaddr);
};

class HATDefault: public HATDefaultBase<AllocGlobalPadded> {
public:
        DEFINE_GLOBALPADDED_NEW(HATDefault);

};
#endif /* #ifndef __HATDEFAULT_H_ */
