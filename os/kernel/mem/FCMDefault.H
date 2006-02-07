#ifndef __FCMDEFAULT_H_
#define __FCMDEFAULT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefault.H,v 1.76 2004/10/29 16:30:31 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FCM class common to computation and mapping a
 * file representative. 
 **************************************************************************/

#include <scheduler/Scheduler.H>
#include "mem/FCMCommon.H"
#include "mem/PageSet.H"
#include "mem/FCMDefaultRoot.H"
#include <defines/paging.H>

class TestSwitch;
class IORestartRequests;

class FCMDefault : public FCMCommon<PageSet<AllocGlobal>, AllocGlobal> {
    friend class FCMDefaultRoot;
    friend class TestSwitch;
    friend class FCMDefaultMultiRepRoot; // direct data transfer

    // Since the root controls dynamic switch negotiation, we don't want
    // the root of FCMDefault to be used by it subclasses. But how can
    // control that if its subclass (eg FCMComputation) calls Default's init
    // method?  Maybe we just shouldn't share the (root) object creation
    // code, but for now I use a virtual method rootCreate, which is called
    // by init(). FCMDefault's subclass MUST override them to create the
    // appropriate root. (Maybe we can do something like private virtual
    // to enforce this but I have to check.)
    virtual FCMRef rootCreate() {
	//err_printf("FCMDefaultRoot::Create() called.\n");
#if 1
	return (FCMRef)FCMDefaultRoot::Create(this);
#else /* #if 1 */
	return (FCMRef)CObjRootSingleRep::Create(this);
#endif /* #if 1 */
    }

protected:
    // data strcture queued in I/O system for flow control
    IORestartRequests *rq;		
    PageDesc *pendingWrite;		// pending, now just for write
    virtual SysStatusUval getForkPage(
	PageDesc* callerPg, uval& returnUval,
	FCMComputationRef& childRef,
	PageFaultNotification *fn,
	uval copyOnWrite);

    uval  numanode;			// where to allocate frames from
    uval  pinnedCount;			// number of pinned frames
#ifdef ENABLE_FCM_SWITCHING
    uval  performedSwitch;
#endif

    // flags are for page allocator
    PageDesc *findOrAllocatePageAndLock(uval fileOffset, SysStatus &rc,
					uval &paddr, uval flags=0);

    SysStatus getFrame(uval& vaddr, uval flags=0);

    SysStatus init(FCMRef &ref, FRRef frRefArg, uval pageSizeArg,
		   uval pageable, uval backedBySwap, uval preFetchPages,
                   uval maxPages);

    /// called by FCMCRW, returns a locked page if it exists
    /// if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    /// unlocked previously gotten page
    virtual SysStatus releasePage(uval fileOffset,uval dirty=0);


    uval preFetch;			// number of pages to prefetch
    void doPreFetch(uval fileOffset);

    uval nextOffset;			// last file offset scanned
    // scanning for pages to free
    void locked_pageScan(PM::MemLevelState memLevelState); 
    SysStatusUval writeFreeFreeList(uval numPages);
    SysStatusUval locked_giveBack(uval numPages);

    enum {GIVE_BACK_NUMPAGES=16};
    uval maxNumPages, giveBackNumPages;

    // the constructor
    FCMDefault() { nextOffset = uval(-1); }


    // we can't free timer until there is no chance that the last
    // timer service thread is around - so wait for COBJ cleanup
    virtual SysStatus cleanup() {
	return FCMCommon<PageSet<AllocGlobal>, AllocGlobal>::cleanup();
    }


    /* get or create a page descriptor and frame for the file offset
     * if this can't be done immediately and an fn is provided, return
     * the notification key having queued the fn on the page
     * descriptor and started the IO.
     *
     * Called unlocked.  Returns with the page locked if the page is available
     */

    virtual SysStatusUval getPageInternal(
	uval fileOffset, PageFaultNotification *fn, PageDesc*& pgarg,
	uval copyOnWrite);

    SysStatusUval startFillPage(
	uval offset, PageFaultNotification* fn, PageDesc* pg);

    // internal form - called with lock held
    virtual SysStatus ioCompleteInternal(
	PageDesc* pg, SysStatus rc, PageFaultNotification* skipFn=0,
	uval keepLock = 0);

public:
    FCMDefault(uval preFetchPages, uval maxPages)
        : preFetch(preFetchPages), maxNumPages(maxPages)
    { 
	giveBackNumPages = MAX(maxNumPages/4, GIVE_BACK_NUMPAGES);
        nextOffset = uval(-1);
    }
    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);


    virtual SysStatus ioComplete(uval offset, SysStatus rc);

    virtual SysStatus detachRegion(RegionRef regRef);

    virtual SysStatus FCMDefault::resumeIO();

    virtual SysStatus giveBack(PM::MemLevelState);

    virtual SysStatus getSummary(PM::Summary &sum);

    DEFINE_GLOBALPADDED_NEW(FCMDefault);
};

#endif /* #ifndef __FCMDEFAULT_H_ */
