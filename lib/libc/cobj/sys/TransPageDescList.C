/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransPageDescList.C,v 1.15 2005/01/26 19:37:09 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "COSMgr.H"
#include "TransPageDescList.H"

GTransEntry *
TransPageDescList::getFreeTransEntry()
{
    GTransEntry *te = 0;
    TransPageDesc *otd;

    lock.acquire();

    if (freeHead) {
	te = freeHead->getFreeTransEntry();
	tassert(te != NULL, err_printf("No free entries on freeHead.\n"));
	if (!freeHead->hasFree()) {
	    // There are no more free entries on the page, so
	    // remove the page from the free list.
	    otd = freeHead->getNextFree();
	    freeHead->setNextFree(0);		// for sanity's sake
	    freeHead = otd;
	}
    }
    lock.release();
    return te;
}

uval
TransPageDescList::numAllocated()
{
    uval ret = 0;
    TransPageDesc *otd;

    lock.acquire();

    for (otd = head; otd != 0; otd = otd->next) {
	ret += otd->numAllocated;
    }

    lock.release();
    return ret;
}

uval
TransPageDescList::returnFreeTransEntry(GTransEntry *te)
{
    lock.acquire();
    TransPageDesc *otd = otdMap.find((uval) te);
    if (otd == 0) {
	// transEntry not on this list return 0
	lock.release();
	return 0;
    }
    if (otd->hasFree()) {
	otd->returnFreeTransEntry(te);
    } else {
	otd->returnFreeTransEntry(te);
	otd->setNextFree(freeHead);
	freeHead = otd;
    }
    lock.release();
    return 1;
}

void
TransPageDescList::addTransPageDesc(TransPageDesc *otd)
{
    // find a free page and add descriptor there
   TransPageDesc *curr;
   TransPageDesc *next;

    // first find a hole, possibly at end
    lock.acquire();

    tassertMsg(length != 0, "Allocation prior to initialization\n");

    curr = head;
    next = NULL;
    while(curr && (next = curr->getNext())) {
	if((curr->getPageAddr() + COSMgr::gTransPageSize) != next->getPageAddr()){
	    // we've found a whole
	    tassert((curr->getPageAddr() + COSMgr::gTransPageSize) <
		     next->getPageAddr(),
		     err_printf("otdecslist out of order\n"));
	    break;
	}
	curr = next;
    }
    if(!curr) {
	// empty list
	tassert(!head, err_printf("otdestlist List not empty?\n"));
	// initialize to first page

	otd->init(startAddr);

	otd->setNext(0);
	head = otd;
	tail = otd;
    } else if(!next) {
	// page goes at end of list
	tassert(curr == tail, err_printf("TransPageDescList not at end?\n"));
	if(!((curr->getPageAddr() + COSMgr::gTransPageSize*2) <=
	     startAddr+length)){
	    err_printf("oops, out of space for otdesc\n");
	    breakpoint();
	}

	otd->init(curr->getPageAddr() + COSMgr::gTransPageSize);

	otd->setNext(0);
	curr->setNext(otd);
	tail = otd;
    } else {
	// page goes somewhere in the middle
	tassert(curr != head, err_printf("TransPageDescList at head?\n"));
	tassert(curr != tail, err_printf("TransPageDescList at tail?\n"));

	otd->init(curr->getPageAddr() + COSMgr::gTransPageSize);

	otd->setNext(next);
	curr->setNext(otd);
    }
    otdMap.add(otd->getPageAddr(), otd);
    // add this the new otd to the head of the free list
    otd->setNextFree(freeHead);
    freeHead = otd;

    lock.release();
}

uval
TransPageDescList::removeTransPageDesc(TransPageDesc *otd)
{
#if 0
    uval found;
    TransPageDesc *curr, *prev;

    lock.acquire();

    found = 0;
    prev = 0;
    curr = head;
    while((curr != 0) && (curr != otd)) {
	prev = curr;
	curr = curr->getNext();
    }
    if(curr) {
	found = 1;
	if(prev == 0) {
	    head = otd->getNext();
	} else {
	    prev->setNext(otd->getNext());
	}
	if(tail == otd) {
	    tail = prev;
	}
    }

    lock.release();

    return found;
#else /* #if 0 */
    tassert(0, err_printf("TransPageDescList::removeTransPageDesc"
			  " not implemented\n"));
    return 0;
#endif /* #if 0 */
}

void
TransPageDescList::print()
{
    TransPageDesc *otd;
    err_printf("TransPageDescList:\n");
    lock.acquire();
    for(otd = head; otd != 0; otd = otd->getNext()) {
	err_printf("\t");
	otd->printNoList();
    }
    lock.release();
}

uval 
TransPageDescList::getCOList(CODesc *coDescs, uval numDescs)
{
    TransPageDesc *otd;
    uval rtn = 0;
    lock.acquire();
    for (otd = head; otd != NULL; otd = otd->getNext()) {
        if ((numDescs - rtn) == 0) break;
 	 rtn += otd->getCOList(&(coDescs[rtn]), numDescs - rtn);
    }
    tassertMsg(numDescs - rtn, "Hmmm might have run out of space\n");
    lock.release();
    return rtn;
}

void
TransPageDescList::printAll()
{
    TransPageDesc *otd;
    err_printf("TransPageDescList:\n");
    lock.acquire();
    for(otd = head; otd != 0; otd = otd->getNext()) {
	err_printf("\t");
	otd->print();
    }
    lock.release();
}

void
TransPageDescList::printVTablePtrs()
{
    TransPageDesc *otd;
    lock.acquire();
    for (otd = head; otd != NULL; otd = otd->getNext()) {
	otd->printVTablePtrs();
    }
    lock.release();
}
