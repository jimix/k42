#ifndef __HAT_H_
#define __HAT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HAT.H,v 1.30 2004/01/23 19:50:30 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Miscellaneous place for early address transation
 * stuff.
 * **************************************************************************/

#include <mem/Access.H>
#include <mem/SegmentTable.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>

class SegmentTable;

class HAT : public Obj {
public:
    DEFINE_REFS(HAT);

    enum { NOSEG=-1 };

    virtual SysStatus mapPage(uval physAddr, uval virtAddr, uval len,
			      AccessMode::pageFaultInfo pfinfo,
			      AccessMode::mode access, VPNum vp,
			      uval wasMapped,
			      uval createIfNoSeg=1) = 0;

    virtual SysStatus attachRegion(uval regionAddr, uval regionSize,
				   SegmentHATRef segmentHATRef ) = 0;

    // can only be called when no more accesses are possible to range, set
    // is processors that have accessed range
    virtual SysStatus unmapRange(uval rangeAddr, uval rangeSize, VPSet set)=0;

    // returns/creates a segment table for this vp
    virtual SysStatus getSegmentTable(VPNum vp, SegmentTable *&st) = 0;

    virtual SysStatus attachProcess(ProcessRef processRef) = 0;

    // returns process this hat is attached to
    virtual SysStatus getProcess(ProcessRef &processRef) = 0;

    // remove a mapping on this physical processor
    virtual SysStatus unmapPage(uval vaddr) = 0;

    // remove all mappings and other pp specific info for a vp
    // preliminary to vp migration on old pp
    virtual SysStatus detachVP(VPNum vp) = 0;
    // does post migration work on new pp
    virtual SysStatus attachVP(VPNum vp) = 0;

    virtual SysStatus destroy() = 0;

    // used in debugging - returns true iff vaddr is backed by
    // a private segment
    virtual SysStatus privateSegment(uval vaddr) = 0;
};

#endif /* #ifndef __HAT_H_ */
