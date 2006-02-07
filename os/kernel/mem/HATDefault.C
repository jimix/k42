/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HATDefault.C,v 1.73 2005/07/06 11:43:27 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Miscellaneous place for early address transation
 * stuff.
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/HATDefault.H"
#include "mem/PageAllocatorKern.H"
#include "proc/Process.H"
#include "mem/SegmentHATPrivate.H"
#include "mem/HATKernel.H"
#include <trace/traceMem.h>
#include <cobj/CObjRootSingleRep.H>

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::Create(HATRef& ref)
{
    HATDefault* hd = new HATDefault;
    if (!hd) {
	tassert(0, err_printf("couldn't create new hat\n"));
	return -1;
    }
    ref = (HATRef)CObjRootSingleRep::Create(hd);
    return 0;
}

/*
 * Some basic initialization, note, all HAT's are dynamically allocated
 * and this functionality is required for all of them
 */
template<class ALLOC>
void
HATDefaultBase<ALLOC>::init()
{
    VPNum vp;
    for (vp=0;vp<Scheduler::VPLimit;vp++) {
	byVP[vp].segps=0;
	byVP[vp].lock.init();
	byVP[vp].segmentList.init();
	byVP[vp].pp = VPNum(-1);
    }
    segmentList.init();
    glock.init();
}

/*
 * We require that destroy be called after all activity on the address
 * space is done.  See the regionlist logic in Process for this.
 * We unmap all the regions first now.  To avoid this, we'd need
 * an alternate form region destroy which synchonizes all map/unmap requests
 * but skips the unmap.
 *
 * Destroy is called once and must deal with all vp's
 *
 * We assume that no kernel VP is borrowing this address space any more.
 *     (See ProcessVPList)
 *
 * I believe this can all be done without locks, but I don't want to
 * try and prove it.
 */
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::destroy(void)
{
    //err_printf("HATDefaultBase %p destroying\n", getRef());

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    SysStatus rc=0;
    VPSet* dummy;

    //Get rid of all the segments
    SegmentList<ALLOC>* restart;
    SegmentHATRef ref;

    //Use region logic, setting region bounds to all of memory
    uval regionAddr = 0;
    uval regionEnd = (uval)(-1);
    uval segmentAddr, segmentEnd;
    uval vp;

    /*
     * clean up the global list
     */
    glock.acquire();
    // regionAddr is updated by delete to be correct for this segment
    restart = &segmentList;
    while (0<=(rc=SegmentList<ALLOC>::DeleteSegment(regionAddr, regionEnd,
					segmentAddr, segmentEnd,
					ref, dummy, restart))) {
	tassert(rc==0,err_printf("Impossible in HATDefaultBase::destroy()\n"));

	//err_printf("HATDefaultBase %p destroying seg %p\n", getRef(), ref);
	DREF(ref)->destroy();
    }


    glock.release();

    for (vp=0;vp<Scheduler::VPLimit;vp++) {
	byVP[vp].lock.acquire();
	// Free segment table
	if (byVP[vp].segps) {
	    //err_printf("HATDefaultBase %p destroying segtable %p on %ld\n",
	    //       getRef(), byVP[vp].segps, vp);
	    byVP[vp].segps->destroy();
	}
	// Free local copy of segmentHAT list
	restart = &(byVP[vp].segmentList);
	while (0<=(rc=SegmentList<ALLOC>::DeleteSegment(regionAddr, regionEnd,
						       segmentAddr, segmentEnd,
						       ref, dummy, restart)));

	byVP[vp].lock.release();
    }

    // schedule the object for deletion
    destroyUnchecked();

    return 0;
}

// region attaches only if it is providing a shared segmentHATRef
// otherwise, segmentHAT's are created as needed.
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::attachRegion(uval regionAddr, uval regionSize,
			 SegmentHATRef segmentHATRef)
{
    SysStatus rc;
    VPSet *dummy;
    AutoLock<LockType> al(&glock);

    if (!segmentHATRef)
	return 0;

    if ((regionAddr|regionSize)&SEGMENT_MASK)
	return -1;

    rc = segmentList.addSegment(regionAddr, regionSize, segmentHATRef, dummy);

    return rc;
}


