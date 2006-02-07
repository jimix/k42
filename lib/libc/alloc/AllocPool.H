#ifndef __ALLOC_POOL_H_
#define __ALLOC_POOL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocPool.H,v 1.38 2005/04/15 17:39:32 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <defines/optimizations.H>
#include <alloc/LMalloc.H>
#include <alloc/VAllocServices.H>
#include <sys/memoryMap.H>	

class AllocStats;

/*****************************************************************************
 * information used to describe a pool of allocation memory
 * This class is instantiated in the per-processor region as "allocLocal"
 * **************************************************************************/
class AllocPool {
public:
    enum {
	NUM_SIZES          = 8,		// distinct sizes (frontend)
	LOG_MIN_BLOCK_SIZE = 4,		// need two words for free chains
	LOG_MAX_BLOCK_SIZE = LOG_MIN_BLOCK_SIZE + NUM_SIZES -1,
	MIN_BLOCK_SIZE	   = 1<<LOG_MIN_BLOCK_SIZE, // bytes
	MAX_BLOCK_SIZE	   = 1<<LOG_MAX_BLOCK_SIZE,
	NUM_MALLOCS	   = 2*NUM_SIZES, // different lmallocs (backend)
	LOG_NUM_MALLOCS    = 5,		// not exact, just big enough

	MAX_POOLS	   = 2,

	// POOLS available in kernel
	PAGED  = 0,			// unpinned memory alloc pool
	PINNED = 1,			// pinned memory alloc pool

	// default pool
	DEFAULT = 0,

	// value for static clustered objects
	STATIC = MAX_POOLS,

	ALL_POOLS = 99,			// magic value used to init all pools

	GLOBAL_OFF = NUM_SIZES,		// offset for global lmallocs
    };


private:

    // room for 2 complete arrays (localstrict, global), not
    // necessarily all used depending on cache line size and padding
    // rules (lml = lmalloc list)
    // a size index is an indes from 0 to NUM_SIZES which identifies a
    //   power of 2 size.
    // a mallocID is an index into the lml array, used by the higher
    //   level so each page of allocations is only used by one kind of lml.
    LMalloc  lml[NUM_MALLOCS];	// array of freelists

    // global/globalpadded always accessed through indirection array
    LMalloc *pGlobal[NUM_SIZES];	// pointers for "global" blocks
    LMalloc *pGlobalPadded[NUM_SIZES];	// pointers for "globalPadded" blocks


    // list of blocks (with mallocID in them) from remote nodes
    // allocated to cover numanodes in this run
    SyncedCellPtr *remoteList;

    // remote nodes attach block lists allocated from this node to list
    // pointed to by this pointer (shared by all processors in the same node)
    AllocCell    **fromRemoteListOfLists;

    // pointers to remote 'fromRemoteListOfLists' on which to place full
    // remoteLists
    AllocCell    **remoteListPtrs[AllocCell::MAX_NUMANODES];

    // points to global shared array of pointers to remoteListPtrs (we cache
    // them locally in the remoteListPtr array)
    AllocCell   ***globalRemoteListOfListsPtrs;

    // FIXME: move this info into lmallocs once we get rid of pools
    uval32 mallocIDToSizeMapping[NUM_MALLOCS];

    /* Note that node here can refer to processor or numa node depending
     * on which mallocid is referenced (localstrict < cacheline vs others)
     */
    uval32 mallocIDToNodeMapping[NUM_MALLOCS];

    /* Simple table that indicates if a mallocID is a strictly localstrict
     * mallocID (i.e., not shared at any level), or not
     */
    uval8 mallocIDIsLocalStrictMapping[NUM_MALLOCS];

    /* array that maps sizes, pre-shifted so they are multiples of min block
     * size, into malloc indexes; must produce the same results as the index
     * function; used in cases where the size is not known at compile-time.
     * +1 in array size because need 0 size and MAX_BLOCK_SIZE entries.
     */
    uval8  sizeToIndexMapping[(MAX_BLOCK_SIZE >> LOG_MIN_BLOCK_SIZE)+1];

    PageAllocatorRef pageAllocator;	// page allocator for this pool
    AllocStats *allocStats;		// system-wide allocator stats
    VAllocServices *vAllocServ;		// virtual intface to mem-type-dep stuf

    uval mypool;			// this pool id
    uval numaNode;			// numa node this processor is in
    VPNum myVP;				// vp this processor - for use in inline
					// functions

