/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageSet.C,v 1.8 2004/10/29 16:30:33 okrieg Exp $
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
#include "mem/PageSet.H"
#include <trace/traceMem.h>

/******* PageSet member functions ************/

#ifdef PAGESET_SANITY
template<class ALLOC>
void
PageSet<ALLOC>::sanity(Entry *p)
{
    Entry *pg;
    uval virtForm;
    if (p) {
	pg = p;
	virtForm = PageAllocatorKernPinned::realToVirt(pg->paddr);
	DREFGOBJK(ThePinnedPageAllocatorRef)->isNotFree(virtForm, pg->len);
    }
    for (pg = head; pg != 0; pg = pg->next) {
	if (!p) {
	    DREFGOBJK(ThePinnedPageAllocatorRef)->
		realToVirt(pg->paddr,virtForm);
	    DREFGOBJK(ThePinnedPageAllocatorRef)->isNotFree(virtForm, pg->len);
	}
	if ((pg->paddr >= 0x100000000UL) ||
	   (pg->len != PAGE_SIZE) ||
	   (pg->paddr & (PAGE_SIZE-1))) {
	    cprintf("PageSet sanity %lx %lx %lx\n",
		    pg,pg->paddr,pg->fileOffset);
	    breakpoint();
	}
    }
}
#endif


template<class ALLOC>
void
PageSet<ALLOC>::extendHash()
{
    Entry **oldTable;
    Entry *e, *nexte;
    uval oldMask = hashMask;
    uval oldNumPages = numPages;
    uval i, index;
    oldTable = hashTable;
    i = 2*(hashMask+1);
    if (i<128) i=128;
    while (2*numPages >= i) i = 2*i;
    hashMask = i-1;
    hashTable = (Entry**) ALLOC::alloc(sizeof(Entry *) * i);
    if (!hashTable) {
	hashTable = oldTable;
	hashMask = oldMask;
	firstIndexUsed = 0;		// conservative estimate
	return;
    }
    for (index=0; index <= hashMask; index++) hashTable[index] = 0;
    numPages = 0;

    for (index=0; index <= oldMask; index++) {
	nexte = oldTable[index];
	while ((e = nexte)) {
	    nexte = e->next;
	    enqueue(e);
	}
    }
    tassert(numPages == oldNumPages,
	    err_printf("lost a page in extendHash\n"));

    if (oldTable != initialTable) {
	ALLOC::free(oldTable, sizeof(Entry *) * (oldMask+1));
    }

    return;
}

template<class ALLOC>
void
PageSet<ALLOC>::destroy()
{
    if (hashTable != initialTable) {
	ALLOC::free(hashTable, sizeof(Entry *) * (hashMask+1));
    }
}

template<class ALLOC>
PageDesc *
PageSet<ALLOC>::find(uval fileOffset)
{
    uval index;
    Entry *node;

    index = hash(fileOffset);
    node = hashTable[index];
    while (node != 0) {
	if (node->fileOffset == fileOffset) {
	    return node;
	}
	node = node->next;
    }
    return 0;
}

template<class ALLOC>
void
PageSet<ALLOC>::enqueue(Entry *p)
{
    uval index;

    index = hash(p->fileOffset);
    p->next = hashTable[index];
    hashTable[index] = p;

    numPages++;
    if ( index < firstIndexUsed) {
	firstIndexUsed = index;
    }

    sanity(p);
}

template<class ALLOC>
PageDesc *
PageSet<ALLOC>::enqueue(uval fileOffset, uval paddr, uval len)
{
    Entry *p;

    if (2*numPages > hashMask) {
	extendHash();
    }

    p = new Entry;

    if (p != 0) {
	p->init(fileOffset, paddr, len);
	enqueue(p);
    }

    return p;
}

template<class ALLOC>
PageDesc *
PageSet<ALLOC>::dequeue(uval fileOffset)
{
    Entry *p, *prev;
    uval index;

    index = hash(fileOffset);
    p = hashTable[index];

    /*set prev initially just to prevent compiler from whining
     *about the use below looking undefined to it*/
    for (prev=0; p != 0; prev = p, p = p->next) {
	if (p->fileOffset == fileOffset) {
	    sanity(p);
	    TraceOSMemPageDeq((uval64)this, fileOffset);
	    if (prev == 0) {
		hashTable[index] = p->next;
	    } else {
		prev->next = p->next;
	    }

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
PageSet<ALLOC>::remove(uval fileOffset)
{
    PageDesc *pg;
    pg = dequeue(fileOffset);
    if (pg) pg->destroy();
}


template<class ALLOC>
PageDesc *
PageSet<ALLOC>::getFirst()
{
    Entry *nexte;
    uval index;

    for (index=firstIndexUsed; index <= hashMask; index++) {
	if ((nexte = hashTable[index])) {
	    firstIndexUsed = index;
	    return nexte;
	}
    }
    return 0;
}

/*
 * we treat the case of getNext of a fileOffset that
 * is not found in a strange way, so that we get
 * an approximate walk of all the values, rather
 * than starting from the beginning all the time.
 */
template<class ALLOC>
PageDesc *
PageSet<ALLOC>::getNext(uval fileOffset)
{
    Entry *e;

    if (fileOffset == uval(-1)) {
	return getFirst();
    }

    e = (Entry*) find(fileOffset);

    if (e == 0) {
	/* first search for a full bucket past the one
	 * that fileOffset should have been in.
	 * we don't use fileOffset's bucket, since anything
	 * there may have been before the fileOffset node
	 * so to speak
	 */
	uval index;
	Entry *node;
	for (index = hash(fileOffset)+1; index <= hashMask; index++) {
	    if ((node = hashTable[index])) {
		return node;
	    }
	}

	return getFirst();
    }

    return getNext(e);
}

template<class ALLOC>
PageDesc *
PageSet<ALLOC>::getNext(PageDesc *p)
{
    Entry *e;
    uval index;

    e = ((Entry *)p)->next;

    if (e) return e;

    index = hash(p->fileOffset);
    for (index++; index<=hashMask; index++) {
	if ((e=hashTable[index])) return e;
    }
    return 0;
}

template<class ALLOC>
void
PageSet<ALLOC>::enqueueFreeList(PageDesc *pg)
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
PageSet<ALLOC>::dequeueFreeList(PageDesc::dqtype type)
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
PageSet<ALLOC>::dequeueFreeList(PageDesc *pg)
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
PageSet<ALLOC>::print()
{
    uval index;
    Entry *p;
    cprintf("%ld pages: ", numPages);
    for (index=0;index <= hashMask; index++) {
	for (p = hashTable[index]; p != 0; p = p->next) {
	    char state = p->used ? 'A' : 'I';
	    cprintf("%c", state);
	}
    }
    cprintf("\n");
}

//template instantiation
template class PageSet<AllocGlobal>;
template class PageSet<AllocGlobalPadded>;
template class PageSet<AllocPinnedGlobal>;
template class PageSet<AllocPinnedGlobalPadded>;
