#ifndef __HATKERNEL_H_
#define __HATKERNEL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HATKernel.H,v 1.27 2002/01/30 15:57:55 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic address translation code - machine dependent
 * **************************************************************************/

#include "mem/HAT.H"
#include "mem/HATDefault.H"

class HATKernel: public HATDefaultBase<AllocPinnedGlobalPadded> {
protected:
    // kernel special, this allocates the segment table
    // for the local virtual processor
    virtual SysStatus initSegmentHATs(void);

    virtual SysStatus createSegmentHATPrivate(SegmentHATRef& segmentHATRef);

public:
    static SysStatus ClassInit(VPNum vp);

    DEFINE_PINNEDGLOBALPADDED_NEW(HATKernel);

    virtual SysStatus unmapRange(uval rangeAddr, uval rangeSize, VPSet ppset);

    //FIXME only until we stop implicit mapping of normal kernel segments
    virtual SysStatus findSegment(uval virtAddr, SegmentHATRef &result,
				  VPNum vp, uval createIfNoSeg);

    // returns/creates a segment table for this vp
    virtual SysStatus getSegmentTable(VPNum vp, SegmentTable *&st);

    void switchToKernelAddressSpace();

};
#endif /* #ifndef __HATKERNEL_H_ */
