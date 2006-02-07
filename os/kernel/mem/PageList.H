#ifndef __PAGE_LIST_H_
#define __PAGE_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageList.H,v 1.34 2004/10/29 16:30:33 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements a list of pages to be used for now
 * by FCMs
 * Notes: - no locking inside, assume locks in calling class
 * **************************************************************************/

//#define PAGELIST_SANITY

#include "mem/PageDesc.H"

template<class ALLOC> class PageList {
    struct Entry : PageDesc {
	Entry *next;		        // next in list

	Entry *freeListNext;		// next in freelist
	Entry *freeListPrev;		// prev in freelist

	Entry *getNext() { return next; }

	void init(uval fo, uval pa, uval ln) {
	    PageDesc::init(fo, pa, ln);
	    next = freeListNext = freeListPrev = NULL; // for safety
	}
	DEFINE_ALLOC_NEW(Entry);

	virtual void destroy() { delete this; }
    };

    Entry *head;
    Entry *tail;
    uval   numPages;

    Entry *freeListHead;
    Entry *freeListTail;
    uval   numPagesFree;

    uval   savedOffset;
    Entry *savedEntry;


#ifdef PAGELIST_SANITY
    void sanity(Entry*p);
#else /* #ifdef PAGELIST_SANITY */
    void sanity(Entry*p) {(void)p;};
#endif /* #ifdef PAGELIST_SANITY */

public:
    PageList() {
	init();
    }

    void init() {
	head = 0; tail = 0; numPages = 0; savedOffset=uval(-1); savedEntry=0;
	freeListHead = 0; freeListTail = 0; numPagesFree = 0;
    }

    void destroy() { /* empty body */ }

    // add a page at the end of the list
    void enqueue(Entry *p);
    PageDesc *enqueue(uval fileOffset, uval paddr, uval len);

    // dequeue page corresponding to offset
    PageDesc *dequeue(uval fileOffset);

    // dequeue page and free page descriptor entry
    void remove(uval fileOffset);

    // search for the page
    PageDesc *find(uval fileOffset);

    PageDesc *getFirst()	{ return head; }

    uval  getNumPages()		{ return numPages; }

    PageDesc *getNext(PageDesc *p)	{ return ((Entry *)p)->getNext(); }

    // offset == uval(-1) means get head
    PageDesc *getNext(uval offset);

    // Use of Entry not allowed for a variety of reasons
    void      enqueueFreeList(PageDesc *pg);
    void      dequeueFreeList(PageDesc *pg);
    PageDesc *dequeueFreeList(PageDesc::dqtype type ); 
    uval      getNumPagesFree()   { return numPagesFree; }

    void print();
};

#endif /* #ifndef __PAGE_LIST_H_ */
