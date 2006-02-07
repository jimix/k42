#ifndef __PAGE_SET_DENSE_H_
#define __PAGE_SET_DENSE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageSetDense.H,v 1.18 2004/10/29 16:30:33 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains a list of pages for an FCM optimized
 * for when many contiguous pages are populated.  It is array
 * based.
 *
 * FIXME: allow for array re-sizing, fine grain per-page locking...
 * **************************************************************************/

//#define PAGESETDENSE_SANITY
class PageFaultNotification;

#include "mem/PageDesc.H"

class PageSetDense {
    struct Entry : PageDesc {
	void init(uval fo, uval pa, uval ln) {
	    PageDesc::init(fo, pa, ln);
	}
	Entry *freeListNext;		// next in freelist
	Entry *freeListPrev;		// prev in freelist
	DEFINE_GLOBALPADDED_NEW(Entry);
	virtual void destroy() { delete this; }
    };

    enum { MAX_PAGES = 512 };
    Entry *pageArray[MAX_PAGES];

    uval IndexFromOffset(uval fileOffset) {
	tassert( ((fileOffset>>LOG_PAGE_SIZE)<MAX_PAGES),
		 err_printf("file offset too large\n") );
	return fileOffset>>LOG_PAGE_SIZE;
    }

    uval numPages;

    Entry *freeListHead;
    Entry *freeListTail;
    uval   numPagesFree;

#ifdef PAGESETDENSE_SANITY
    void sanity(uval index);
#else /* #ifdef PAGESETDENSE_SANITY */
    void sanity(uval index) {(void)index;};
#endif /* #ifdef PAGESETDENSE_SANITY */

    // dequeue page corresponding to offset
    PageDesc *dequeue(uval fileOffset);

public:
    PageSetDense();

    void init();

    void destroy() { /* empty body */ }

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
    PageDesc *dequeueFreeList(PageDesc::dqtype type ); 
    uval      getNumPagesFree()   { return numPagesFree; }

    void print();
};

#endif /* #ifndef __PAGE_SET_DENSE_H_ */
