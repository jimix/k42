#ifndef __SEGMENT_TABLE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __SEGMENT_TABLE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentTable.H,v 1.2 2001/10/05 21:50:09 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * machine dependent object to manage a segment.  This is NOT
 * a clustered object - it is managed by the process HAT
 * **************************************************************************/

/* FORWARD */ class SegmentTable;

#include "exception/ExceptionLocal.H"


class HATKernel;

class SegmentTable {

public:

    void mapSegment(uval L2PTPhysAddr, uval virtAddr) {
    }

    void unmapSegment(uval virtAddr) {
    }

    // When a reference is made to a kernel address while running
    // with an non-kernel segment table
    // we check to see if the kernel had that segment mapped, and if so
    // copy the mapping into the user segment table
    // This is done at exception level
    // This can happen only if the kernel is running with a "borrowed"
    // user segment table
    //
    uval checkKernelSegmentFault(uval virtAddr) {
	return 0;
    }

    static SysStatus Create(SegmentTable*& segmentTable);


    // not localstrict because destruction is done all on one processor:
    // could do delete remotely, since other cleanup is done that way, if
    // we really want to
    DEFINE_PINNEDGLOBALPADDED_NEW(SegmentTable);

    SysStatus initKernel();
    SysStatus initKernelSegments(HATKernel* hat, VPNum vp);

    void switchToAddressSpace() {
    }

    SysStatus destroy();
};
