/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageSetDense.C,v 1.19 2004/10/29 16:30:33 okrieg Exp $
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
#include "mem/PageSetDense.H"
#include <trace/traceMem.h>


#ifdef PAGESETDENSE_SANITY
void
PageSetDense::sanity(uval index)
{
    Entry *pg;
    uval virtForm;
    if(index!=uval(-1)) {
	pg = pageArray[index];
	if(!pg) breakpoint();
	virtForm = PageAllocatorKernPinned::realToVirt(pg->paddr);
	DREFGOBJK(ThePinnedPageAllocatorRef)->isNotFree(virtForm, pg->len);
    }
    for (uval i=0; i<MAX_PAGES; i++) {
	if((pg=pageArray[i])) {
	    if(index == uval(-1)) {
		DREFGOBJK(ThePinnedPageAllocatorRef)->
		    realToVirt(pg->paddr, virtForm);
		DREFGOBJK(ThePinnedPageAllocatorRef)->
		    isNotFree(virtForm, pg->len);
	    }
	    if((pg->paddr >= 0x100000000UL) ||
	       (pg->len != PAGE_SIZE) ||
	       (pg->paddr & (PAGE_SIZE-1))) {
		cprintf("PageSet sanity %lx %lx %lx\n",i,pg,pg->paddr);
		breakpoint();
	    }
	}
    }
}
#endif

PageSetDense::PageSetDense()
{
    init();
}

void
PageSetDense::init()
{
    numPages = 0;
    for (uval i=0; i<MAX_PAGES; i++) {
	pageArray[i] = 0;
    }
}

PageDesc *
PageSetDense::find(uval fileOffset)
{
    uval index = IndexFromOffset(fileOffset);

    /* in addition to checking for the right fileoffset, we must
       also make sure that the page is not an interval marker
       (special purpose) page. Interval markers have: fileOffset
       zero (a valid page may have fileOffset zero), paddr zero
       (there can be page descriptors with paddr == 0 if they are in
       process of allocating the physical page), and len zero.  */

    if (!pageArray[index]) return 0;
    sanity(index);
    tassert((pageArray[index]->fileOffset == fileOffset),
	    err_printf("woops\n"));
    return pageArray[index];
}

PageDesc *
PageSetDense::getFirst()
{
    uval i;
    Entry *p;
    for (i=0; i<MAX_PAGES; i++) {
	p = pageArray[i];
	if (p != NULL) return p;
    }
    return 0;
}

PageDesc *
PageSetDense::enqueue(uval fileOffset, uval paddr, uval len)
{
    uval index = IndexFromOffset(fileOffset);
    Entry *p = new Entry;
    tassert(p, err_printf("woops\n"));
    if (!p) return 0;

    p->init(fileOffset, paddr, len);
    numPages++;
    pageArray[index] = p;
    sanity(index);
    return p;
}

// take out from list the page corresponding to fileOffset
PageDesc *
PageSetDense::dequeue(uval fileOffset)
{
    uval index = IndexFromOffset(fileOffset);
    if (!pageArray[index]) return 0;
    sanity(index);
    Entry *p=pageArray[index];
    numPages--;
    pageArray[index]=0;
    if (p->free) {
	dequeueFreeList(p);
    }
    return p;
}

void
PageSetDense::remove(uval fileOffset)
{
    PageDesc *pg;
    pg = dequeue(fileOffset);
    if(pg) pg->destroy();
}

// FIXME: get rid of this
PageDesc *
PageSetDense::getNext(PageDesc *p)
{
    return getNext(p->fileOffset);
}

PageDesc *
PageSetDense::getNext(uval offset)
{
    uval i;
    if (offset == uval(-1)) {
	return getFirst();
    }

    // get next page after this one
    i = IndexFromOffset(offset);
    i++;
    for(;i<MAX_PAGES;i++) {
	if (pageArray[i])
	    return pageArray[i];
    }
    return 0;
}

void
PageSetDense::enqueueFreeList(PageDesc *pg)
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

PageDesc *
PageSetDense::dequeueFreeList(PageDesc::dqtype type)
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
void
PageSetDense::dequeueFreeList(PageDesc *pg)
{
    Entry *p = (Entry *)pg;
    tassert(pg->free, err_printf("oops\n"));
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

void
PageSetDense::print()
{
    cprintf("%ld pages: ", numPages);
    for (uval i=0; i<MAX_PAGES; i++) {
	if (pageArray[i]) {
	    Entry *p = pageArray[i];
	    char state = p->used ? 'A' : 'I';
	    cprintf("%c", state);
	}
    }
    cprintf("\n");
}
