#ifndef __ALLOC_STATS_H_
#define __ALLOC_STATS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocStats.H,v 1.6 2001/10/16 05:31:46 okrieg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Class that stores allocator stats; one for each pool, otherwise shared
 * system wide.
 * **************************************************************************/

class GMallocs;

class AllocStats {

public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba) {
	return ba->alloc(size);
    }

    uval pool;				// alloc pool collecting stats for

#ifdef ALLOC_STATS
    void inc(uval &stat, uval amount=1) { FetchAndAddVolatile(&stat, amount); }
#else /* #ifdef ALLOC_STATS */
    void inc(uval &stat, uval amount=1) { /* no stats */ }
#endif /* #ifdef ALLOC_STATS */

#ifdef ALLOC_TRACK
    /* these variables are pointed to in AllocPool and updated directly
     * from there; see comments in that class for explanation
     */
    uval totalAllocated;
    uval maxAllocated;
    uval totalSpace;
    uval maxSpace;
    uval totalRequestedByMID[AllocPool::NUM_MALLOCS];
    uval maxRequestedByMID[AllocPool::NUM_MALLOCS];
    uval totalSpaceByMID[AllocPool::NUM_MALLOCS];
    uval maxSpaceByMID[AllocPool::NUM_MALLOCS];
#endif /* #ifdef ALLOC_TRACK */
    // inc even if ndebug set
    void incNDebug(uval &stat, uval amount=1) {
	FetchAndAddVolatile(&stat, amount);
    }

    // total number of alloc requests
    uval numAllocCalls[AllocPool::NUM_MALLOCS];
    void incNumAllocCalls(uval i) { inc(numAllocCalls[i]); }
    // total number of blocks allocated
    uval cellsAllocated[AllocPool::NUM_MALLOCS];
    void incCellsAllocated(uval i, uval n) { inc(cellsAllocated[i],n); }
    // total number of free calls made
    uval cellsFreed[AllocPool::NUM_MALLOCS];
    void incCellsFreed(uval i) { inc(cellsFreed[i]); }
    // total number of regions searched across all alloc requests
    uval regionsSearched[AllocPool::NUM_MALLOCS];
    void incRegionsSearched(uval i) { inc(regionsSearched[i]); }
    // total number of memdescs searched across all alloc requests
    uval memDescsSearched[AllocPool::NUM_MALLOCS];
    void incMemDescsSearched(uval i) { inc(memDescsSearched[i]); }
    // total number of memdescs allocated
    uval memDescsAllocated[AllocPool::NUM_MALLOCS];
    void incMemDescsAllocated(uval i) { inc(memDescsAllocated[i]); }
    // total number of memdescs freed
    uval memDescsFreed[AllocPool::NUM_MALLOCS];
    void incMemDescsFreed(uval i) { inc(memDescsFreed[i]); }
    // total number of memdescs emptied
    uval memDescsEmptied[AllocPool::NUM_MALLOCS];
    void incMemDescsEmptied(uval i) { inc(memDescsEmptied[i]); }
    // total number of memdescs un-emptied
    uval memDescsUnemptied[AllocPool::NUM_MALLOCS];
    void incMemDescsUnemptied(uval i) { inc(memDescsUnemptied[i]); }
    // total number of regions allocated
    uval regionsAllocated;
    void incRegionsAllocated() { inc(regionsAllocated); }
    // total number of regions freed
    uval regionsFreed;
    void incRegionsFreed() { inc(regionsFreed); }
    // maximum pages allocated (only makes sense if regions not given back
    uval maxMemDescAllocated;
    void incMaxMemDescAllocated() { incNDebug(maxMemDescAllocated); }

#ifdef ALLOC_STATS
    // lots of space, so make conditional on ALLOC_STATS
    enum { MY_MAX_PROCS = 16 };		// avoid bringing in scheduler
    GMalloc *gmallocs[MY_MAX_PROCS][AllocPool::NUM_MALLOCS];
#endif /* #ifdef ALLOC_STATS */

    void init(uval pool);
    void printStats();
};


#endif /* #ifndef __ALLOC_STATS_H_ */
