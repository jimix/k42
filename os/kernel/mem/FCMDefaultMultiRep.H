#ifndef __FCMDEFAULT_MULTI_REP_H_
#define __FCMDEFAULT_MULTI_REP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefaultMultiRep.H,v 1.17 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic FCM for mapping a file representative.
 * **************************************************************************/
#include "FCMCommonMultiRep.H"
#include "FCMDefaultMultiRepRoot.H"
#include "defines/experimental.H"

class FCMDefaultMultiRep :
    public FCMCommonMultiRep<CObjRootMultiRep,
        AllocGlobalPadded,AllocLocalStrict> {

    FCMDefaultMultiRepRoot *root() {
	return (FCMDefaultMultiRepRoot *)myRoot;
    }

    friend class FCMDefaultMultiRepRoot;

    virtual SysStatusUval getForkPage(
	PageDesc* callerPg, uval& returnUval,
	FCMComputationRef& childRef,
	PageFaultNotification *fn,
	uval copyOnWrite);
protected:
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    // unlocked previously gotten page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0);

    LocalPageDescData *findOrAllocatePageAndLock(uval fileOffset,
						 SysStatus &rc,
						 uval *paddr);

    SysStatusUval getPageInternal(uval fileOffset, PageFaultNotification *fn,
				  LocalPageDescData **ld);

    SysStatus
    FCMDefaultMultiRep::getFrame(LocalPageDescData *ld);

    SysStatusUval
    startFillPage(uval offset, uval paddr, PageFaultNotification* fn,
		  LocalPageDescData *ld);

public:

    static SysStatus Create(FCMRef &ref, FRRef cr, uval pageable);

    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);

    virtual SysStatus ioComplete(uval offset, SysStatus rc);

    virtual SysStatus detachRegion(RegionRef regRef) {
	return COGLOBAL(detachRegion(regRef));
    }

    // force all modified pages back to FR
    virtual SysStatus fsync(uval force) {
	return COGLOBAL(fsync(force));
    }

    // override because behavior when zero is different - lazy close
    virtual SysStatus removeReference() {
    	passert(0, err_printf("removeReference()\n"));
	return COGLOBAL(removeReference());
    }

    virtual SysStatus giveBack(PM::MemLevelState) { 
	return _SRETUVAL(PageAllocatorKernPinned::PINNED); 
    }

    virtual SysStatus getSummary(PM::Summary &sum) { 
	// FIXME: should be able to return summary
	sum.reset();
	return _SRETUVAL(PageAllocatorKernPinned::PINNED); 
    }

    DEFINE_LOCALSTRICT_NEW(FCMDefaultMultiRep);
};

#endif /* #ifndef __FCMDEFAULT_MULTI_REP_H_ */
