/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageList.C,v 1.38 2004/10/29 16:30:33 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements a list of pages to be used for now
 * by FCMs
 * **************************************************************************/

#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PM.H"
#include "mem/PageList.H"
#include <trace/traceMem.h>

/******* PageList member functions ************/

#ifdef PAGELIST_SANITY
template<class ALLOC>
void
PageList<ALLOC>::sanity(Entry *p)
{
    Entry *pg;
    uval virtForm;
    if(p) {
	pg = p;
	virtForm = PageAllocatorKernPinned::realToVirt(pg->paddr);
	DREFGOBJK(ThePinnedPageAllocatorRef)->isNotFree(virtForm, pg->len);
    }
    for(pg = head; pg != 0; pg = pg->next) {
	if (!p) {
	    DREFGOBJK(ThePinnedPageAllocatorRef)->
		realToVirt(pg->paddr,virtForm);
	    DREFGOBJK(ThePinnedPageAllocatorRef)->isNotFree(virtForm, pg->len);
	}
	if((pg->paddr >= 0x100000000UL) ||
	   (pg->len != PAGE_SIZE) ||
	   (pg->paddr & (PAGE_SIZE-1))) {
	    cprintf("PageList sanity %lx %lx %lx\n",
		    pg,pg->paddr,pg->fileOffset);
	    breakpoint();
	}
    }
}
#endif


template<class ALLOC>
PageDesc *
PageList<ALLOC>::find(uval fileOffset)
{
    // FIXME: look up through the hash table

    for(Entry *p = head; p != 0; p = p->next) {
	if(p->fileOffset == fileOffset) {
	    sanity(p);
	    return p;
	}
    }
    return 0;
}

// add page at end of the list
template<class ALLOC>
void
PageList<ALLOC>::enqueue(Entry *p)
{
    p->next = NULL;
    if (tail) {
	tail->next = p;
	tail = p;
    }
    else
	head = tail = p;
    numPages++;

    sanity(p);
}

template<class ALLOC>
PageDesc *
PageList<ALLOC>::enqueue(uval fileOffset, uval paddr, uval len)
{
    Entry *p = new Entry;

    if(p != 0) {
	p->init(fileOffset, paddr, len);
	enqueue(p);
    }

    return p;
}

// take out from list the page corresponding to fileOffset
template<class ALLOC>
PageDesc *
PageList<ALLOC>::dequeue(uval fileOffset)
{
    Entry *p, *prev;

    /*set prev initially just to prevent compiler from whining
     *about the use below looking undefined to it*/
    for(p = head,prev=0; p != 0; prev = p, p = p->next) {
	if(p->fileOffset == fileOffset) {
	    sanity(p);
	    TraceOSMemPageDeq((uval64)this, fileOffset);
	    if(p == head) {
		if((head == tail)) {	// only one element in queue
		    head = tail = NULL;
		} else {
		    head = head->next;		// advance head
		}
	    } else { // since p != head, prev must have been set
		if((prev->next = p->next) == NULL) {
		    tail = prev;		// removed last one
		}
	    }
	    // check if removing saved location and invalidate
	    if (fileOffset == savedOffset) savedOffset = uval(-1);
	    p->next = NULL;		// for safety
	    numPages--;
	    if (p->free) {
		dequeueFreeList(p);
	    }
	    return p;
	}
    }
    return 0;
}

template<class ALLOC>
void
PageList<ALLOC>::remove(uval fileOffset)
{
    PageDesc *pg;
    pg = dequeue(fileOffset);
    if(pg) pg->destroy();
}


template<class ALLOC>
PageDesc *
PageList<ALLOC>::getNext(uval offset)
{
    Entry *e;

    if (offset == uval(-1)) {
	e = head;
	if (e != NULL) {
	    savedOffset = e->fileOffset;
	    savedEntry  = e;
	}
	return e;
    }

    if (savedOffset == offset) {
	e = savedEntry;
	tassert(e != 0, err_printf("oops\n"));
    } else {
	// no saved info; do a regular search
	e = (Entry *)find(offset);
	// if we don't find it, we just return the first
	if (e == NULL) {
	    e = head;
	    if (e != NULL) {
		savedOffset = e->fileOffset;
		savedEntry  = e;
	    }
	    return e;
	}
    }
    e = e->next;
    if (e) {
	savedOffset = e->fileOffset;
	savedEntry  = e;
    }
    return e;
}

template<class ALLOC>
void
PageList<ALLOC>::enqueueFreeList(PageDesc *pg)
{
    Entry *p = (Entry *)pg;
    Entry *t;
    tassert(p->freeListNext == NULL, err_printf("oops\n"));
    tassert(p->freeListPrev == NULL, err_printf("oops\n"));
    tassert(p->free, err_printf("oops\n"));
    t = freeListTail;
    freeListTail = p;
    p->freeListPrev = t;
    if (t != NULL) {
	tassert(freeListHead != NULL, err_printf("oops\n"));
	t->freeListNext = p;
    } else {
	tassert(freeListHead == NULL, err_printf("oops\n"));
	freeListHead = p;
    }
    numPagesFree++;
}

template<class ALLOC>
PageDesc *
PageList<ALLOC>::dequeueFreeList(PageDesc::dqtype type)
{
    Entry *pg;
    pg = freeListHead;
    while (pg != NULL) {
	if ((type == PageDesc::DQ_HEAD) || 
	    ((type == PageDesc::DQ_DIRTY) && (pg->doingIO || pg->dirty)) ||
	    ((type == PageDesc::DQ_CLEAN) && !(pg->doingIO || pg->dirty))) 
	    break;
	pg = pg->freeListNext;
    }

    if (pg == NULL) return NULL;
    dequeueFreeList(pg);
    return pg;
}

// take out from freelist the page pointed at by arg
template<class ALLOC>
void
PageList<ALLOC>::dequeueFreeList(PageDesc *pg)
{
    Entry *p = (Entry *)pg;
    tassert(p->free, err_printf("oops\n"));
    if (p->freeListPrev == NULL) {
	// we are at the front
	tassert(freeListHead == p, err_printf("oops\n"));
	freeListHead = p->freeListNext;
    } else {
	// we are not at the front
	p->freeListPrev->freeListNext = p->freeListNext;
	tassert(freeListHead != p, err_printf("oops\n"));
    }

    if (p->freeListNext == NULL) {
	// we are at the tail
	tassert(freeListTail == p, err_printf("oops\n"));
	freeListTail = p->freeListPrev;
    } else {
	p->freeListNext->freeListPrev = p->freeListPrev;
    }
    p->freeListPrev = p->freeListNext = NULL;	// for safety
    numPagesFree--;
}



template<class ALLOC>
void
PageList<ALLOC>::print()
{
    // FIXME: look up through the hash table
    cprintf("%ld pages: ", numPages);
    for(Entry *p = head; p != 0; p = p->next) {
	char state = p->used ? 'A' : 'I';
	cprintf("%c", state);
    }
    cprintf("\n");
}

//template instantiation
template class PageList<AllocGlobal>;
template class PageList<AllocGlobalPadded>;
template class PageList<AllocPinnedGlobal>;
template class PageList<AllocPinnedGlobalPadded>;
