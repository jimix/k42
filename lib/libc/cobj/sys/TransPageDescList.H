#ifndef __TRANS_PAGE_DESC_LIST_H_
#define __TRANS_PAGE_DESC_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransPageDescList.H,v 1.17 2005/01/26 03:21:51 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains a list of OTDesc for allocating free
 * trans entries.  The list is maintained in sorted order according to
 * page address with the idea that it will allow later pages a greater
 * chance of having all their trans entries returned allowing the page
 * to be returned to the drammanager.  This may not be ideal for other
 * purposes.  Also, even pages that have no free entries are maintained
 * on the list to simplify management for now.
 *
 * Also note that we are friends with class OTDesc.
 * **************************************************************************/
#include <cobj/sys/TransEntry.H>
#include <cobj/sys/TransPageDescHash.H>
#include <cobj/sys/CODesc.H>

class TransPageDescList {
    uval8             _pool;
    uval              startAddr;      // starting address of trans entries
    uval              length;         // length, in bytes, of table
    TransPageDescHash otdMap;	      // map from addr to otd

    TransPageDesc     *head;
    TransPageDesc     *tail;
    TransPageDesc     *freeHead;

    BLock  lock;		// probably should be bit lock on head

    TransPageDescList() : otdMap(AllocPool::STATIC) { init(0,0); }
public:
    // allocation can be either pinned or paged
    inline void * operator new(size_t size, uval8 pool) {
	uval pl = pool;
	tassert(size==sizeof(TransPageDescList), err_printf("bad size \n"));
	if (pl == AllocPool::PINNED) {
	    return (allocPinnedGlobal(sizeof(TransPageDescList)));
	}
	tassert((pl==AllocPool::PAGED),
		err_printf("UnSupported pool type pool=%ld\n", pl));
	return (allocGlobal(sizeof(TransPageDescList)));
    }
    inline void operator delete(void * p, uval size) {
	tassert(size==sizeof(TransPageDescList),
		err_printf("Wrong delete called for TransPageDescList \n"));
	uval pl = ((TransPageDescList *)p)->_pool;
	if (pl == AllocPool::PINNED) {
	    return (freePinnedGlobal(p, sizeof(TransPageDescList)));
	}
	tassert((pl==AllocPool::PAGED),
		err_printf("UnSupported pool type pool=%ld\n", pl));
	return (freeGlobal(p, sizeof(TransPageDescList)));
    }

    void init(uval addr, uval len) {
	head = tail = freeHead = 0;
	startAddr = addr;
	length = len;
    }

    void init(uval addr, uval len, TransPageDesc *firstPage, uval numRes) {
	startAddr = addr;
	length = len;

	firstPage->init(startAddr);
	firstPage->setNext(0);
	head = firstPage;
	tail = firstPage;
	otdMap.add(firstPage->getPageAddr(), firstPage);
	// add this the new otd to the head of the free list
	firstPage->setNextFree(freeHead);
	freeHead = firstPage;

	// Remove all reserved entries from the free list
	for (uval i = 0; i < numRes; i++) {
	    TransEntry *te;
	    te = freeHead->getFreeTransEntry();
	    tassertMsg(te != NULL, "Unable to reserve translation entries.\n");
	}
	tassertMsg(freeHead->hasFree(), "firstPage already exhausted.\n");
    }

    TransPageDescList(uval8 p)
	: _pool(p), otdMap(p) {
	/* empty body */
    }

    TransPageDescList(uval8 p, uval addr, uval len)
	: _pool(p), otdMap(p) { init(addr, len); }

    uval numAllocated();

    GTransEntry *getFreeTransEntry();
    uval        returnFreeTransEntry(GTransEntry *te);

    void        addTransPageDesc(TransPageDesc *otd);
    uval        removeTransPageDesc(TransPageDesc *otd);

    void        print();
    void        printAll();
    uval        getCOList(CODesc *coDescs, uval numDescs);
    void	printVTablePtrs();

};

#endif /* #ifndef __TRANS_PAGE_DESC_LIST_H_ */
