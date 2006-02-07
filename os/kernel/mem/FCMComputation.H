#ifndef __FCMCOMPUTATION_H_
#define __FCMCOMPUTATION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMComputation.H,v 1.42 2004/03/18 19:15:26 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FCM for a computational (paged) region.
 *                     supports forkCopy
 *
 * **************************************************************************/

#include "mem/FCMDefault.H"
#include "mem/FRComputation.H"

class FCMComputation;
typedef FCMComputation** FCMComputationRef;

class FCMComputation : public FCMDefault {
    friend class FCMComputationShadow;
protected:
    /*
     * We use FCMComputationRef's here so that the fork implementation
     * is NOT part of the base FCM interface.
     */
    FCMComputationRef forkChild1, forkChild2, forkParent;
    // track number of pages which have been copied to either immediate
    // parent - used in agressive copy heuristics.
    // approximate - we don't mess up base classes to get edge cases
    // right since it can't matter
    uval copies1, copies2, lastChildUnmappedCount;
    // with plenty of room, 8 bit flags are more efficient than 1
    uval hasCollapsed:8;		// true when collapse is done
    uval recursiveDestroy:8;		// destroy of copy base
    uval lastForkUsedCopy:8;		// for statistics only

public:
// if number unused last time less than copy/factor copy indicated
    static uval FCMComputation::CopyOnForkFactor;
// if less than Always pages, copy
    static uval FCMComputation::CopyOnForkAlways;

protected:
#if 1
    // See comment for this method in FCMDefault.H
    virtual FCMRef rootCreate() {
	//err_printf("FCMComputationRoot::Create() called.\n");
	return (FCMRef)CObjRootSingleRep::Create(this);
    }
#endif /* #if 1 */

    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    // must deal hear with mapBase
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    virtual SysStatusUval getPageInternal(
	uval fileOffset, PageFaultNotification *fn, PageDesc*& pgarg,
	uval copyOnWrite);

    virtual SysStatusUval getForkPage(
	PageDesc* callerPg, uval& returnUval,
	FCMComputationRef& childRef,
	PageFaultNotification *fn,
	uval copyOnWrite);

    SysStatusUval initiateGetParentPage(
	uval fileOffset, PageDesc* pg, PageFaultNotification *fn,
	uval copyOnWrite);

    uval queueFnOrBlock(PageFaultNotification *fn, PageDesc* pg);

    virtual SysStatus doDestroy();

    virtual SysStatus locked_givePage(PageDesc* pg, uval blockID);

    virtual SysStatus locked_dropPage(uval fileOffset);

    virtual SysStatus givePageCopy(uval fileOffset, uval len, uval vaddr);

    virtual SysStatusUval adoptChildAndLock(
	FCMComputationRef child, FRRef& parentFR,
	uval& parentCopies, uval& childUnmapped, uval& parentPages);

    virtual SysStatus adoptChildrenAndLock(
	FCMComputationRef child1, FCMComputationRef child2);

    virtual SysStatus locked_completeAdoption();

    // this FCM is child of caller
    // N.B. does not acquire lock - may happen while we hold the
    // lock for other reasons.
    virtual SysStatus setParent(FCMComputationRef fcmRef) {
	forkParent = fcmRef;
	return 0;
    }

    virtual SysStatus getParent(FCMComputationRef& fcmRef) {
	fcmRef = forkParent;
	return 0;
    }

    virtual SysStatus setChildrenAndLock(
	FCMComputationRef c1, FCMComputationRef c2) {
	lock.acquire();
	forkChild1 = c1;
	forkChild2 = c2;
	referenceCount+=2;
	//N.B. leave locked
	return 0;
    }


    virtual SysStatus locked_replaceChild(
	FCMComputationRef oldChild, FCMComputationRef newChild) {
	_ASSERT_HELD(lock);
	if (forkChild1 == oldChild) {
	    forkChild1 = newChild;
	} else {
	    tassert(forkChild2==oldChild,
		    err_printf("non child trying to modify parent\n"));
	    forkChild2 = newChild;
	}
	return 0;
    }

    virtual SysStatusUval locked_detachFromParent(FCMComputationRef& child);
    
    virtual SysStatusUval detachForkChild(
	FCMComputationRef child, uval count);

    virtual SysStatusUval completeDetachForkChild(FCMComputationRef& child);
    
    virtual SysStatusUval removeReferenceNoRecursion(
	FCMComputationRef& parentRef);
    
    virtual void locked_unmapBase();

    virtual void blockOnIO(PageDesc*& pg);

public:
    static SysStatus Create(
	FCMRef &ref, FRRef cr, uval pageSizeArg, uval pageable);
    static SysStatus CreateFixedNumaNode(
	FCMRef &ref, FRRef cr, uval pageSizeArg, uval pageable, uval numanode);
    // this version used to create copy on reference copy
    static SysStatus Create(
	FCMRef &ref, FRRef cr, FRRef baseFR, uval pageSizeArg, uval pageable);

    FCMComputation() {
	forkChild1 = forkChild2 = 0;
	forkParent = 0;
	copies1 = copies2 = 0;
	lastChildUnmappedCount = 0;
	hasCollapsed = 0;
	recursiveDestroy = 0;
	lastForkUsedCopy = 0;
	noSharedSegments = 1;
	isCRW = 0;
	mapBase = 0;
    }

    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    virtual SysStatus detachRegion(RegionRef regRef);

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
	if(noSharedSegments == 0) {
	    // this fcm might make shared segments
	    return 1;
	} else if(mapBase) {
	    // we dont but our base might
	    return DREF(forkParent)->sharedSegments();
	} else {
	    return 0;
	}
    }
	
    // fork copy request from region
    virtual SysStatus forkCopy(FRRef& newChildFRRef);

    // make a new CRW using same base as current
    virtual SysStatus newCRW(FRRef& newFRRef);

    // collapse yourself into your parent
    virtual SysStatus forkCollapse();

    // for use by FR only
    virtual SysStatus newBlockID(uval fileOffset, uval blockID);

    DEFINE_GLOBALPADDED_NEW(FCMComputation);

    DEFINE_REFS(FCMComputation);

    // we override these methods to make mapBase work
    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);
};

#endif /* #ifndef __FCMCOMPUTATION_H_ */
