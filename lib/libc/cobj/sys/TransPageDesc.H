#ifndef __TRANS_PAGE_DESC_H_
#define __TRANS_PAGE_DESC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransPageDesc.H,v 1.19 2005/01/26 03:21:50 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definition of the descriptor used to store
 * information about a page worth of translation entries.
 * **************************************************************************/
#include <cobj/sys/TransEntry.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <cobj/sys/CODesc.H>

class TransPageDescHash;
class TransPageDescList;

class TransPageDesc  {
    uval8 _pool; // is AllocPool::PINNED, or AllocPool::PAGED
    TransEntry		       *teFreeList;	// free entries for this page
    TransPageDesc              *next;
    TransPageDesc              *nextForFreeEntries;
    TransPageDesc              *nextForHashChain;
    // we may need to deal with TransPageDescs at interrupt level
//    SimpleSpinBlockLock  _lock;		// lock for all fields
    // maybe pageAddr should be int; page number in ot region
    uval               pageAddr;	// what page this describes
    uval               numAllocated;	// how many entries on page allocated

    friend class TransPageDescHash;
    TransPageDesc     *getNext()			{ return next; }
    void        setNext(TransPageDesc *otd)		{ next = otd; }

    friend class TransPageDescList;
    TransPageDesc     *getNextFree()		{ return nextForFreeEntries; }
    void        setNextFree(TransPageDesc *otd)	{ nextForFreeEntries = otd; }
    TransPageDesc     *getNextHash()            { return nextForHashChain; }
    void        setNextHash(TransPageDesc *otd) { nextForHashChain = otd; }
    uval              hasFree()                 { return  teFreeList != 0; }
    TransPageDesc() { /* empty body */ }
public:
    TransPageDesc(uval8 p) : _pool(p) { /* empty body */ }

    // allocation can be either pinned or paged
    inline void * operator new(size_t size, uval8 pool) {
	uval pl = pool;
	tassert(size==sizeof(TransPageDesc), err_printf("bad size \n"));
	if (pl == AllocPool::PINNED) {
	    return (allocPinnedGlobal(sizeof(TransPageDesc)));
	}
	tassert((pl==AllocPool::PAGED),
		err_printf("UnSupported pool type pool=%ld\n", pl));
	return (allocGlobal(sizeof(TransPageDesc)));
    }
    inline void operator delete(void * p, uval size) {
	tassert(size==sizeof(TransPageDesc),
		err_printf("Wrong delete called for TransPageDesc \n"));
	uval pl = ((TransPageDesc *)p)->_pool;
	if (pl == AllocPool::PINNED) {
	    return (freePinnedGlobal(p, sizeof(TransPageDesc)));
	}
	tassert((pl==AllocPool::PAGED),
		err_printf("UnSupported pool type pool=%ld\n", pl));
	return (freeGlobal(p, sizeof(TransPageDesc)));
    }

    // can also allocate from primative allocator
    DEFINE_PRIMITIVE_NEW(TransPageDesc);
    void        init(uval page);
    uval      getPageAddr() 			{ return pageAddr; }

    GTransEntry *getFreeTransEntry() {
//	acquireLock();
	GTransEntry *te = (GTransEntry *)teFreeList;
	tassertMsg((((uval)teFreeList)&7)==0, "non-aligned GTransEntry\n");
	if (te) {
	    teFreeList = te->next;
	    numAllocated++;
	}
//	releaseLock();
	return te;
    }

    void        returnFreeTransEntry(GTransEntry *te) {
//	acquireLock();
	tassertMsg((((uval)te)&7)==0, "non-aligned GTransEntry\n");
	tassertMsg((((uval)teFreeList)&7)==0, "non-aligned GTransEntry list\n");
	te->co = 0;
	te->next = teFreeList;
	teFreeList = te;
	numAllocated--;
//	releaseLock();
    }

//    void        acquireLock()			{ _lock.acquireLock(); }
//    int         tryLock()			{ return _lock.tryLock(); }
//    void        releaseLock()			{ _lock.releaseLock(); }

    void        print();
    void        printNoList();
    void        printVTablePtrs();
    uval        getCOList(CODesc *coDescs, uval numDescs);
};

#endif /* #ifndef __TRANS_PAGE_DESC_H_ */
