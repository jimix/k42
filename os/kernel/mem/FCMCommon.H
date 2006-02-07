#ifndef __FCMCOMMON_H_
#define __FCMCOMMON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCommon.H,v 1.72 2005/08/24 15:00:42 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implements base functionality required by
 * nearly all FCMs
 * This is a pure base class - it can't be used directly
 * **************************************************************************/

#include <misc/ListSimpleKeyLocked.H>
#include <sync/MPMsgMgr.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>
#include "mem/PageDesc.H"
#include "mem/FCM.H"
#include "mem/SegmentHAT.H"

template <class PL, class ALLOC>
class FCMCommon : public FCM {
protected:
    typedef BLock LockType;
    LockType      lock;			// FIXME: lock protects PageList
    FRRef         frRef;		// fr this fcm services
    uval          pageSize;		// size for fixed size FCM's
    PL		  pageList;

    struct RegionInfo {
	PMRef   pm;			// pm for this region
	VPSet   ppset;			// pps that this region is mapped on
	// non-shared-segment region info
	RegionInfo(PMRef p) {pm=p;}
	DEFINE_ALLOC_NEW(RegionInfo)
    };
    // regions mapping to this FCM
    ListSimpleKeyLocked<RegionRef, RegionInfo *, ALLOC> regionList;
    // support function to maintain regioninfo ppset bits
    void updateRegionPPSet(RegionRef reg);

    // for now, we use the common lock to protect list manipulations
    // list keyed by segment offsets of list keyed by access type of segments
    // accessmode for segments is converted to uval, since this is required
    // for generic list stuff to work properly
    typedef ListSimpleKey<uval,SegmentHATRef,ALLOC> SegmentHATByAccessModeList;
    ListSimpleKey<uval,SegmentHATByAccessModeList*,ALLOC> segmentHATList;


    sval          referenceCount;	// users other than Regions
                                        // see addReference()
    PMRef         pmRef;		// current PM for page alloc/dealloc

    uval  pageable:1;			// does fr support paging?
    uval  backedBySwap:1;		// alg'thm slightly diff if swapped
    uval  priv:1;			// belongs (primarily) to one process
    uval  beingDestroyed:1;		// indicates if being destroyed
    uval  mappedExecutable:1;		// its been mapped executable
    /*
     * N.B. noSharedSegments is intended for use by derived FCM's which
     * do not allow shared mappings - see FCMComputation
     * A region can supress shared mappings by defining its
     * isSharedOffset and isSharedVaddr methods to always return false
     */
    uval  noSharedSegments:1;		// force only private mappings
    /*
     * Copy on write initially passes all requests to base
     * An FCM doing this has mapBase set
     */
    uval isCRW:1;
    uval mapBase:1;

#ifdef DILMA_TRACE_PAGE_DIRTY // this should go away soon
    uval dirtyCounter, lastCounterTrace;
#endif // #ifdef DILMA_TRACE_PAGE_DIRTY // this should go away soon

protected:
    PageDesc *findPage(uval fileOffset);

    PageDesc *addPage(uval fileOffset, uval paddr, uval len);

    virtual void detectModified(PageDesc* pg, AccessMode::mode &access,
				AccessMode::pageFaultInfo pfinfo);

    // support function for interacting with HAT to map a page
    virtual SysStatus mapPageInHAT(uval virtAddr,
				   AccessMode::pageFaultInfo pfinfo,
				   AccessMode::mode access, VPNum vp,
				   HATRef hat, RegionRef reg,
				   PageDesc *pg,
				   uval offset);

    // called with PageDesc, with lock held
    // returns with lock held, and either pg->PageDesc for offset
    // not doingIO or pg==NULL if the page disappeared
    // N.B. lock is released while blocked
    virtual void blockOnIO(PageDesc*& pg);

    // block until all io is complete, called with lock held
    // N.B. lock may be released and reaquired
    void finishAllIO(uval clearCopyFlags=0);

    // Remove mappings only for local processor; called as a remote op
    virtual SysStatus unmapPageLocal(uval fileOffset);

    struct UnmapPageMsg;
    static SysStatus UnmapPageMsgHandler(uval msgUval);

    // remove all mapping of this page
    // N.B. unless the page is locked or there are no regions
    // attached this operation is futile
    virtual SysStatus internal_unmapPage(uval offset, uval ppset);
    virtual SysStatus unmapPage(PageDesc* pg) {
        SysStatus rc;
	tassertMsg( (pg->mapped != PageDesc::CLEAR), 
		    "perf opt, should check this call outside contact orran\n" );
	rc = internal_unmapPage(pg->fileOffset, pg->ppset);
        // page is no longer mapped, so clear ppset
        pg->ppset = 0;
        pg->mapped = PageDesc::CLEAR;
        // also clear FrameArray accessed bit
        // PageAllocatorKernPinned::clearAccessed(pg->paddr);
        return rc;
    }


    // unmap all pages.  used by forkCopy, in which case
    // we "know" that no mapping requests will happen
    // to undo things.
    virtual SysStatus locked_unmapAll();

    // update current pm when detaching from region using given pm
    virtual SysStatus updatePM(PMRef pmDetachingFrom);