    // for requests too big for cell allocator
    void *largeAlloc(uval size);
    void *largeAlloc(uval size, uval &realSize);
    void  largeFree(void * ptr, uval size);

#ifdef ALLOC_TRACK
    /* point to shared variables in AllocRegionManager; done this way because
     * of complex object dependencies (this makes this class more independent
     * of the other classes)
     */
    uval *totalAllocated;
    uval *maxAllocated;
    uval *totalSpace;
    uval *maxSpace;
    uval *totalRequestedByMID;		// points to array [NUM_MALLOCS]
    uval *maxRequestedByMID;		// points to array [NUM_MALLOCS]
    uval *totalSpaceByMID;		// points to array [NUM_MALLOCS]
    uval *maxSpaceByMID;		// points to array [NUM_MALLOCS]

    // really should be in atomic.h
    static void AtomicMax(uval newval, volatile uval *old) {
	uval tmp;
	do {
	    tmp = *old;
	    if (newval <= tmp) break;
	} while (!CompareAndStoreSynced(old, tmp, newval));
    }

    void updateAllocated(uval size, LMalloc *lm) {
	uval newval;
	uval mid = lm->getMallocID();
	uval space = lm->getSize();
	newval = FetchAndAddSynced(&totalRequestedByMID[mid], size) + size;
	AtomicMax(newval, &maxRequestedByMID[mid]);
	newval = FetchAndAddSynced(&totalSpaceByMID[mid], space) + space;
	AtomicMax(newval, &maxSpaceByMID[mid]);
	newval = FetchAndAddSynced(totalAllocated, size) + size;
	AtomicMax(newval, maxAllocated);
	newval = FetchAndAddSynced(totalSpace, space) + space;
	AtomicMax(newval, maxSpace);
    }
    void updateFreed(uval size, LMalloc *lm) {
	uval mid = lm->getMallocID();
	uval space = lm->getSize();
	FetchAndAddSynced(&totalRequestedByMID[mid], uval(-size));
	FetchAndAddSynced(&totalSpaceByMID[mid], uval(-space));
	FetchAndAddSynced(totalAllocated, uval(-size));
	FetchAndAddSynced(totalSpace, uval(-space));
    }
#else /* #ifdef ALLOC_TRACK */
    void updateAllocated(uval size, LMalloc *lm) { }
    void updateFreed(uval size, LMalloc *lm) { }
#endif /* #ifdef ALLOC_TRACK */

public:
    static uval index(uval size);
    static uval IfBasedIndex(uval size); // uses cascading ifs, no array
    static uval ArrayBasedIndex(uval size); // uses array to index size
    static uval blockSize(uval index);
    uval   mallocIDToSize(uval mallocID);
    static uval   MallocIDToSize(uval mallocID);
    // Note that node here can refer to processor or numa node depending
    // on which mallocid is referenced (localstrict < cacheline vs others)
    uval   mallocIDToNode(uval mallocID);
    static uval   MallocIDToNode(uval mallocID);
    uval   mallocIDIsLocalStrict(uval mallocID);

    void  *allocLocalStrict(uval size);
    void  *allocLocalStrict(uval size, uval &realSize);
    void   freeLocalStrict(void *ptr, uval size);

    void  *allocGlobal(uval size);
    void  *allocGlobal(uval size, uval &realSize);
    void   freeGlobal(void *ptr, uval size);

    void  *allocGlobalPadded(uval size);
    void  *allocGlobalPadded(uval size, uval &realSize);
    void   freeGlobalPadded(void *ptr, uval size);

    void   freeRemote(void *toFree, uval mallocID);
    void   freeRemoteFull(AllocCellPtr list, VPNum numanode);
    uval   checkForRemote();

    void   init(VPNum vp, VAllocServices *vas, uval8 pool);

    uval              getPool()          { return mypool; }
    VAllocServices   *getVAllocServ()    { return vAllocServ; }
    PageAllocatorRef  getPageAllocator() { return pageAllocator; }
    AllocStats       *getStats()         { return allocStats; }

    void   printStats();

    LMalloc  *byMallocId(uval idx)	{ return &lml[idx]; }
    LMalloc  *localStrict(uval idx)	{ return &lml[idx];}
    LMalloc  *global(uval idx)	{ return pGlobal[idx]; }
    LMalloc  *globalPadded(uval idx)	{ return pGlobalPadded[idx]; }
};

