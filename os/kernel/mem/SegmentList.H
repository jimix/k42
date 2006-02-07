#ifndef __SEGMENT_LIST_H_
#define __SEGMENT_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentList.H,v 1.18 2003/01/03 20:35:57 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * A dummy SegmentList object is embedded in the using Object (see HATDefault).
 * It supports add, find, and delete operations.
 * Find searches for an entry that "contains" the address passed, and
 * returns the segmentHATRef for it.
 * Add inserts an entry covering a range.
 * delete searches for the next entry that is "inside" the range passed to
 * delete.  it returns both the segmentHATRef and a segmentList pointer
 * for efficient restart of the delete operation.
 * **************************************************************************/

#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>

template<class ALLOC> class SegmentList {
    SegmentList<ALLOC> *   segmentList;
    uval            segmentstart;
    uval            segmentend;
    VPSet           ppset;		// phys. procs that have accessed seg
    SegmentHATRef   segmentHATRef;
public:
    void init()
    {
	segmentList = 0;		// dummy entry
	segmentstart = 0;
	segmentend = 0;
	segmentHATRef = 0;
	ppset.init();
    }
    //FIXME - must have efficient implemenation - probably machine
    // dependent
    SegmentHATRef findSegment(uval s)
    {
	SegmentList* next = segmentList;
	while (next) {
	    if((next->segmentstart<=s) &&
	       (next->segmentend > s)) {
		return next->segmentHATRef;
	    }
	    next = next->segmentList;
	}
	return 0;
    }
    // returns pointers to ppset for updating, be careful with locking
    SegmentHATRef findSegment(uval s, VPSet * &set)
    {
	SegmentList* next = segmentList;
	while (next) {
	    if((next->segmentstart<=s) &&
	       (next->segmentend > s)) {
		set = &next->ppset;
		return next->segmentHATRef;
	    }
	    next = next->segmentList;
	}
	return 0;
    }
    // returns pointer to internal ppset for updating by caller
    SysStatus addSegment(uval s, uval l, SegmentHATRef ref, VPSet * &set)
    {
	uval e = s + l;		// convert length to end
	SegmentList<ALLOC>* sl = new SegmentList;
	if (!sl)
	    return -1;
	SegmentList<ALLOC> * next = this;
	while (next->segmentList &&
		(next->segmentList->segmentstart < s))
	    next = next->segmentList;
	sl->segmentList = next->segmentList;
	sl->segmentstart = s;
	sl->segmentend = e;
	sl->segmentHATRef = ref;
	set = &sl->ppset;
	next->segmentList = sl;
	return 0;
    }
    /*
     * delete searches for the next segment inside the range passed.
     * if the segment is completely inside the range, it removes it from
     * the list.  restart is set to the element needed to restart the
     * search - NOT the element deleted.
     */
    static SysStatus
    DeleteSegment(uval regionStart, uval regionEnd,
		  uval& segAddr, uval& segEnd, SegmentHATRef &ref, VPSet* &set,
		  SegmentList<ALLOC>* &restart)
    {
	SegmentList<ALLOC> * next, * prev = restart;
	uval segmentStart,segmentEnd;	// use locals to help compiler
	while ((next=prev->segmentList)) {
	    segmentStart=next->segmentstart;
	    segmentEnd=next->segmentend;
	    if(segmentEnd<=regionStart) {
	    } else if (regionEnd<=segmentStart) {
		// all following segments are above the range
		return -1;
	    } else if ((segmentStart>=regionStart) && (segmentEnd<=regionEnd)){
		// segment complete in range, delete it
		restart=prev;
		segAddr=segmentStart;
		segEnd=segmentEnd;
		ref=next->segmentHATRef;
		set=&(next->ppset);
		prev->segmentList = next->segmentList;
		delete next;
		return 0;
	    } else {
		// partial
		restart=next;
		segAddr=segmentStart;
		segEnd=segmentEnd;
		ref=next->segmentHATRef;
		set=&(next->ppset);
		return 1;
	    }
	    prev=next;
	}
	return -1;
    }
    /*
     * findNext searches for the next segment inside the range passed.
     * restart is set to the element needed to restart the
     * search which is the element found.
     */
    static SysStatus
    FindNextSegment(uval regionStart, uval regionEnd,
		  uval& segAddr, uval& segEnd, SegmentHATRef &ref, VPSet* &set,
		  SegmentList<ALLOC>* &restart)
    {
	SegmentList<ALLOC> * next, * prev = restart;
	uval segmentStart,segmentEnd;	// use locals to help compiler
	while ((next=prev->segmentList)) {
	    segmentStart=next->segmentstart;
	    segmentEnd=next->segmentend;
	    if(segmentEnd<=regionStart) {
	    } else if (regionEnd<=segmentStart) {
		// all following segments are above the range
		return -1;
	    } else {
		restart=next;
		segAddr=segmentStart;
		segEnd=segmentEnd;
		ref=next->segmentHATRef;
		set=&(next->ppset);
		return 0;
	    }
	    prev=next;
	}
	return -1;
    }

    DEFINE_ALLOC_NEW(SegmentList);
};
#endif /* #ifndef __SEGMENT_LIST_H_ */
