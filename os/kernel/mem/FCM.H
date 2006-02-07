#ifndef __FCM_H_
#define __FCM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCM.H,v 1.77 2004/11/08 22:22:49 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The base class from wich all File Chunk
 * Managers (FCM)s are derived.
 * **************************************************************************/

#ifndef _IN_STUBGEN
#include "mem/HAT.H"
#include "mem/FR.H"
#include "mem/PM.H"
// we include the following for now to get PageConstraints
#include "mem/PageAllocatorKernPinned.H"
#endif /* #ifndef _IN_STUBGEN  */

class PageFaultNotification;
class FCMComputation;
typedef FCMComputation** FCMComputationRef;
class PageDesc;

class FCM : public Obj {
protected:
#ifndef _IN_STUBGEN  // FIXME: stubcompiler problem
    friend class FCMCRW;
    friend class FCMMIP;
    friend class PinnedMapping;
    friend class Process;
    // FCMComputation methods declared here so the top
    // parent can be any FCM - supports copy on write
    friend class FCMComputation;
#endif /* #ifndef _IN_STUBGEN  // FIXME: ... */
    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn)=0;
    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0)=0;

    /*
     * This function exists because getFCM is protected
     * and FCM is a friend.  But the way C++ works
     * that doesn't make derived classes friends.
     * So we do this to make getFCM available to all the classes
     * derived from FCM
     * This function is protected in FCM to limit its use.
     */
#ifndef _IN_STUBGEN
    SysStatus getFCM(FRRef frRef, FCMRef& fcmRef) {
	return DREF(frRef)->getFCM(fcmRef);
    }
#else
    SysStatus getFCM(FRRef frRef, FCMRef& fcmRef);
#endif

    // see comments in FCMComputation.C
    enum  {DOINGIO, FRAMETOCOPY, FRAMETOKEEP, BLOCKIDTOKEEP, MAPPAGE, NOPAGE,
	   TRYPARENT, COMPLETE, RETRY};
    virtual SysStatusUval adoptChildAndLock(
	FCMComputationRef child, FRRef& parentFR,
	uval& count, uval& unmapped, uval& parentPages) {
	return 0;			// refuse
    }

    virtual SysStatus adoptChildrenAndLock(
	FCMComputationRef child1, FCMComputationRef child2) {
	return 0;			// refuse
    }

    virtual SysStatus locked_completeAdoption() {
	return 0;
    }

    virtual SysStatus locked_replaceChild(
	FCMComputationRef oldChild, FCMComputationRef newChild) {
	/*
	 * any FCM which tracks its copy children must
	 * override this to update its records
	 */
	return 0;
    }

    virtual SysStatus locked_givePage(PageDesc* pg, uval blockID) {
#ifndef _IN_STUBGEN
	passertMsg(0, "should never be called\n");
#endif
	return 0;
    }

    virtual SysStatus locked_dropPage(uval fileOffset) {
#ifndef _IN_STUBGEN
	passertMsg(0, "should never be called\n");
#endif
	return 0;
    }

    virtual SysStatusUval getForkPage(
	PageDesc* callerPg, uval& returnUval,
	FCMComputationRef& childRef,
	PageFaultNotification *fn,
	uval copyOnWrite) {
	return NOPAGE;
    }

    virtual SysStatus unLockPage(uval token) {
	return 0;
    }

    virtual SysStatusUval detachForkChild(
	FCMComputationRef child, uval lastChildCount) {
	/*
	 * if children are being tracked, or referenced counted
	 * this must be overridden to account for loss of a child
	 */
	return 0;
    }

