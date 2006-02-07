#ifndef __LMALLOC_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __LMALLOC_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LMalloc.H,v 1.19 2003/08/28 18:11:12 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine specific component of LMAlloc.H
 * **************************************************************************/

#if defined(INCLASS)
/* nothing need inclass for powerpc */
#elif defined(OUTOFCLASS)
/* nothing need outofclass for powerpc */
#else /* #if defined(INCLASS) */

inline void *
LMalloc::lMalloc()
{
    void *el;
#if defined(ALLOC_STATS)
    FetchAndAddVolatile(&allocs, 1);
#endif /* #if defined(ALLOC_STATS) */
    if ((el = freeList.pop(nodeID)) == NULL) {
	el = slowMalloc();
    }
    return el;
}

inline void
LMalloc::lFree(void *tolfree)
{
    tassertMsg((uval(tolfree)&(blockSize-1))==0,
	       "freeing misaligned pointer %p\n", tolfree);
    AllocCellPtr tmp;
#if defined(CHECK_MALLOC_IDS)
    checkMallocID(tolfree);
#endif /* defined(CHECK_MALLOC_IDS) */
    if (AllocCell::AddrToNumaNode(uval(tolfree)) != nodeID) {
#if defined(ALLOC_STATS)
	FetchAndAddVolatile(&remoteFrees, 1);
#endif /* #if defined(ALLOC_STATS) */
	allocLocal[pool].freeRemote(tolfree, mallocID);
	return;
    } else {
#if defined(ALLOC_STATS)
	FetchAndAddVolatile(&frees, 1);
#endif /* #if defined(ALLOC_STATS) */
	if (freeList.push(tolfree, maxCount, tmp)
	    == SyncedCellPtr::SUCCESS) {
	    return;
	}
    }
    moveUp(tmp);
}

inline void
LMalloc::lFreeNoCheckLocal(void *tolfree)
{
    tassertMsg((uval(tolfree)&(blockSize-1))==0,
	       "freeing misaligned pointer %p\n", tolfree);
    AllocCellPtr tmp;
#if defined(CHECK_MALLOC_IDS)
    checkMallocID(tolfree);
#endif /* defined(CHECK_MALLOC_IDS) */
#if defined(ALLOC_STATS)
    FetchAndAddVolatile(&frees, 1);
#endif /* #if defined(ALLOC_STATS) */
    if (freeList.push(tolfree, maxCount, tmp) == SyncedCellPtr::SUCCESS) {
	return;
    }
    moveUp(tmp);
}

#endif /* #if defined(INCLASS) */

