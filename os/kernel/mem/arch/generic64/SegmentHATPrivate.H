#ifndef __SEGMENT_HATPRIVATE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __SEGMENT_HATPRIVATE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentHATPrivate.H,v 1.5 2003/01/13 22:36:57 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

/* FORWARD */ class SegmentHATPrivate;
/* FORWARD */ class SegmentHATKernel;

#include "mem/SegmentHAT.H"
#include <mem/Access.H>

class SegmentHATPrivate : public SegmentHAT {
protected:
  uval  vpCount;
public:
    static SysStatus Create(SegmentHATRef& ref);
    // for debugging
    virtual SysStatus privateSegment() { return 1;};
    // unmapSharedPage is unmapPage in private segment
};


/*
 * Derived from SegmentHATPrivate for making pinned kernel
 * SegmentHAT's. These objects look like clustered objects but
 * can't be in the (pageable) object translation table - so we
 * make private refs.  */

class SegmentHATKernel : public SegmentHATPrivate {
protected:
    // FIXME: guaranteed always freed on local processor?
    DEFINE_PINNEDLOCALSTRICT_NEW(SegmentHATKernel);

public:
    static SysStatus Create(SegmentHATRef& ref);

    // during kernel init, same page tables already exist, and need
    // segment hats retrofitted to them.  This call completes that
    virtual SysStatus initSegmentHAT(uval virtAddr, uval l2pt, VPNum vp);
};


// Derived SegmentHAT for shared FCMs; shared across multiple apps
class SegmentHATShared : public SegmentHATPrivate {
protected:
    DEFINE_GLOBALPADDED_NEW(SegmentHATShared);

public:
    // UnMapShared true means shared segments need to be unmapped
    // from each region to fix up tlb.  Not true on all architectures.
    enum {UnMapShared = 0};

    static SysStatus Create(SegmentHATRef& ref);

    // not supported for shared segments
    virtual SysStatus unmapRange(HATRef hatRef,
				 uval segmentAddr, uval segmentEnd,
				 uval regionAddr, uval regionEnd,
				 VPNum vp );

    virtual SysStatus unmapPage(HATRef hatRef, uval virtAddr);

    virtual SysStatus unmapSharedPage(HATRef hatRef, uval virtAddr);

    // we provide new detach/destroy methods to deal with sharing issues
    virtual SysStatus detachHAT(HATRef hatRef, uval virtAddr, VPNum vp);

    // if everything is right, this should never be called, but we provide
    // dummy implementation anyway so we can trap any errors
    virtual SysStatus destroy();

    // owner of segmentHAT, fcm, uses this instead of other destroy, so that
    // accidental destroys will be caught
    virtual SysStatus sharedSegDestroy();
    // for debugging
    virtual SysStatus privateSegment() { return 0;};
};