//N.B. allocLocal is in processor specific memory - so each processor
//     automagically gets its own copy!
extern AllocPool allocLocal[AllocPool::MAX_POOLS];

// simple support class for doing allocations for allocator objects;
// similar (but not identical to) the MemoryMgrPrimitive
class AllocBootStrapAllocator {
    uval             mem;		// address of start of free chunk
    uval             avail;		// size of free chunk
    PageAllocatorRef pa;		// allocator to get more memory
public:
    AllocBootStrapAllocator(PageAllocatorRef p) { mem = avail = 0; pa = p; }
    void *alloc(uval size);
    void getChunk(uval &m, uval &a) { m = mem; a = avail; mem = avail = 0; }
};


// LMalloc mach-dep implementation needs allocPool definitions above
#include __MINC(LMalloc.H)

inline uval
AllocPool::ArrayBasedIndex(uval size)
{
    uval retvalue;

    if (size <= MAX_BLOCK_SIZE) {
	retvalue = allocLocal[DEFAULT].
	    sizeToIndexMapping[(size+(MIN_BLOCK_SIZE-1))>>LOG_MIN_BLOCK_SIZE];
    } else {
	retvalue = uval(-1);
    }
    return(retvalue);
}

inline uval
AllocPool::IfBasedIndex(uval size)
{
    if      (size <= (MIN_BLOCK_SIZE *   1))	{ return 0; }
    else if (size <= (MIN_BLOCK_SIZE *   2))	{ return 1; }
    else if (size <= (MIN_BLOCK_SIZE *   4))	{ return 2; }
    else if (size <= (MIN_BLOCK_SIZE *   8))	{ return 3; }
    else if (size <= (MIN_BLOCK_SIZE *  16))	{ return 4; }
    else if (size <= (MIN_BLOCK_SIZE *  32))	{ return 5; }
    else if (size <= (MIN_BLOCK_SIZE *  64))	{ return 6; }
    else if (size <= (MIN_BLOCK_SIZE * 128))	{ return 7; }
    // bigger than our quick-alloc block size
    return (uval)-1;
}

inline uval
AllocPool::index(uval size)
{
    // if size is a compile-time constant, use cascading ifs, otherwise array
    if (__builtin_constant_p(size)) {
	return IfBasedIndex(size);
    } else {
	return ArrayBasedIndex(size);
    }
}

// note, make sure this matches above
inline uval
AllocPool::blockSize(uval index)
{
    return MIN_BLOCK_SIZE << index;
}

// note that the ID here is not the same as the index into the front-end
// lists
inline uval
AllocPool::mallocIDToSize(uval mallocID)
{
    return mallocIDToSizeMapping[mallocID];
}
inline uval
AllocPool::MallocIDToSize(uval mallocID)
{
    extern AllocPool allocLocal[AllocPool::MAX_POOLS];
    return allocLocal[DEFAULT].mallocIDToSizeMapping[mallocID];
}
// Note that node here can refer to processor or numa node depending
// on which mallocid is referenced (localstrict < cacheline vs others)
inline uval
AllocPool::mallocIDToNode(uval mallocID)
{
    return mallocIDToNodeMapping[mallocID];
}
inline uval
AllocPool::MallocIDToNode(uval mallocID)
{
    extern AllocPool allocLocal[AllocPool::MAX_POOLS];
    return allocLocal[DEFAULT].mallocIDToNodeMapping[mallocID];
}
inline uval
AllocPool::mallocIDIsLocalStrict(uval mallocID)
{
    return mallocIDIsLocalStrictMapping[mallocID];
}

