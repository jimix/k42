#ifndef __FCMPRIMITIVE_H_
#define __FCMPRIMITIVE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitive.H,v 1.33 2003/12/31 20:03:50 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A primative FCM that allocates a physical page
 * on each request.  Lock is in base class, as is list of pages.
 * **************************************************************************/

#include "mem/FCMCommon.H"

template <class PL, class ALLOC>
class FCMPrimitive : public FCMCommon<PL, ALLOC> {
    typedef typename FCMCommon<PL,ALLOC>::RegionInfo RegionInfo;
protected:
    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0);

public:
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

    DEFINE_GLOBALPADDED_NEW(FCMPrimitive);
};

#endif /* #ifndef __FCMPRIMITIVE_H_ */