// Unmap the range on all vp's which are executing on this physical processor
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::unmapRangeLocal(uval rangeAddr, uval rangeSize)
{
    SysStatus rc=0;
    SegmentList<ALLOC>* restart;
    SegmentHATRef ref;
    HATRef hatRef = getRef();
    uval rangeEnd = rangeAddr+rangeSize;
    uval segmentAddr, segmentEnd;
    VPNum myPP,vp;
    VPSet* dummy;

    TraceOSMemHatDefDetReg(rangeAddr, rangeSize);

    //err_printf("HATDefaultBase %p unmapRangeLocal a %lx, s %lx\n", getRef(),
    //       rangeAddr, rangeSize);
    myPP = Scheduler::GetVP();		// physical processor

    for (vp=0;vp<Scheduler::VPLimit;vp++) {
	if (byVP[vp].pp == myPP) {
	    //err_printf("HATDefaultBase %p unmapRange vp %d pp %d\n",
	    //       getRef(), vp, byVP[vp].pp);
	    byVP[vp].lock.acquire();
	    restart = &(byVP[vp].segmentList);
	    while (0<=(rc=SegmentList<ALLOC>::DeleteSegment(
		rangeAddr, rangeEnd, segmentAddr, segmentEnd,
		ref, dummy, restart))) {
		if (rc==0) {
		    // no longer used by this vp.  
		    // segmenthat reduces use count, goes away if not needed.
		    // In any case, the segment hat must unmap the segment.
		    //err_printf("HATDefaultBase %p unmapRange detach %p\n",
		    //       getRef(), ref);
		    rc = DREF(ref)->detachHAT(hatRef, segmentAddr,vp);
		    tassert(_SUCCESS(rc), err_printf("oops\n"));
		} else {
		    // unmap the pages in which the region ovelaps the segment.
		    // this should only occur for private segments
		    //err_printf("HATDefaultBase %p unmapRange unmapr %p\n",
		    //       getRef(), ref);
		    rc = DREF(ref)->unmapRange(hatRef, segmentAddr, segmentEnd,
					       rangeAddr, rangeEnd, vp);
		    tassert(_SUCCESS(rc), err_printf("oops\n"));
		}
	    }
	    byVP[vp].lock.release();
	}
    }
    return 0;
}


template<class ALLOC>
struct HATDefaultBase<ALLOC>::UnmapRangeMsg {
    sval     barrier;
    ThreadID waiter;
    HATRef   myRef;
    uval     start;
    uval     size;
};

template<class ALLOC>
/* static */ SysStatus
HATDefaultBase<ALLOC>::UnmapRangeMsgHandler(uval msgUval)
{
    UnmapRangeMsg *const msg = (UnmapRangeMsg *) msgUval;
    uval w;
    SysStatus rc;
    rc = DREF((HATDefaultBase **)msg->myRef)->
	unmapRangeLocal(msg->start,msg->size);
    w = FetchAndAddSignedVolatile(&msg->barrier,-1);
    // w is value before subtract
    if (w==1) Scheduler::Unblock(msg->waiter);
    return rc;
}


