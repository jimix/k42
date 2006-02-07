#ifndef __FCMFIXED_H_
#define __FCMFIXED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMFixed.H,v 1.24 2003/12/31 20:03:50 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: An FCM which remembers mappings established
 * externally.  All this mappings are fixed - no unmaps are ever
 * initiated internally by this FCM, although an external request can
 * remove a page
 * **************************************************************************/

#include "mem/FCMCommon.H"
#include "mem/PageSet.H"

template<class ALLOC>
class FCMFixed : public FCMCommon<PageSet<ALLOC>,ALLOC> {
    typedef typename FCMCommon<PageSet<ALLOC>,ALLOC>::LockType LockType;
    typedef typename FCMCommon<PageSet<ALLOC>,ALLOC>::RegionInfo RegionInfo;
protected:
    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0);

public:
    // provide a frame mapping to the FCM
    virtual SysStatus establishPage(uval offset, uval virtAddr, uval length);

    virtual SysStatus establishPagePhysical(uval offset, uval paddr,
					    uval length);

    // unconditionally unmaps and removes a single page previously established,
    // returns error if paddr doesn't match
    virtual SysStatus removeEstablishedPage(uval offset, uval paddr);

    static SysStatus Create(FCMRef &ref);

    virtual SysStatusUval mapPage(uval fileOffset,
				  uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);

    virtual SysStatus detachRegion(RegionRef regRef);

    DEFINE_ALLOC_NEW(FCMFixed);
};

#endif /* #ifndef __FCMFIXED_H_ */
