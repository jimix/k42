/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TransPageDescHash.C,v 1.6 2001/11/01 01:09:13 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of OTHash object
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "COSMgr.H"
#include "TransPageDescHash.H"

TransPageDescHash::TransPageDescHash(uval8 pool)
    : _pool(pool)
{
    for (int i=0; i<TABLESIZE; i++) _hashTable[i]._chainHead=0;
}

inline int
TransPageDescHash::hash(uval addr) {
    return (addr>>COSMgr::logGTransPageSize) % TABLESIZE;
}

void
TransPageDescHash::add(uval addr, TransPageDesc *pageDesc)
{
    HashNode *hnode;

    hnode = &_hashTable[hash(addr)];

//    hnode->lock();
    pageDesc->setNextHash(hnode->getHead());
    hnode->setHead(pageDesc);
//    hnode->unlock();
}

void
TransPageDescHash::remove(uval addr)
{
    HashNode   *hnode;
    TransPageDesc     *pageDesc, *prev;
    uval      pageAddr;

    hnode = &_hashTable[hash(addr)];
    pageAddr = addr>>COSMgr::logGTransPageSize;

//    hnode->lock();

    for(prev=0, pageDesc=hnode->getHead(); pageDesc!=0;
	 prev=pageDesc, pageDesc=pageDesc->getNextHash()) {

	if((pageDesc->getPageAddr()>>COSMgr::logGTransPageSize) == pageAddr) {
	    // found it, unlink
	    if(prev) {
		prev->setNextHash(pageDesc->getNextHash());
	    } else {
		hnode->setHead(pageDesc->getNextHash());
	    }
//	    hnode->unlock();
	    return;
	}
    }
    tassertWrn(0, "TransPageDescHash::remove failed\n");
//    hnode->unlock();
}

TransPageDesc *
TransPageDescHash::find(uval addr)
{
    HashNode   *hnode;
    TransPageDesc     *pageDesc;
    uval      pageAddr;

    hnode = &_hashTable[hash(addr)];
    pageAddr = addr>>COSMgr::logGTransPageSize;

#if 0
retry:
#endif

//    hnode->lock();

    for(pageDesc=hnode->getHead(); pageDesc!=0;
	pageDesc=pageDesc->getNextHash()) {
	if((pageDesc->getPageAddr()>>COSMgr::logGTransPageSize) == pageAddr) {
	    // ignore locking the otd for now
#if 0
	    // found it; try to lock, but if already locked, retry
	    if(!pageDesc->tryLock()) {
		hnode->unlock();
		yield();
		goto retry;
	    }
#endif
//	    hnode->unlock();
	    return pageDesc;
	}
    }
//    hnode->unlock();

    return (TransPageDesc *)0;
}