inline void * AllocPool::
allocLocalStrict(uval size)
{
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateAllocated(size, localStrict(inx));
	return localStrict(inx)->lMalloc();
    } else {
	return largeAlloc(size);
    }
}
inline void * AllocPool::
allocLocalStrict(uval size, uval &realSize)
{
    LMalloc *lm;
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateAllocated(size, localStrict(inx));
	lm = localStrict(inx);
	realSize = lm->getSize();
	return lm->lMalloc();
    } else {
	return largeAlloc(size, realSize);
    }
}
inline void AllocPool ::
freeLocalStrict(void *ptr, uval size)
{
    uval inx = index(size);
    if (inx != uval(-1)) {
// we no longer code the cpu in local strict addresses
// this is so we can share the higher level numa allocators, which
// encode the numa node number in the address
#if 1
	tassert(vAllocServ->nodeID(ptr) ==
		((uval(ptr) >= KERNEL_REGIONS_END)?
		 myVP:localStrict(inx)->getNodeID()),
		err_printf("LocalStrict 0x%lx memdesc node 0x%lx, "
			   "LMalloc node 0x%lx, vp 0x%lx\n",
			   uval(ptr),
			   vAllocServ->nodeID(ptr),
			   localStrict(inx)->getNodeID(),
			   myVP));
#endif /* #if 1 */
	updateFreed(size, localStrict(inx));
	localStrict(inx)->lFreeNoCheckLocal(ptr);
    } else {
	largeFree(ptr, size);
    }
}


inline void * AllocPool::
allocGlobal(uval size)
{
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateAllocated(size, global(inx));
	return global(inx)->lMalloc();
    } else {
	return largeAlloc(size);
    }
}
inline void * AllocPool::
allocGlobal(uval size, uval &realSize)
{
    LMalloc *lm;
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateAllocated(size, global(inx));
	lm = global(inx);
	realSize = lm->getSize();
	return lm->lMalloc();
    } else {
	return largeAlloc(size, realSize);
    }
}
inline void AllocPool ::
freeGlobal(void *ptr, uval size)
{
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateFreed(size, global(inx));
	global(inx)->lFree(ptr);
    } else {
	largeFree(ptr, size);
    }
}


inline void * AllocPool::
allocGlobalPadded(uval size)
{
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateAllocated(size, global(inx));
	return globalPadded(inx)->lMalloc();
    } else {
	return largeAlloc(size);
    }
}
inline void * AllocPool::
allocGlobalPadded(uval size, uval &realSize)
{
    LMalloc *lm;
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateAllocated(size, globalPadded(inx));
	lm = globalPadded(inx);
	realSize = lm->getSize();
	return lm->lMalloc();
    } else {
	return largeAlloc(size, realSize);
    }
}
inline void AllocPool ::
freeGlobalPadded(void *ptr, uval size)
{
    uval inx = index(size);
    if (inx != uval(-1)) {
	updateFreed(size, globalPadded(inx));
	globalPadded(inx)->lFree(ptr);
    } else {
	largeFree(ptr, size);
    }
}


#define DEFINE_NEW(CLASS,ALLOC)						\
inline void * operator new(size_t size)					\
{									\
    tassert(size==sizeof(CLASS),					\
	     err_printf("Wrong new called for" #CLASS "\n"));		\
    return ALLOC::alloc(sizeof(CLASS));					\
}									\
inline void * operator new[](size_t size)					\
{									\
    return ALLOC::alloc(size);						\
}									\
inline void operator delete(void * p, uval size)			\
{									\
    tassert(size==sizeof(CLASS),					\
	     err_printf("Wrong delete called for" #CLASS "\n"));	\
    ALLOC::free(p, sizeof(CLASS));					\
}                       						\
inline void operator delete[](void * p, uval size)			\
{									\
    ALLOC::free(p, size);						\
}

#define DEFINE_LOCALSTRICT_NEW(CLASS)	DEFINE_NEW(CLASS,AllocLocalStrict)
#define DEFINE_GLOBAL_NEW(CLASS)	DEFINE_NEW(CLASS,AllocGlobal)
#define DEFINE_GLOBALPADDED_NEW(CLASS)	DEFINE_NEW(CLASS,AllocGlobalPadded)
#define DEFINE_PINNEDLOCALSTRICT_NEW(CLASS) \
                                  DEFINE_NEW(CLASS,AllocPinnedLocalStrict)
#define DEFINE_PINNEDGLOBAL_NEW(CLASS)	\
                                  DEFINE_NEW(CLASS,AllocPinnedGlobal)
#define DEFINE_PINNEDGLOBALPADDED_NEW(CLASS) \
                                  DEFINE_NEW(CLASS,AllocPinnedGlobalPadded)

/*
 * use the following in templates paramaterized by the allocator.  By
 * convention, the allocator class variable must be ALLOC.
 */
#define DEFINE_ALLOC_NEW(CLASS) \
                                  DEFINE_NEW(CLASS,ALLOC)



#endif /* #ifndef __ALLOC_POOL_H_ */
