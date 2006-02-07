#ifndef __PAGE_SET_H_
#define __PAGE_SET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageSet.H,v 1.5 2004/10/29 16:30:33 okrieg Exp $
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

//#define PAGESET_SANITY

#include "mem/PageDesc.H"

template<class ALLOC> class PageSet {
    struct Entry : PageDesc {
	Entry *next;		        // next in list

	Entry *freeListNext;		// next in freelist
	Entry *freeListPrev;		// prev in freelist

	void init(uval fo, uval pa, uval ln) {
	    PageDesc::init(fo, pa, ln);
	    next = freeListNext = freeListPrev = NULL; // for safety
	}
	DEFINE_ALLOC_NEW(Entry);

	virtual void destroy() { delete this; }
    };

    Entry **hashTable;
    enum {InitialHashTableSize = 4};
    Entry *initialTable[InitialHashTableSize];
    uval   hashMask;			// size of table less 1
    uval   numPages;
    uval   firstIndexUsed;		// get first optimization

    Entry *freeListHead;
    Entry *freeListTail;
    uval   numPagesFree;

    uval hash(uval fileOffset) {
	return((fileOffset>>LOG_PAGE_SIZE)&hashMask);
    }

    void extendHash();

    // add a page at the end of the list
    void enqueue(Entry *p);

#ifdef PAGESET_SANITY
    void sanity(Entry*p);
#else /* #ifdef PAGESET_SANITY */
    void sanity(Entry*p) {(void)p;};
#endif /* #ifdef PAGESET_SANITY */

    // dequeue page corresponding to offset
    PageDesc *dequeue(uval fileOffset);

public:
    PageSet() {
	init();
    }

    void init() {
	uval index;
	numPages = 0;
	freeListHead = 0; freeListTail = 0; numPagesFree = 0;
	hashMask = InitialHashTableSize-1; hashTable = initialTable;
	firstIndexUsed = hashMask;
	for(index=0; index<=hashMask; index++) hashTable[index] = 0;
    }

    void destroy();

    PageDesc *enqueue(uval fileOffset, uval paddr, uval len);

    // dequeue page and free page descriptor entry
    void remove(uval fileOffset);

    // search for the page
    PageDesc *find(uval fileOffset);

    PageDesc *getFirst();

    uval  getNumPages()		{ return numPages; }

    PageDesc *getNext(PageDesc *p);

    // offset == uval(-1) means get head
    PageDesc *getNext(uval offset);

    // Use of Entry not allowed for a variety of reasons
    void      enqueueFreeList(PageDesc *pg);
    void      dequeueFreeList(PageDesc *pg);
    PageDesc *dequeueFreeList(PageDesc::dqtype type); 
    uval      getNumPagesFree()   { return numPagesFree; }

    void print();
};

#endif /* #ifndef __PAGE_SET_H_ */