/*
 * Every region must unmap before it destroys itself.  This must occur
 * after all outstanding map request are done.
 * unmap cleans up storage and unmaps all frames in the region
 * The region passes the set of physical processors that have ever accessed
 * this range of memory.
 * This protocol assumes that the hat can always do this efficiently without
 * being passed information about which frames exist.
 * The routine will do the unmap on every processor which has seen page
 * faults to the region (the passed ppset) or which have segment hats.  On
 * each, we must clean up all the vp's on that processor.
 * we must also clean up the global list as well.
 *
 * there is no locking problem because the region logic guarantees that no
 * new mapping faults will occur in the relevant range of addresses
 * until detach completes on ALL processors.
 *
 * this call can be used to unmap any range
 */
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::unmapRange(uval rangeAddr, uval rangeSize, VPSet ppset)
{
    SysStatus rc=0;
    SegmentList<ALLOC>* restart;
    SegmentHATRef ref;
    VPSet *tmpset, unionset;
    VPNum numprocs, pp, mypp;
    uval rangeEnd = rangeAddr+rangeSize;
    uval segmentAddr, segmentEnd;
    UnmapRangeMsg msg;
    /* we first cleanup the global list and find what processors have cached
     * copies of the segment in their local lists.
     */
    glock.acquire();
    restart = &segmentList;
    while (0<=(rc = SegmentList<ALLOC>::DeleteSegment(
	rangeAddr, rangeEnd,  segmentAddr, segmentEnd,
	ref, tmpset, restart))) {
	// accumulate all pp's which have seen any relevant segment
	unionset.unite(*tmpset);
	if(rc == 0) {
	    // if we delete the whole segmentHAT, we must visit
	    // every processor that's seen it, no matter what this
	    // region says
	    ppset.unite(*tmpset);
	}
    }
    glock.release();
    // only visit pp's which this region has seen
    unionset.intersect(ppset);

    msg.barrier	    = 1;
    msg.waiter	    = Scheduler::GetCurThread();
    msg.myRef       = getRef();
    msg.start       = rangeAddr;
    msg.size        = rangeSize;

    mypp = Scheduler::GetVP();		// physical processor
    numprocs = DREFGOBJK(TheProcessRef)->vpCount();
    for (pp = unionset.firstVP(numprocs); pp < numprocs;
	 pp = unionset.nextVP(pp, numprocs)) {
	if (pp != mypp) {
	    //err_printf("Remote unmap for %d\n", vp);
	    FetchAndAddSignedVolatile(&msg.barrier,1);
	    rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
					 SysTypes::DSPID(0, pp),
					 UnmapRangeMsgHandler, uval(&msg));
	    tassert(_SUCCESS(rc),err_printf("UnmapRange remote failed\n"));
	}
    }

    if (unionset.isSet(mypp)) {
	rc = unmapRangeLocal(rangeAddr, rangeSize);
	tassert(_SUCCESS(rc), err_printf("oops\n"));
    }

    FetchAndAddSignedVolatile(&msg.barrier,-1);

    while (msg.barrier != 0) {
	Scheduler::Block();
    }

    //err_printf("All done unmapRange\n");

    return 0;
}

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::getSegmentTable(VPNum vp, SegmentTable *&st)
{
    if (!byVP[vp].segps) {
	TraceOSMemAllocSegment(vp);
	SysStatus rc = SegmentTable::Create(byVP[vp].segps);
	if (!_SUCCESS(rc)) {
	    tassert(0, err_printf("no space for HAT segment table\n"));
	    return rc;
	}
    }
    // record current pp on every call - may change on migration
    byVP[vp].pp = Scheduler::GetVP();
    st = byVP[vp].segps;
    return 0;
}

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::createSegmentHATPrivate(SegmentHATRef& segmentHATRef)
{
    return SegmentHATPrivate::Create(segmentHATRef);
}

/*
 * called holding the vp lock
 */

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::findSegment(uval virtAddr, SegmentHATRef &result,
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
    rc = byVP[vp].segmentList.addSegment(
	virtAddr&(~(SEGMENT_SIZE-1)), SEGMENT_SIZE, result, dummy);
    return rc;
}

