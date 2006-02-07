#ifndef __FCMSHARED_TRIVIAL_H_
#define __FCMSHARED_TRIVIAL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMSharedTrivial.H,v 1.18 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/
#include "mem/FCM.H"

class TestSwitch;

template <class PL, class ALLOC>
class FCMSharedTrivial : public FCM {
    friend class TestSwitch;
    typedef BLock LockType;
    LockType      lock;			// FIXME: lock protects PageList

    RegionRef reg;
    PMRef     pm;
    FRRef     frRef;
    PL        pageList;

protected:
    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn) {return -1;}

    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0) 
    {
        tassertMsg(dirty==0, "NYI");
        return(-1);
    }

    void locked_deallocPageList() // FIXME: kludge
    {
	PageDesc *p;
	uval      virtForm;

	_ASSERT_HELD(lock);

	while ((p = pageList.getFirst()) != NULL) {
	    // indicate that we are given up ownership of pages
	    // PageAllocatorKernPinned::clearFrameDesc(p->paddr);

	    virtForm = PageAllocatorKernPinned::realToVirt(p->paddr);
	    DREF(pm)->deallocPages(getRef(), virtForm, p->len);

	    pageList.remove(p->fileOffset);
	}
    }

public:
    FCMSharedTrivial() { frRef = NULL; }

    static SysStatus Create(FCMRef &ref);

    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);

    DEFINE_ALLOC_NEW(FCMSharedTrivial);

    // attach to the FCM to map (possibly) part of it
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    virtual SysStatus detachRegion(RegionRef regRef) {
	// FIXME: kludge to free up memory for dcofcm test
	lock.acquire();
	locked_deallocPageList();
	lock.release();
	return -1;
    }

    virtual SysStatus attachFR(FRRef frRefArg){
	tassert(frRef == NULL, err_printf("already attached\n"));
	frRef = frRefArg;
	return 0;
    }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	frRefArg = frRef; return 0;}

    virtual SysStatus destroy() {return(-1);}

    virtual SysStatusUval isNotInUse() {return(0);}

    virtual SysStatusUval isEmpty() { return 0; }

    virtual SysStatus discardCachedPages() {
	passertMsg(0, "NYI\n");
	return 0;
    }

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
	return 0;
    }

    // used for debugging
    virtual SysStatus printStatus(uval kind);
};

#endif /* #ifndef __FCMSHARED_TRIVIAL_H_ */
