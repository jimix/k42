#ifndef __SEGMENT_HAT_H_
#define __SEGMENT_HAT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentHAT.H,v 1.24 2004/01/23 19:50:31 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * class which defines the hardware dependent paging machanisms
 * A segmentHAT represents one segment of the physical address space
 * **************************************************************************/
#include "mem/Access.H"
#include "mem/HAT.H"
#include "mem/SegmentTable.H"
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>

class SegmentHAT : public Obj {
public:
    virtual SysStatus mapSegment(SegmentTable* segp, uval virtAddr, uval len,
				 VPNum vp) = 0;

    virtual SysStatus mapPage(uval physAddr, uval virtAddr, uval len,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp,
			      uval wasMapped) = 0;

    virtual SysStatus detachHAT(HATRef hatRef, uval virtAddr, VPNum) = 0;

    virtual SysStatus unmapRange(
	HATRef hatRef, uval segmentAddr, uval segmentEnd,
	uval regionAddr, uval regionEnd, VPNum vp) = 0;

    virtual SysStatus unmapPage(HATRef hatRef, uval virtAddr) = 0;

    virtual SysStatus unmapSharedPage(HATRef hatRef, uval virtAddr) = 0;
    
    virtual SysStatus changePP(VPNum vp) = 0;

    virtual SysStatus destroy() = 0;

    // used in debugging
    virtual SysStatus privateSegment() = 0;
};

#endif /* #ifndef __SEGMENT_HAT_H_ */
