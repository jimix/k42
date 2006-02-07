#ifndef __PM_H_
#define __PM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PM.H,v 1.22 2005/06/27 06:15:52 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The abstract base class from wich all PMs are derived.
 * **************************************************************************/

// include to get allocation flag values
#include <sys/PageAllocator.H>
#include "mem/FreeFrameList.H"

class PM : public Obj {
protected:
    DEFINE_REFS(PM);

public:

    // state of memory requests
    enum MemLevelState {
	CRITICAL,			// out of memory
	LOW,			// low on memory, free some
	MID,			// not low or high
	HIGH			// no problems
    };

    // couple of macroes to manipulate memlevelstate
    uval static IsLow(MemLevelState v) {
	if ((v == LOW) || (v == CRITICAL)) {
	    return 1;
	} 
	return 0;
    }
    uval static IsCrit(MemLevelState v) {
	if (v == CRITICAL) return 1;
	return 0;
    }

    class Summary {
    public:
	uval total;
	uval free;
	Summary(uval t=0, uval f=0) { total = t; free = f; }
	// could probably be done with operators and copy-constructors
	Summary(Summary *sum) {
	    total = sum->total;
	    free = sum->free;
	}
	void add(Summary *sum) {
	    total += sum->total;
	    free += sum->free;
	}
	void sub(Summary *sum) {
	    uval l, r;			// local, remote
	    // make sure values doesn't go below zero
	    l = total; r = sum->total;
	    total = (l < r) ? 0 : l - r;
	    l = free; r = sum->free;
	    free = (l < r) ? 0 : l - r;
	}
	void replace(Summary *sum) {
	    total = sum->total;
	    free  = sum->free;
	}
	void set(uval t, uval f) { total = t; free = f; }
	void reset() { total = free = 0; }
	void print() {
	    err_printf("tot %ld, free %ld", total, free);
	}
    };

    // attach/detach an fcm to/from this pm
    virtual SysStatus attachFCM(FCMRef fcm)=0;
    virtual SysStatus detachFCM(FCMRef fcm)=0;

    // attach/detach a pm to/from this pm ('this' becomes 'pm's parent)
    virtual SysStatus attachPM(PMRef pm)=0;
    virtual SysStatus detachPM(PMRef pm)=0;

    /* the following are taken from PageAllocator; we could use subclassing,
     * but we lose much of the benefit with the cobj double indirections
     */

    // allocates contiguous region of memory,
    virtual SysStatus allocPages(FCMRef fcm, uval &ptr, uval size,
				 uval pageable,
				 uval flags=0,
				 VPNum node=PageAllocator::LOCAL_NUMANODE)=0;
    virtual SysStatus allocPages(PMRef pm, uval &ptr, uval size,
				 uval pageable,
				 uval flags=0,
				 VPNum node=PageAllocator::LOCAL_NUMANODE)=0;

    // list of standard size, standard node, frames
    virtual SysStatus allocListOfPages(FCMRef fcm, uval count, 
				       FreeFrameList *ffl)=0;
    virtual SysStatus allocListOfPages(PMRef pm, uval count, 
				       FreeFrameList *ffl)=0;

    // free memory at specified physical address
    virtual SysStatus deallocPages(FCMRef fcm, uval paddr, uval size)=0;
    virtual SysStatus deallocPages(PMRef pm, uval paddr, uval size)=0;

    // free memory at addresses specified in a list
    virtual SysStatus deallocListOfPages(FCMRef fcm, FreeFrameList *ffl)=0;
    virtual SysStatus deallocListOfPages(PMRef pm, FreeFrameList *ffl)=0;

    /* End PageAllocator stuff */

    // various entities might hold references to us for adding to the
    // hierarchy, this allows them to logically obtain/release their reference
    virtual SysStatus attachRef()=0;
    virtual SysStatus detachRef()=0;

    // request pages back from a pm 
    virtual SysStatus giveBack(PM::MemLevelState memLevelState)=0;

    // request from allocator to give back cached frames - called without
    // locks
    virtual SysStatus kickPaging()=0;

    // rules for destruction not yet worked out
    virtual SysStatus destroy()=0;

    virtual SysStatus markNonPageable() {return 0;}

    // for testing - print summary info
    virtual SysStatus print()=0;

    // 
    virtual SysStatus freeCachedFrames() { return 0; };
};

#endif /* #ifndef __PM_H_ */