public:
    DEFINE_REFS(FCM);

    /**
     * combine finding frame and calling hat to map it in FCM
     * so whole thing can happen under FCM lock.  This eliminates
     * the problem of a race between an unmap request and a map request
     *
     * this interface passes the bounds of the "region".  In fact
     * the caller is free to restrict this to any subregion which
     * contains the needed vaddr.  The reason for passing more is
     * to allow the FCM to map extra pages if this appears to be
     * a good policy.
     *
     * fileOffset - offset in "file" of "region"
     * regionVaddr - address of start of "region"
     * regionSize - size of "region"
     * pfinfo - cause of page fault
     * vaddr - virtual address at which to map the requested page
     * mode - access mode at which to map this page
     * hat - HAT of the process that contains this region
     * vp - virtual processor of the owning process that needs the page
     * reg - region making the call
     * firstAccessOnPP - flag set indicating if first access of region on pp
     * fn - notification class if not in-core to do asynchrounous
     *      notification of completion
     *
     * return value:  < 0 error, == 0 in-core, > 0 async
     */
    virtual SysStatusUval mapPage(uval fileOffset,
				  uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr,
				  AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn)=0;

    virtual SysStatus ioComplete(uval offset, SysStatus rc);

    // attach to the FCM to map (possibly) part of it
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode) = 0;

    virtual SysStatus detachRegion(RegionRef regRef)=0;

    // when an fr/fcm pair is build externally, the fr must connect
    virtual SysStatus attachFR(FRRef frRef);

    // sometimes need to find the FR of an FCM
    virtual SysStatus getFRRef(FRRef& frRefArg) = 0;

    // This should only be called by the owning FR class (not easy to enforce)
    virtual SysStatus destroy()=0;

    // provide a frame mapping to the FCM
    virtual SysStatus establishPage(uval offset, uval vMapsRAddr, uval length)
    { return _SERROR(1273, 0, EINVAL);}

    virtual SysStatus establishPagePhysical(uval offset, uval paddr,
					    uval length)
    { return _SERROR(1274, 0, EINVAL);}

    // Unwire established pages - FCM may now page them and
    // return frames to PageManager
    virtual SysStatus disEstablishPage(uval offset, uval length)
    { return _SERROR(1313, 0, EINVAL);}

    // Remove an established page.  Unlike disEstablish, in this case
    // the page is forgotten by the FCM.  The caller is responsible for
    // the allocation of the page.  The vMapsRAddr of the page is returned.
    virtual SysStatus removeEstablishedPage(
	uval offset, uval length, uval &vMapsRAddr)
    { return _SERROR(1332, 0, EINVAL);}

    // If something other than a region needs to prevent FCM destruction
    // is must add a reference (count).  see ProcessVPList for example.
    virtual SysStatus addReference()
    { return _SERROR(1314, 0, EINVAL);}

    virtual SysStatus removeReference()
    { return _SERROR(1315, 0, EINVAL);}

    // if force writing of all modified pages, else
    // just periodic unmap and next time write out modified free
    // common implementation of fsync is to do nothing!
    virtual SysStatus fsync(uval force) { return 0;};

    // return 1 if FCM has no users
    // N.B. FCM may still have frames cached
    virtual SysStatusUval isNotInUse()=0;

    // return 1 if FCM has not frames cached
    // N.B. FCM may still have users
    virtual SysStatusUval isEmpty()=0;

    // resume any pending I/O, call back when some resource freed
    virtual SysStatus resumeIO();

    // request from PM, returns in numPages num given back
    virtual SysStatus giveBack(PM::MemLevelState) { 
	return _SRETUVAL(PageAllocatorKernPinned::PINNED); 
    }

    // FIXME: should be able to return some status for everything
    // MUST NOT ACQUIRE LOCK, called for printing information, 
    // deadlock condition if acquire lock
    virtual SysStatus getSummary(PM::Summary &sum) {
	sum.reset();
	return _SRETUVAL(PageAllocatorKernPinned::PINNED); 
    }

    virtual SysStatus forkCloneFCM(
	FRRef& newFRRef, RegionType::Type regionType) {
	tassertMsg(0, "This FCM does not support forkClone\n");
	return _SERROR(2711, 0, EINVAL);
    }
	
    // fork copy request from region
    virtual SysStatus forkCopy(FRRef& newChildFRRef) {
	tassertMsg(0, "fork interface called on non-computational FCM\n");
	return _SERROR(1347, 0, EINVAL);
    }

    // make a new CRW using same base as current
    virtual SysStatus newCRW(FRRef& newFRRef) {
	tassertMsg(0, "newCRW interface called on non-computational FCM\n");
	return _SERROR(2709, 0, EINVAL);
    }
	
    // unmap and throw away all pages - even dirty ones!
    virtual SysStatus discardCachedPages()=0;


    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments()=0;
	
    enum {PendingFaults, MemoryUse};

    virtual SysStatus printStatus(uval kind)=0;

    static SysStatus PrintStatus(uval kind);
};

#endif /* #ifndef __FCM_H_ */
