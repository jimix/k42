#ifndef __FCMCOMMON_MULTI_REP_ROOT_H_
#define __FCMCOMMON_MULTI_REP_ROOT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCommonMultiRepRoot.H,v 1.17 2004/10/20 18:10:28 okrieg Exp $
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
#include <cobj/CObjRootMultiRep.H>
#include "mem/PageDesc.H"
#include "mem/FCM.H"
#include "mem/SegmentHAT.H"
#include <misc/DHashTable.H>
#include "mem/PageDescData.H"

template <class RBASE, class RGALLOC, class RLALLOC> class FCMCommonMultiRep;

template <class BASE, class ALLOC,class LALLOC>
class FCMCommonMultiRepRoot : public BASE {
    friend class FCMCommonMultiRep<BASE,ALLOC,LALLOC>;
public:
#if 0
    typedef MasterDHashTable<MasterPageDescData,LocalPageDescData,
	             AllocGlobal,AllocLocalStrict> MasterHashTable;
#else
    typedef MasterDHashTable<MasterPageDescData,LocalPageDescData,
	             ALLOC,LALLOC> MasterHashTable;
#endif
    MasterHashTable masterDHashTable;
protected:

    typedef BLock LockType;
    LockType      lock;			// FIXME: lock protects PageList
    FRRef         frRef;		// fr this fcm services
    uval          pageSize;		// default page size
    
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


    uval  pageable:1;			// does fr support paging?
    uval  backedBySwap:1;		// alg'thm slightly diff if swapped
    uval  priv:1;			// belongs (primarily) to one process
    uval  beingDestroyed:1;		// indicates if being destroyed
    uval  mappedExecutable:1;		// its been mapped executable
    uval  noSharedSegments:1;		// no shared segments may be used

protected:
    PMRef         pmRef;		// current PM for page alloc/dealloc

    virtual SysStatus noSegMapPageInHAT(
	uval physAddr, uval virtAddr, uval len,
	AccessMode::pageFaultInfo pfinfo,
	AccessMode::mode access, VPNum vp, HATRef hat, RegionRef reg,
	uval offset, uval acckey);

    void doSetCacheSynced(LocalPageDescData *pg);
    void doSetDirty(LocalPageDescData *pg);
    void doSetFreeAfterIO(MasterPageDescData *md);

    PMRef uninitPM() { return PMRef(TheBPRef); }
    uval  uninitPM(PMRef pm) { return pm == PMRef(TheBPRef); }

    virtual SysStatus doDestroy();

    // update current pm when detaching from region using given pm
    virtual SysStatus updatePM(PMRef pmDetachingFrom);

    // called with MasterPageDescData, lock held
    // if the page is doingIO the thread is blocked and will be woken
    // by some associated notify on the page.
    // This method drops md's lock before blocking.  There is no
    // assurance of the state of md when the thread awakes.
    void blockOnIO(MasterPageDescData*& md);

    // Remove mappings only for local processor; called as a remote op
    virtual SysStatus unmapPageLocal(uval fileOffset);

    struct UnmapPageMsg;
    static SysStatus UnmapPageMsgHandler(uval msgUval);

    // remove all mapping of this page
    // N.B. unless the page is locked or there are no regions
    // attached this operation is futile
    virtual SysStatus unmapPage(MasterPageDescData *md);

    // unmap all pages.  used by forkCopy, in which case
    // we "know" that no mapping requests will happen
    // to undo things.
    virtual SysStatus locked_unmapAll();
    void commonInit();
    FCMCommonMultiRepRoot(uval numPages = 1);
    FCMCommonMultiRepRoot(uval numPages, uval minSize);

    virtual ~FCMCommonMultiRepRoot() { masterDHashTable.cleanup(); }
    // all users are gone (except the FR is there is one)
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

    // cleans up pagecache; needs lock and drops lock if page is busy
    enum {DO_BLOCK, DONT_BLOCK};
    virtual void locked_deallocPageList(uval block=DO_BLOCK);

public:
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    virtual SysStatus addReference();

    virtual SysStatus removeReference();

    // are ther any users
    virtual SysStatusUval isNotInUse() {
        return (referenceCount == 0 && regionList.isEmpty())?1:0; }

    // are there any frames
    virtual SysStatusUval isEmpty() {
	return masterDHashTable.getNumNonEmpty()==0?1:0; 
    }

    // check for transition to no frames
    // N.B. this MUST be called without the lock
    // unfortunately, we have no way to tassert on this
    virtual SysStatus checkEmpty() {
	if (isEmpty() && isNotInUse()) return notInUse();
	return 0;
    }

#if 0	    
    // detach strategy differs for each kind of FCM
    virtual SysStatus detachRegion(RegionRef regRef)=0;
#endif

    virtual SysStatus destroy();

    // provide a frame mapping to the FCM
    virtual SysStatus establishPage(uval offset, uval vMapsRAddr, uval length);

    // Remove an established page.  Unlike disEstablish, in this case
    // the page is forgotten by the FCM.  The caller is responsible for
    // the allocation of the page.  The vMapsRAddr of the page is returned.
    virtual SysStatus removeEstablishedPage(
	uval offset, uval length, uval &vMapsRAddr);

    // unmap and throw away all pages - even dirty ones!
    virtual SysStatus discardCachedPages();

};

#endif /* #ifndef __FCMCOMMON_MULTI_REP_ROOT_H_ */