/*
 * get information from the global list and update ppset with this processor
 */
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::findSegmentGlobal(uval virtAddr, SegmentHATRef &result,
				   uval createIfNoSeg)
{
    SysStatus rc = 0;
    SegmentHATRef segmentHATRef;
    VPSet *ppset;
    AutoLock<LockType> al(&glock);

    tassertMsg(virtAddr < KERNEL_REGIONS_END,
	       "%lx should be handled by the low level page fault code\n",
	       virtAddr);
    segmentHATRef = segmentList.findSegment(virtAddr, ppset);

    //err_printf("findSegmentGlobal %lx/%ld -> %p\n", virtAddr, createIfNoSeg,
    //       segmentHATRef);
    if (!segmentHATRef) {
	if (!createIfNoSeg) return HAT::NOSEG;
	// round address down to segment boundary
	uval segVirtAddr = virtAddr&(~(SEGMENT_SIZE-1));
	// create private segment using protected function
	// so kernel can do it differently - see HATKernel
	rc = createSegmentHATPrivate(segmentHATRef);
	tassert(!rc, err_printf("no memory for SegmentHAT\n"));
	if (rc)
	    return rc;
	rc = segmentList.addSegment(segVirtAddr,SEGMENT_SIZE,
				    segmentHATRef, ppset);
	tassert(!rc, err_printf("no memory for SegmentList\n"));
	if (rc)
	    return rc;
    }
    ppset->addVP(Scheduler::GetVP());
    result = segmentHATRef;
    return rc;
}

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::mapPage(uval physAddr, uval virtAddr, uval len,
			       AccessMode::pageFaultInfo pfinfo,
			       AccessMode::mode access, VPNum vp,
			       uval wasMapped,
			       uval createIfNoSeg)
{
    uval pte_index, page_index;
    SegmentHATRef segmentHATRef;
    SegmentHAT * segmentHAT;
    SysStatus rc = 0;
    SegmentTable* segp;			// Segment table for this vp
    AutoLock<LockType> al(&(byVP[vp].lock));

    // record current pp on every call - may change on migration
    byVP[vp].pp = Scheduler::GetVP();

    if ((rc = getSegmentTable(vp, segp))) return rc;

    pte_index = virtAddr >> LOG_SEGMENT_SIZE;
    page_index = (virtAddr & (SEGMENT_SIZE-1))>>LOG_PAGE_SIZE;

    if ((rc=findSegment(virtAddr, segmentHATRef, vp, createIfNoSeg))) {
	return rc;
    }
    segmentHAT = DREF(segmentHATRef);

    /*
     * It is possible for the segment to be UNMAPPED even though
     * we don't have a segmentFault indication - in which case
     * there will ultimately be another fault with the indication on
     * We test the indicator to optimize the normal case - the segment
     * is mapped and we don't need to map it again
     */
    if (AccessMode::isSegmentFault(pfinfo)) {
	rc = segmentHAT->mapSegment(segp, virtAddr, len, vp);
	tassertWrn(!rc,"mapSegment failed in HATDefault\n");
	if (rc)
	    return rc;
    }

    rc = segmentHAT -> mapPage(physAddr, virtAddr, len, pfinfo, access,
			       vp, wasMapped);

    return rc;

}

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::getSegmentHAT(uval virtAddr, SegmentHATRef &shr)
{
    SysStatus rc = 0;
    if ((rc = findSegmentGlobal(virtAddr, shr, /*createIfNoSeg*/ 1))) {
	tassert(0, err_printf(
	    "findSegmentGlobal failed initializing kernel address space\n"));
    }
    return rc;
}

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::attachProcess(ProcessRef processRefarg)
{
    processRef = processRefarg;
    return 0;
}


// returns process this hat is attached to
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::getProcess(ProcessRef &processRefarg)
{
    processRefarg = processRef;
    return 0;
}

// remove a mapping from all VPs on this processor
// FIXME can be done cheaper
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::unmapPage(uval vaddr)
{
    SysStatus rc;
    SegmentHATRef segmentHATRef;
    HATRef hatRef = getRef();

    TraceOSMemHatDefDetReg(vaddr, PAGE_SIZE);

    rc = findSegmentGlobal(vaddr, segmentHATRef, 0);
    if (_FAILURE(rc)) {
	return 0;
    }

    /* shared segments are never really "unmapped" via the hat
     * rather, the FCM calls unmapPage on the SegmentHAT.
     * But on some machines, the TLB must be invalidated for
     * each region mapping the shared segment.  On these,
     * we go throught the region unmap interface and when
     * we get here call unmapSharedPage which does the tlb
     * invalidate.
     * For private mappings, this is the normal unmap path
     */
    return DREF(segmentHATRef)->unmapSharedPage(hatRef, vaddr);
}

