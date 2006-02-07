#ifndef __FCMLTRANSTABLE_H_
#define __FCMLTRANSTABEL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMLTransTable.H,v 1.3 2003/03/30 18:30:55 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A primative FCM that allocates a physical page
 * on each request.  Lock is in base class, as is list of pages.
 * **************************************************************************/
#include "PageSet.H"
#include "FCMPrimitive.H"

template <class ALLOC>
class FCMLTransTable : public FCMPrimitive<PageSet<ALLOC>,ALLOC> {
protected:
    uval defaultObject;

    void init(FRRef myFR, uval dObj);
    
    void initPage(uval virtAddr, uval vaddr);

public:
    static SysStatus Create(FCMRef &ref, FRRef myFR, uval dObj);

    virtual SysStatusUval mapPage(uval fileOffset,
				  uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);

    DEFINE_ALLOC_NEW(FCMLTransTable);
};

class FCMLTransTablePinned : public FCMLTransTable<AllocPinnedGlobal> {
public:
    static SysStatus Create(FCMRef &ref, FRRef myFR, uval dObj);
};
#endif /* #ifndef __FCMPRIMITIVE_H_ */