    // set current pm - used when creating a fork parent, for example
    virtual SysStatus locked_setPM(PMRef newpm);

    // cleans up pagecache; needs lock and drops lock if page is busy
    enum {DO_BLOCK, DONT_BLOCK};
    virtual void locked_deallocPageList(uval block=DO_BLOCK);

    void locked_cacheSyncAll();

    void printRegionList();

    PMRef uninitPM() { return PMRef(TheBPRef); }
    uval  uninitPM(PMRef pm) { return pm == PMRef(TheBPRef); }

    FCMCommon();

    virtual SysStatus doDestroy();

    // all users are gone (except the FR if there is one)
    // should be called anywhere the use counts are seen to be zero
    // default is to destroy, but FCMs with FRs must tell the
    // FR instead
    virtual SysStatus notInUse() {
	tassert(isNotInUse() , err_printf("Caller of notInUse lies\n"));
	if(frRef) {
	    return DREF(frRef)->fcmNotInUse();
	} else {
	    return destroy();
	}
    }

    virtual SysStatus locked_removeReference();

    /*
     * called on a copy on write parent as well as a fork parent
     * for now we just reference count this users, but
     * in the future we will track them to implement copy on write,
     * where we must be able to call back to unmap frames.
     * the reference count was increased when we originally
     * made the copyonwrite FCM and it called FR::attachFCM
     */
    virtual SysStatusUval detachForkChild(
	FCMComputationRef child, uval lastChildCount) {
	return removeReference();
    }

    virtual SysStatusUval adoptChildAndLock(
	FCMComputationRef child, FRRef& parentFR,
	uval& count, uval& unmapped, uval& parentPages) {
	lock.acquire();
	return 0;			// refuse
    }

    virtual SysStatus locked_completeAdoption() {
	lock.release();
	return 0;
    }
    
    virtual SysStatus unLockPage(uval token);

    virtual void locked_unmapBase() {
	passertMsg(!mapBase, "mapBase set, locked_unmapBase not imp\n");}

    
public:
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    // detach strategy differs for each kind of FCM
    virtual SysStatus detachRegion(RegionRef regRef)=0;

    // when an fr/fcm pair is build externally, the fr must connect
    virtual SysStatus attachFR(FRRef frRefArg){
	tassert(frRef == NULL, err_printf("already attached\n"));
	frRef = frRefArg;
	return 0;
    }
    
    virtual SysStatus getFRRef(FRRef& frRefArg) {
	frRefArg = frRef; return 0;}

    virtual SysStatus destroy();

    // provide a frame mapping to the FCM
    virtual SysStatus establishPage(uval offset, uval vMapsRAddr, uval length);

    virtual SysStatus establishPagePhysical(uval offset, uval paddr,
					    uval length);

    // Unwire established pages - FCM may now page them and
    // return frames to PageManager
    virtual SysStatus disEstablishPage(uval offset, uval length);

    // Remove an established page.  Unlike disEstablish, in this case
    // the page is forgotten by the FCM.  The caller is responsible for
    // the allocation of the page.  The vMapsRAddr of the page is returned.
    virtual SysStatus removeEstablishedPage(
	uval offset, uval length, uval &vMapsRAddr);

    // If something other than a region needs to prevent FCM destruction
    // is must add a reference (count).  see ProcessVPList for example.
    virtual SysStatus addReference();

    virtual SysStatus removeReference();

    void notify(PageDesc* pg, SysStatus rc=0,
		PageFaultNotification* skipFn=0,
		uval keepLock = 0);

    // are there any users of this FCM
    virtual SysStatusUval isNotInUse() {
	return (referenceCount == 0 && regionList.isEmpty())?1:0; }

    // are there any frames
    virtual SysStatusUval isEmpty() {
	return pageList.getNumPages()==0?1:0; }
	    
    // check for transition to no frames
    // N.B. this MUST be called without the lock
    // unfortunately, we have no way to tassert on this
    virtual SysStatus checkEmpty() {
	if (isEmpty() && isNotInUse()) return notInUse();
	return 0;
    }
    
    // unmap and throw away all pages - even dirty ones!
    virtual SysStatus discardCachedPages();

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
	tassertMsg(!mapBase,
		   "when mapBase is true this method must be overridden\n");
	return noSharedSegments?0:1;
    }
	
    // used for debugging
    virtual SysStatus printStatus(uval kind);

    // just used to get statistics here
    virtual SysStatus getSummary(PM::Summary &sum);

    // reproduce an FCM for fork
    virtual SysStatus forkCloneFCM(
	FRRef& newFRRef, RegionType::Type regionType) {
	SysStatus rc;
	switch(regionType&~RegionType::KeepOnExec) {
	case RegionType::ForkCopy:
	    rc = forkCopy(newFRRef);
	    break;
	case RegionType::NewCRW:
	    rc = newCRW(newFRRef);
	    break;
	case RegionType::UseSame:
	    rc = getFRRef(newFRRef);
	    break;
	default:
	    tassertMsg(0, "Unknown RegionType value %lx\n",
		       uval(regionType));
	    rc = _SERROR(2708, 0, EINVAL);
	}
	return rc;
    }
};
#endif /* #ifndef __FCMCOMMON_H_ */
