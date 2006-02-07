#ifndef __FREEFRAMELIST_H_
#define __FREEFRAMELIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FreeFrameList.H,v 1.5 2003/11/21 04:04:07 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains lists of free page frames, used in
 * PM classes and soon FCMs, all locking outside of these classes.
 * **************************************************************************/


class FreeFrameList {
public:
    static const uval PER_FRAME_ELEM=509;
    static const uval NATURAL_ALLOC=PER_FRAME_ELEM+1;
protected:
    friend class FreeFrameListOfLists;
    struct FrameInfo {
	FrameInfo *next;
	FrameInfo *prev;
	uval frameCount;
	uval pages[PER_FRAME_ELEM];
    };
    FrameInfo *tail;
    FrameInfo *head;
    uval count;
    void kosher();

public:
    void init() { head=NULL; tail=NULL; count=0; }
    FreeFrameList() { init(); }

    uval getCount() { return count; }
    uval isNotEmpty() { return count; }

    void freeFrame(uval addr) {
	FrameInfo *fi;
	kosher();
	count++;
	if (head && (head->frameCount < PER_FRAME_ELEM)) {
	    head->pages[head->frameCount] = addr;
	    head->frameCount++;
	    kosher();
	    return;
	}

	fi = (FrameInfo *)addr;
	if (head != NULL) {
	    fi->next = head;
	    head->prev = fi;
	} else {
	    fi->next = NULL;
	    tail = fi;
	}
	fi->prev = NULL;
	fi->frameCount = 0;
	head = fi;
	kosher();
    }
	    
    // 0 means no frame available
    uval getFrame() {
	FrameInfo *fi;
	uval addr;
	kosher();

	if (count == 0)  return 0;
	count--;

	if (head->frameCount) {
	    head->frameCount--;
	    addr = head->pages[head->frameCount];
	    tassertMsg(addr, "bad address returned\n");
	    tassertMsg((((uval)addr) & PAGE_MASK) == 0, 
		       "%lx not page aligned?\n", (uval)addr);
	    kosher();
	    return addr;
	}

	// giving up a whole list
	fi = head;
	if (fi->next != NULL) {
	    fi->next->prev = NULL;
	} else {
	    tail = NULL;
	}

	addr = (uval)fi;
	head = fi->next;
	
	tassertMsg(addr, "bad address returned\n");
	tassertMsg((((uval)addr) & PAGE_MASK) == 0, 
		   "%lx not page aligned?\n", (uval)addr);
	kosher();
	return 	addr;
    }	
	
    // returns close to but perhaps less than amount requested
    // for efficient movement of lists
    uval getUpTo(uval reqCount,  FreeFrameList *ffl);

    // returns number of values not satisfied, i.e., 0 means got it all
    uval getList(uval reqCount, FreeFrameList *ffl);

    // returns some list that can be cheaply returned
    // initializes ffl to be 0 if nothing returned
    void getList(FreeFrameList *ffl);

    // returns some list that can be cheaply returned
    // initializes ffl to be 0 if nothing returned
    void getListTail(FreeFrameList *ffl);

    void freeList(FreeFrameList *ffl);
};
    
#endif /* __FREEFRAMELIST_H_ */
    

	    