// remove all mappings and other pp specific info for a vp
// preliminary to vp migration
// must run on the current (pre migration) pp
// caller must guarantee no memory activity (i.e. faults) occur
// from detachVP until attachVP
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::detachVP(VPNum vp)
{
    SysStatus rc;
    SegmentList<ALLOC>* restart;
    SegmentHATRef ref;
    HATRef hatRef = getRef();
    uval segmentAddr, segmentEnd;
    VPSet* dummy;

    byVP[vp].lock.acquire();
    tassertMsg(byVP[vp].pp == Scheduler::GetVP(),
	       "detachVP called on wrong pp\n");
    tassertMsg(byVP[vp].segps != exceptionLocal.currentSegmentTable,
	       "detaching current segment table\n");
    restart = &(byVP[vp].segmentList);
    while (0<=SegmentList<ALLOC>::FindNextSegment(
	0, uval(-1), segmentAddr, segmentEnd, ref, dummy, restart)) {
	rc = DREF(ref)->unmapRange(hatRef, segmentAddr, segmentEnd,
				   0, uval(-1), vp);
	tassertMsg(_SUCCESS(rc), "oops\n");
    }
    byVP[vp].lock.release();
    return 0;
}

template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::attachVP(VPNum vp)
{
    SysStatus rc;
    SegmentTable* segp;
    SegmentList<ALLOC>* restart;
    SegmentHATRef ref;
    uval segmentAddr, segmentEnd;
    VPSet* ppset;

    byVP[vp].lock.acquire();
    rc = getSegmentTable(vp, segp);
    tassertMsg(_SUCCESS(rc), "but this cant fail\n");
    // fix up segment table after move - for example get rid of
    // segment mappings which may now be wrong
    segp->changePP();
    // tell each SegmentHAT that the vp to pp map has changed
    // also, add this pp to the ppset in the global list by looking it up
    restart = &(byVP[vp].segmentList);
    while (0<=SegmentList<ALLOC>::FindNextSegment(
	0, uval(-1), segmentAddr, segmentEnd, ref, ppset, restart)) {
	rc = DREF(ref)->changePP(vp);
	tassertMsg(_SUCCESS(rc),"how can this fail?\n");
	// side effect of findSegmentGlobal is recording pp
	rc = findSegmentGlobal(segmentAddr, ref, 0 /*dont create*/);
	tassertMsg(_SUCCESS(rc),"how can this fail?\n");
    }
    byVP[vp].lock.release();
    /*
     * if we just keep on adding new processors to the global ppset of
     * each segment without removing old ones, we will eventually
     * visit every processor for every unmap.  But just because on vp
     * has migratated off a pp doesn't mean another isn't still there.
     * So we do a brute force check to bound the problem.  We compute
     * a set of pp's we can possibly be interested in and intersect it
     * with each global segment ppset.
     */
    VPSet activeSet;
    activeSet.init();

    /*
     * if more than one attach is happening (can it?) it
     * doesn't matter which one does this first,
     * as long as one of them does it last without
     * interference.
     */
    glock.acquire();
    for(vp = 0; vp<Scheduler::VPLimit;vp++) {
	byVP[vp].lock.acquire();
	activeSet.addVP(byVP[vp].pp);
	byVP[vp].lock.release();	
    }
    restart = &segmentList;
    while (0<=SegmentList<ALLOC>::FindNextSegment(
	0, uval(-1), segmentAddr, segmentEnd, ref, ppset, restart)) {
	ppset->intersect(activeSet);
    }
    glock.release();
    return 0;
}

// used in debugging - returns true iff vaddr is backed by
// a private segment
template<class ALLOC>
SysStatus
HATDefaultBase<ALLOC>::privateSegment(uval vaddr)
{
    SysStatus rc;
    SegmentHATRef shr;
    rc = findSegmentGlobal(vaddr, shr, /*createIfNoSeg*/ 0);
    if (_FAILURE(rc)) return 0;
    return DREF(shr)->privateSegment();
}

template class HATDefaultBase<AllocGlobalPadded>;
template class HATDefaultBase<AllocPinnedGlobalPadded>;
