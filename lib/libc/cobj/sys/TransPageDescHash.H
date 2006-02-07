#ifndef __TRANS_PAGE_DESC_HASH_H_
#define __TRANS_PAGE_DESC_HASH_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransPageDescHash.H,v 1.9 2001/10/16 05:31:52 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A hash table to keep a mapping between page addresses
 * and transentry descriptors.
 * **************************************************************************/

// This hash stuff should probably be a template class, although there are
// some subtle differences between the different implementations

#include <cobj/sys/TransPageDesc.H>
#include <cobj/sys/COSMgr.H>

class TransPageDescHash {
    uval8 _pool;
    enum {TABLESIZE=32};

    struct HashNode {
	union {
	    TransPageDesc        *_chainHead;
//	    BSLock<u_long, Bit0>  _chainLock;
	};

	TransPageDesc *getHead()
		{ return (TransPageDesc *)(_chainHead /* & ~3L */); }

	void setHead(TransPageDesc *node)
		{ _chainHead = (TransPageDesc *)
		      (node/* | (reg_t(_chainHead)&3L) */); }

//	void lock()			{ _chainLock.acquireLock(); }
//	void unlock()			{ _chainLock.releaseLock(); }
    };

    HashNode _hashTable[TABLESIZE];

    inline int hash(uval addr);
    TransPageDescHash() { /* empty body */ }
public:
    // allocation can be either pinned or paged
    inline void * operator new(size_t size, uval8 pool) {
	uval pl = pool;
	tassert(size==sizeof(TransPageDescHash), err_printf("bad size \n"));
	if (pl == AllocPool::PINNED) {
	    return (allocPinnedGlobal(sizeof(TransPageDescHash)));
	}
	tassert((pl==AllocPool::PAGED),
		err_printf("UnSupported pool type pool=%ld\n", pl));
	return (allocGlobal(sizeof(TransPageDescHash)));
    }
    inline void operator delete(void * p, uval size) {
	tassert(size==sizeof(TransPageDescHash),
		err_printf("Wrong delete called for TransPageDescHash \n"));
	uval pl = ((TransPageDescHash *)p)->_pool;
	if (pl == AllocPool::PINNED) {
	    return (freePinnedGlobal(p, sizeof(TransPageDescHash)));
	}
	tassert((pl==AllocPool::PAGED),
		err_printf("UnSupported pool type pool=%ld\n", pl));
	return (freeGlobal(p, sizeof(TransPageDescHash)));
    }
    TransPageDescHash(uval8 pool);

    void     add(uval addr, TransPageDesc *otd);
    void     remove(uval addr);
    TransPageDesc  *find(uval addr);
};


#endif /* #ifndef __TRANS_PAGE_DESC_HASH_H_ */
