/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocPool.C,v 1.54 2005/08/01 01:35:14 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: per-pool alloc class
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <misc/macros.H>
#include <defines/mem_debug.H>
#include <defines/experimental.H>
#include <scheduler/Scheduler.H>
#include "PageAllocator.H"
#include "AllocPool.H"
#include "MemDesc.H"
#include "GMalloc.H"
#include "PMalloc.H"
#include "LMalloc.H"
#include "AllocRegionManager.H"
#include "AllocStats.H"
#include "VAllocServices.H"

#ifdef DEBUG_LEAK
#include <sys/LeakProof.H>
LeakProof* allocLeakProof = 0;
#endif /* #ifdef DEBUG_LEAK */

// some type decls for local init structures
struct InitInfo {
    void               *allocRegion;	// opaque handle
    GMalloc            *gms[AllocPool::NUM_MALLOCS];
    PMalloc            *pms[AllocPool::NUM_MALLOCS];
    AllocCell         **fromRemoteListOfLists;
};
typedef InitInfo *InitInfoP;
typedef InitInfoP InitInfoPArray[AllocCell::MAX_NUMANODES];

// support function for allocating memory from pageallocator pages

void *
AllocBootStrapAllocator::alloc(uval size)
{
    SysStatus rc;
    void *result;
    uval  pasize;

    tassertMsg(size > 0, "Allocating 0-size chunk.\n");
    if (size > avail) {
	pasize = ALIGN_UP(size,ALLOC_PAGE_SIZE);
	rc = DREF(pa)->allocPages(mem, pasize);
	tassertSilent(_SUCCESS(rc), BREAKPOINT);
	avail = pasize;
    }
    result = (void *)mem;
    mem += size;
    avail -= size;
    return result;
}

// wait for init info from other processor to be set
static void
WaitForInitInfo(VPNum numanode, uval8 pool,
		InitInfoPArray *init[AllocPool::MAX_POOLS])
{
    // wait for init info array to be set up
    if (init[pool] == NULL) {
	//err_printf("Alloc - waiting for primary proc\n");
	do {
	    Scheduler::DelayMicrosecs(100000);
	} while (init[pool] == NULL);
    }
    // wait for the init info entry for the one we need to be set up
    if ((*init[pool])[numanode] == NULL) {
	//err_printf("Alloc - waiting for numanode %ld\n", uval(numanode));
	do {
	    Scheduler::DelayMicrosecs(100000);
	} while ((*init[pool])[numanode] == NULL);
    }
}

// support structures to figure out configuration of system
// We support different views of the world for pinned and unpinned allocations;
// types are uval to make CompareAndStore easier
uval *VpToNumaNode[AllocPool::MAX_POOLS]; // size [Scheduler::VPLimit]
uval *NumaNodeToVP[AllocPool::MAX_POOLS]; // size [AllocCell::MAX_NUMANODES]

// stand-in functions to pretend we have NUMA node mapping stuff figured out
// true if vp is the very first vp (i.e. == 0)
static uval AmFirstProc(VPNum vp, uval8 pool)
{
  return vp == 0;
}

// returns numa node the given vp is part of
static VPNum VPToNUMANode(VPNum vp, uval8 p) {
    tassertSilent(VpToNumaNode[p][vp] != uval(-1), BREAKPOINT);
    return VpToNumaNode[p][vp];
}
#if 0
// returns best matching vp on given numanode given vp from another numanode
static VPNum NUMANodeToVP(VPNum numanode, VPNum vp, uval8 p) {
    tassertSilent(NumaNodeToVP[p][numanode] != uval(-1), BREAKPOINT);
    return NumaNodeToVP[p][numanode];
}
#endif /* #if 0 */

// simple support functions that encapsulate policies for list sizes and
// lengths
// size of lists held in lmalloc (and gmalloc for sublists) layer
static uval ListSize(uval sizeIndex, uval mallocID)
{
    uval listSize;
    //const uval listSpace = 1*PAGE_SIZE;	// total space in list
    const uval listSpace = 16*PAGE_SIZE;	// total space in list
    const uval minListSize = 4;		// mostly arbitrary
    const uval maxListSize = AllocCell::CELL_MAX_COUNT;

    // idea is to keep amount of data in list roughly constant
    listSize = listSpace / AllocPool::blockSize(sizeIndex);

    // respect min/max values
    if (listSize > maxListSize) listSize = maxListSize;
    if (listSize < minListSize) listSize = minListSize;

    return listSize;
}
// number of lists held in gmalloc layer
static uval NumLists(uval sizeIndex, uval mallocID, uval degreeOfSharing)
{
    // idea is to size number of lists based on number of processors
    // sharing the object, plus some extra factors to provide a buffer
    // between the lower and upper levels
    return degreeOfSharing * 4/*8*/;
}

/*
 * Drives initialization of allocation facility for this vp and pool.
 * Includes policy decisions for degree of sharing across processors at
 * various levels, sizes of various free lists, cacheline size driven
 * tradeoff point, etc.
 */
void
AllocPool::init(VPNum vp, VAllocServices *vas, uval8 pool)
{
    uval i;
    uval clsizeindex;			// front-end index for size==clsize
    uval realclsizeindex;		// overrides debugging settings
    uval amFirstProcForNUMANode = 0;
    uval otherPool; GMalloc *gm;
    PMalloc *pm;
    void *localAllocRegionManager;	// opaque handle
    InitInfo *myInitInfo = 0;
    GMalloc *localGMs[NUM_MALLOCS];	// not all are needed
    uval numberOfNumaNodes;		// used to size arrays
    uval numberOfVPs;			// used to size arrays

    // points to init information for each numa node
    // static so shared across all vps
    static InitInfoPArray *initInfo[MAX_POOLS];
    static AllocStats *initStats[MAX_POOLS];
    static AllocCell ***GlobalRemoteListOfListsPtrs[MAX_POOLS];

    AllocBootStrapAllocator ba(vas->getPageAllocator());

    VPNum nodeSize;
    
    // get these from PageAllocator so real memory can be handled
    // differently
    DREF(vas->getPageAllocator())->getNumaInfo(vp, numaNode, nodeSize);

    // FIXME: Most processes have just one VP.  The vast majority of the rest
    //        have no more VPs than there are physical processors.  So sizing
    //        these structures for the maximum possible number of VPs is
    //        wasteful overkill.
    numberOfVPs = Scheduler::VPLimit;

    numberOfNumaNodes = (numberOfVPs+nodeSize-1)/nodeSize;
    
    myVP = vp;
    
    tassertSilent(numberOfNumaNodes < AllocCell::MAX_NUMANODES, breakpoint());

    /*
     * Note it's not safe to use printfs here (at least for user-level)
     */

    // we assume there are only 2 pools to initialize
    tassertSilent(MAX_POOLS == 2, BREAKPOINT);
    if (pool == ALL_POOLS) {
	// we are initializing both pools at once and should share resources
	// we reset pool to our current pool and set the otherPool to
	// refer to the other pool
	pool = this - allocLocal;
	tassertSilent(pool == PAGED || pool == PINNED, BREAKPOINT);
	otherPool = (pool == PAGED) ? PINNED : PAGED;
    } else {
	// in several places we set the otherPool fields; if pool==otherPool,
	// these will just be expensive nops, but we don't really care about
	// the minor cost
	otherPool = pool;
    }

    // Note, we assume this function will run-to-completion on the first vp
    // before other vps can be created
    if (AmFirstProc(vp, pool)) {
	VpToNumaNode[pool] = (uval *) ba.alloc(sizeof(uval) * numberOfVPs);
	memset(VpToNumaNode[pool], -1, sizeof(uval) * numberOfVPs);
	VpToNumaNode[otherPool] = VpToNumaNode[pool];
	NumaNodeToVP[pool] = (uval *)
	    ba.alloc(sizeof(uval) * numberOfNumaNodes);
	memset(NumaNodeToVP[pool], -1, sizeof(uval)*numberOfNumaNodes);
	NumaNodeToVP[otherPool] = NumaNodeToVP[pool];
    }

    VpToNumaNode[pool][vp] = numaNode;
    VpToNumaNode[otherPool][vp] = numaNode;

    // see if we're the first to initialize on this node
    if (CompareAndStoreSynced(&NumaNodeToVP[pool][numaNode], uval(-1), vp)) {
	// we're first in numa node to initialize
	amFirstProcForNUMANode = 1;
    }
    tassertSilent(numaNode == VPToNUMANode(vp, pool), BREAKPOINT);

    // just to be safe
    for (i = 0; i < NUM_MALLOCS; i++) {
	localGMs[i] = 0;
	allocLocal[pool].lml[i].init(0,0,0,0,0,0);
	if (otherPool != pool) {
	    allocLocal[otherPool].lml[i].init(0,0,0,0,0,0);
	}
    }

    /* initialize size to index mappings for both pools, since we always
     * use the DEFAULT one, but have no guarantee which pool is initialized
     * first; double init doesn't matter since they are the same in each.
     */
    for (i = 0; i <= MAX_BLOCK_SIZE; i += MIN_BLOCK_SIZE) {
	allocLocal[PAGED].sizeToIndexMapping[i >> LOG_MIN_BLOCK_SIZE] =
	    IfBasedIndex(i);
	allocLocal[PINNED].sizeToIndexMapping[i >> LOG_MIN_BLOCK_SIZE] =
	    IfBasedIndex(i);
    }

    /* For debugging, we pretend that the dividing line between local strict
     * and global, the cache line size, is actually the maximum block size,
     * so that the two are kept distinct.  However, we still need the real
     * divider for the globalpadded case
     */
#if defined(NDEBUG) && !defined(ASSERTIONS_ON)
    clsizeindex = index(KernelInfo::SCacheLineSize());
#else
    clsizeindex = NUM_SIZES;		// keeps everything separate
#endif /* #if defined(NDEBUG) && !defined(ASSERTIONS_ON) */

    allocLocal[pool].pageAllocator = vas->getPageAllocator();
    allocLocal[pool].mypool = pool;
    allocLocal[pool].vAllocServ = vas;
    if (otherPool != pool) {
	allocLocal[otherPool].pageAllocator = vas->getPageAllocator();
	allocLocal[otherPool].mypool = otherPool;
	allocLocal[otherPool].vAllocServ = vas;
    }


#ifdef DEBUG_LEAK

/*
 *   WHENEVER SOMEONE RE-ENABLES THIS
 *	1. WE NEED LOCKS, NO LOCKING GOING ON
 *	2. WE CANNOT DO MEMORY ALLOCATION AT THIS STAGE
 *  SORRY, GUESS YOU NOTICED THIS HUH?
*/
    //leakProof must be running before static constructors are run
    //so we make it a local static.  Also, we only make one, even
    //if there are several alloc pools
    //FIXME: do we do anything special for per-processor pools?
    //FIXME: can't do memory allocs this early other than on
    //       vp 0 because can't block on allocator lock this early
    if (!allocLeakProof) {
	uval ptr;
	i = sizeof(LeakProof);		// allocate space for leakproof itself
	i = ALIGN_UP(i,PAGE_SIZE);
	ptr = (uval) ba.alloc(i);
	tassertSilent(ptr != 0, BREAKPOINT);
	allocLeakProof = (LeakProof *)ptr;
	allocLeakProof -> init(0, 0);
    }
#endif /* #ifdef DEBUG_LEAK */

    // first we do system-wide one-time stuff
    if (AmFirstProc(vp, pool)) {
	// first create stats class; only one across all processors
	initStats[pool] = (AllocStats *)ba.alloc(sizeof(AllocStats));
	initStats[pool]->init(pool);
	// next need to allocate the array of pointers
	uval sizeOfInitInfoPArray;
	sizeOfInitInfoPArray = sizeof(InitInfoP) * numberOfNumaNodes;
	initInfo[pool] = (InitInfoPArray *)ba.alloc(sizeOfInitInfoPArray);
	memset(initInfo[pool], 0, sizeOfInitInfoPArray); // clear to be safe

	// next allocate array of remotelist pointers that point to per-node
	// lists of lists of blocks freed remotely
	GlobalRemoteListOfListsPtrs[pool] = (AllocCell ***)
	    ba.alloc(sizeof(AllocCell **)*numberOfNumaNodes);
	memset(GlobalRemoteListOfListsPtrs[pool], 0,
	       sizeof(AllocCell **)*numberOfNumaNodes);
    }
    
#ifdef ALLOC_STATS
    tassertSilent(vp < AllocStats::MY_MAX_PROCS, BREAKPOINT);
#endif /* #ifdef ALLOC_STATS */
    allocLocal[pool].allocStats = initStats[pool];
    allocLocal[otherPool].allocStats = initStats[pool];
#ifdef ALLOC_TRACK
    allocLocal[pool].totalAllocated	    = &initStats[pool]->totalAllocated;
    allocLocal[otherPool].totalAllocated    = &initStats[pool]->totalAllocated;
    allocLocal[pool].maxAllocated	    = &initStats[pool]->maxAllocated;
    allocLocal[otherPool].maxAllocated	    = &initStats[pool]->maxAllocated;
    allocLocal[pool].totalSpace		    = &initStats[pool]->totalSpace;
    allocLocal[otherPool].totalSpace	    = &initStats[pool]->totalSpace;
    allocLocal[pool].maxSpace		    = &initStats[pool]->maxSpace;
    allocLocal[otherPool].maxSpace	    = &initStats[pool]->maxSpace;
    allocLocal[pool].totalRequestedByMID= initStats[pool]->totalRequestedByMID;
    allocLocal[otherPool].totalRequestedByMID =
	initStats[pool]->totalRequestedByMID;
    allocLocal[pool].maxRequestedByMID = initStats[pool]->maxRequestedByMID;
    allocLocal[otherPool].maxRequestedByMID=initStats[pool]->maxRequestedByMID;
    allocLocal[pool].totalSpaceByMID	    = initStats[pool]->totalSpaceByMID;
    allocLocal[otherPool].totalSpaceByMID   = initStats[pool]->totalSpaceByMID;
    allocLocal[pool].maxSpaceByMID	    = initStats[pool]->maxSpaceByMID;
    allocLocal[otherPool].maxSpaceByMID	    = initStats[pool]->maxSpaceByMID;
#endif /* #ifdef ALLOC_TRACK */

    // allocate the remoteList for this vp
    allocLocal[pool].remoteList =
	(SyncedCellPtr*)ba.alloc(sizeof(SyncedCellPtr)*numberOfNumaNodes);
    
    if (otherPool != pool) {
	allocLocal[otherPool].remoteList = (SyncedCellPtr*)
	    ba.alloc(sizeof(SyncedCellPtr)*numberOfNumaNodes);
    }
	
    // next we do stuff that is always one-per-numanode
    if (amFirstProcForNUMANode) {
	    // init stuff neded by other processors in same numa node
	myInitInfo = (InitInfo *)ba.alloc(sizeof(InitInfo));
	memset(myInitInfo, 0, sizeof(InitInfo)); // clear to be safe

	// we also allocate the region stuff; one per numa node
	myInitInfo->allocRegion = vas->createNodeAllocRegionManager(this,&ba);

	myInitInfo->fromRemoteListOfLists = (AllocCell **)
	    ba.alloc(sizeof(AllocCell *));
	*myInitInfo->fromRemoteListOfLists = NULL;
	GlobalRemoteListOfListsPtrs[pool][numaNode] =
	    myInitInfo->fromRemoteListOfLists;

	// next we do all the stuff that might be shared across other
	// processors and hence goes in the initstuff array; we want to
	// fully initialize this stuff before we make it available to
	// other processors by putting it in the shared array

	// we do the local stuff >= secondary cache line which
	// uses shared g/p stuff
	for (i=clsizeindex; i<NUM_SIZES; i++) {
	    pm = vas->createPMalloc(
		i, numaNode, myInitInfo->allocRegion, pool, &ba);
	    gm = new(&ba) GMalloc(ListSize(i,i), NumLists(i, i, nodeSize),
				  pool, numaNode, pm);
	    // save for secondary processors
	    myInitInfo->pms[i] = pm;
	    myInitInfo->gms[i] = gm;
#ifdef ALLOC_STATS
	    initStats[pool]->gmallocs[vp][i] = gm;
#endif /* #ifdef ALLOC_STATS */
	}


	// next we do the global stuff for less than secondary cache line
	// size (everything larger will use the local stuff)
	for (i=GLOBAL_OFF; i<(GLOBAL_OFF+clsizeindex); i++) {
	    const uval idx = i - GLOBAL_OFF;
	    pm = vas->createPMalloc(
		i, numaNode, myInitInfo->allocRegion, pool, &ba);
	    gm = new(&ba) GMalloc(ListSize(idx,i), NumLists(idx, i, nodeSize),
				  pool, numaNode, pm);
	    // save for secondary processors
	    myInitInfo->pms[i] = pm;
	    myInitInfo->gms[i] = gm;
#ifdef ALLOC_STATS
	    initStats[pool]->gmallocs[vp][i] = gm;
#endif /* #ifdef ALLOC_STATS */
	}
    }


    // we next want to initialize anything else that needs bootstrapmem,
    // which is not shared; we store in local variables to hold until we
    // really need it


    // per processor allocregion for strictly local stuff
    localAllocRegionManager = vas->createLocalAllocRegionManager(this, &ba);

    // allocate gm/pm that are not shared; localstrict < cachelinesize
    for (i=0; i<clsizeindex; i++) {
	pm = vas->createPMalloc(
	    i, numaNode, localAllocRegionManager, pool, &ba);
	gm = new(&ba) GMalloc(
	    ListSize(i,i), NumLists(i,i,1), pool, numaNode, pm);
	localGMs[i] = gm;
#ifdef ALLOC_STATS
	initStats[pool]->gmallocs[vp][i] = gm;
#endif /* #ifdef ALLOC_STATS */
    }



#undef PERPROCGM

#ifdef PERPROCGM
    // temporarily testing gmalloc per proc, but still shared pm; we need
    // to allocate now, but init later when we have the pm info we need

    // next we do the local stuff >= secondary cache line which
    // uses shared g/p stuff
    for (i=clsizeindex; i<NUM_SIZES; i++) {
	localGMs[i] = new(&ba) GMalloc;
	initStats[pool]->gmallocs[vp][i] = gm;
    }

    // next we do the global stuff for less than secondary cache line
    // size (everything larger will use the local stuff)
    for (i=GLOBAL_OFF; i<(GLOBAL_OFF+clsizeindex); i++) {
	localGMs[i] = new(&ba) GMalloc;
	initStats[pool]->gmallocs[vp][i] = gm;
    }
#endif /* #ifdef PERPROCGM */

    // we alloc mem-type-dep stuff to use left-over mem if they need it
    vas->useLeftOverMem(&ba);

    if (amFirstProcForNUMANode) {
	// now that everything is initialized, we can store our initInfo
	// stuff in shared array
	(*initInfo[pool])[numaNode] = myInitInfo;
    }

    // the rest of this stuff may depend on other processor's initstuff; so
    // we wait for anything we depend on before using it
    // For now, we only ever need to wait for our numanode primary
    WaitForInitInfo(numaNode, pool, initInfo);
    myInitInfo = (*initInfo[pool])[numaNode];

    // I don't like doing this because this imposes local/node notions
    // outside of this configuration routine, but it seems necessary in
    // some cases (kernel, pinned memory)
    vas->setNodeAllocRegionManager(myInitInfo->allocRegion);


#ifdef PERPROCGM

    // temporarily testing gmalloc per proc, but still shared pm

    // next we do the local stuff >= secondary cache line which
    // uses shared g/p stuff
    for (i=clsizeindex; i<NUM_SIZES; i++) {
	localGMs[i]->init(ListSize(i,i), NumLists(i, i, nodeSize),
			  pool, myInitInfo->pms[i]);
    }


    // next we do the global stuff for less than secondary cache line
    // size (everything larger will use the local stuff)
    for (i=GLOBAL_OFF; i<(GLOBAL_OFF+clsizeindex); i++) {
	const uval idx = i - GLOBAL_OFF;
	localGMs[i]->init(ListSize(idx,i), NumLists(idx, i, nodeSize),
			  pool, myInitInfo->pms[i]);
    }
    
#endif /* #ifdef PERPROCGM */


    // next we do local stuff, up to cache line size; we use localgm stuff
    // that we snarfed away earlier
    for (i=0; i<clsizeindex; i++) {
	/* init both PAGED and PINNED to avoid startup race condition
	 * with trying to access the default one (see
	 * AllocPool::MallocIDToSize)
	 */
	allocLocal[PAGED].mallocIDToSizeMapping[i] = blockSize(i);
	allocLocal[PINNED].mallocIDToSizeMapping[i] = blockSize(i);

	// local strict < cache line size; per processor
	allocLocal[pool].mallocIDToNodeMapping[i] = vp;
	allocLocal[pool].mallocIDIsLocalStrictMapping[i] = 1;

	// we init the local gms now
	//FIXME maa 02/25/04 I think that local strict lml's should
	//use the vp as node, not the numanode. 
	//The LMalloc nodeid must correspond to the address bits in
	//the nodeID part of the address.  But for now, all our
	//Pinned addreses have a zero in that part of the address,
	//so we have no choice - all kernel pinned LMallocs must use
	//a nodeID of zero
	//What happens is that in user processes and kernel paged
	//nodeid == vp for now
	//while in kernel pinned, nodeID == 0
	//so I've left this alone - change to vp must go on otherpool as well
	//when its done.
	lml[i].init(ListSize(i,i), localGMs[i], blockSize(i), i,
		    numaNode, pool);
	if (otherPool != pool) {
	    allocLocal[otherPool].mallocIDToNodeMapping[i] = vp;
	    allocLocal[otherPool].mallocIDIsLocalStrictMapping[i] = 1;
	    allocLocal[otherPool].lml[i].init(ListSize(i,i), localGMs[i],
					      blockSize(i), i, numaNode,
					      otherPool);
	}
    }
    
    // next we do the local stuff >= secondary cache line which is
    // uses shared g/p stuff
    for (i=clsizeindex; i<NUM_SIZES; i++) {
	/* init both PAGED and PINNED
	 * to avoid startup race condition with trying to access the default
	 * one (see AllocPool::MallocIDToSize)
	 */
	allocLocal[PAGED].mallocIDToSizeMapping[i] = blockSize(i);
	allocLocal[PINNED].mallocIDToSizeMapping[i] = blockSize(i);

	// local strict >= cache line size; per numa node
	allocLocal[pool].mallocIDToNodeMapping[i] = numaNode;
	allocLocal[pool].mallocIDIsLocalStrictMapping[i] = 0;

	// note we always use processor's gms; i.e. shared across all procs
	lml[i].init(ListSize(i,i),
		    localGMs[i] != NULL ? localGMs[i] : myInitInfo->gms[i],
		    blockSize(i), i, numaNode, pool);
	if (otherPool != pool) {
	    // local strict >= cache line size; per numa node
	    allocLocal[otherPool].mallocIDToNodeMapping[i] = numaNode;
	    allocLocal[otherPool].mallocIDIsLocalStrictMapping[i] = 0;
	    // note we always use procs's gms; i.e. shared across all procs
	    allocLocal[otherPool].lml[i].
		init(ListSize(i,i),
		     localGMs[i]!=NULL ? localGMs[i] : myInitInfo->gms[i],
		     blockSize(i),i,numaNode,otherPool);
	}
    }

    // next we do the global stuff for less than secondary cache line
    // size (everything larger will use the local stuff)
    for (i=GLOBAL_OFF; i<(GLOBAL_OFF+clsizeindex); i++) {
	const uval idx = i - GLOBAL_OFF;
	/* init both PAGED and PINNED to avoid startup race condition
	 * with trying to access the default one (see
	 * AllocPool::MallocIDToSize)
	 */
	allocLocal[PAGED].mallocIDToSizeMapping[i] = blockSize(idx);
	allocLocal[PINNED].mallocIDToSizeMapping[i] = blockSize(idx);

	// global; per numa node
	allocLocal[pool].mallocIDToNodeMapping[i] = numaNode;
	allocLocal[pool].mallocIDIsLocalStrictMapping[i] = 0;
	// note we always use processor's gms; i.e. shared across all procs
	lml[i].init(ListSize(idx,i),
		    localGMs[i] != NULL ? localGMs[i] : myInitInfo->gms[i],
		    blockSize(idx), i, numaNode, pool);
	if (otherPool != pool) {
	    // global; per numa node
	    allocLocal[otherPool].mallocIDToNodeMapping[i] = numaNode;
	    allocLocal[otherPool].mallocIDIsLocalStrictMapping[i] = 0;
	    // note we always use procs's gms; i.e. shared across all procs
	    allocLocal[otherPool].lml[i].
		init(ListSize(idx,i),
		     localGMs[i]!=NULL ? localGMs[i] : myInitInfo->gms[i],
		     blockSize(idx),i,numaNode,otherPool);
	}
    }

    // finally we initialize the indirection tables for global and global
    // padded
    // for < clsize, global is separate from local
    for (i=0; i<clsizeindex; i++) {
	allocLocal[pool].pGlobal[i] = &allocLocal[pool].lml[GLOBAL_OFF+i];
	allocLocal[otherPool].pGlobal[i] =
	    &allocLocal[otherPool].lml[GLOBAL_OFF+i];
    }
    // for >= clsize, global and padded share lml with local
    for (i=clsizeindex; i<NUM_SIZES; i++) {
	allocLocal[pool].pGlobal[i] = &allocLocal[pool].lml[i];
	allocLocal[otherPool].pGlobal[i] = &allocLocal[otherPool].lml[i];
    }
    // for padded; < real clsize points to same as global for realclsizeindex
    realclsizeindex = index(KernelInfo::SCacheLineSize());
#ifdef NO_PADDED_ALLOCATIONS
    realclsizeindex = 0;
#endif
    for (i=0; i<realclsizeindex; i++) {
	allocLocal[pool].pGlobalPadded[i] =
	    allocLocal[pool].pGlobal[realclsizeindex];
	allocLocal[otherPool].pGlobalPadded[i] =
	    allocLocal[otherPool].pGlobal[realclsizeindex];
    }
    // for >= clsize, global and padded share lml with local
    for (i=realclsizeindex; i<NUM_SIZES; i++) {
	allocLocal[pool].pGlobalPadded[i] = allocLocal[pool].pGlobal[i];
	allocLocal[otherPool].pGlobalPadded[i] =
	    allocLocal[otherPool].pGlobal[i];
    }

    for (i=0; i<numberOfNumaNodes; i++) {
	allocLocal[pool].remoteList[i].init();
	allocLocal[pool].remoteListPtrs[i] = NULL;
	if (otherPool != pool) {
	    allocLocal[otherPool].remoteList[i].init();
	    allocLocal[otherPool].remoteListPtrs[i] = NULL;
	}
    }
    allocLocal[pool].globalRemoteListOfListsPtrs =
	GlobalRemoteListOfListsPtrs[pool];
    allocLocal[pool].fromRemoteListOfLists =
	myInitInfo->fromRemoteListOfLists;
    allocLocal[otherPool].globalRemoteListOfListsPtrs =
	GlobalRemoteListOfListsPtrs[pool];
    allocLocal[otherPool].fromRemoteListOfLists =
	myInitInfo->fromRemoteListOfLists;
}

void
AllocPool::largeFree(void *ptr, uval size)
{
    SysStatus rc = DREF(pageAllocator)->deallocPages((uval)ptr, size);
    tassert(_SUCCESS(rc), err_printf("returned memory wrong pager\n"));
}

void *
AllocPool::largeAlloc(uval size)
{
    uval add;
    uval flags;

    // round up to a multiple of a page
    // ? should we have a call to PageAllocator that returns size
    // actually allocated?
    size = PAGE_ROUND_UP(size);

    if (mypool == PINNED) {
	// FIXME: probably just rid of this stuff when we do pinned
	// separately and differently anyway
	// we can't handle these calls failing, so for now, we block
	//flags = PageAllocator::PAGEALLOC_USERESERVE
	//| PageAllocator::PAGEALLOC_NOBLOCK;
	flags = 0;
    } else {
	flags = 0;
    }
    SysStatus rc = DREF(pageAllocator)->allocPages(add, size, flags);
    if (_FAILURE(rc)) return 0;
    return (void *)add;
}

void *
AllocPool::largeAlloc(uval size, uval &realSize)
{
    uval add;
    uval flags;

    // round up to a multiple of a page
    // ? should we have a call to PageAllocator that returns size
    // actually allocated?
    realSize = PAGE_ROUND_UP(size);

    if (mypool == PINNED) {
	// FIXME: probably just rid of this stuff when we do pinned
	// separately and differently anyway
	// we can't handle these calls failing, so for now, we block
	//flags = PageAllocator::PAGEALLOC_USERESERVE
	//| PageAllocator::PAGEALLOC_NOBLOCK;
	flags = 0;
    } else {
	flags = 0;
    }
    SysStatus rc = DREF(pageAllocator)->allocPages(add, realSize, flags);
    if (_FAILURE(rc)) return 0;
    return (void *)add;
}

void
AllocPool::printStats()
{
    allocStats->printStats();
}


// quick remote free code; this may be small enough, with a bit of tweaking
// to actually inline, but we leave it here for now
void
AllocPool::freeRemote(void *toFree, uval mallocID)
{
    AllocCellPtr fullList;
    uval rc;
    uval numanode;
    AllocCell *cell;

    tassert(mallocID >= GLOBAL_OFF,
	    err_printf("MallocID localstrict; remote free not supported\n"));

    cell = (AllocCell *)toFree;
    cell->mallocID(mallocID);
    /*
     * FIXME: would like this all to work with localstrict stuff where the
     * notion of the nodeid is processor rather than numa node.
     */
    numanode = AllocCell::AddrToNumaNode(uval(cell));
    tassertMsg(numanode < AllocCell::MAX_NUMANODES, "numanode %ld\n", numanode);

    //err_printf("Block %p from %ld freed\n", toFree, numanode);

    rc = remoteList[numanode].push(cell, AllocCell::CELL_MAX_COUNT, fullList);

    if (rc != SyncedCellPtr::SUCCESS) freeRemoteFull(fullList, numanode);
}

// called from lmalloc::free when given a remotely allocated block
void
AllocPool::freeRemoteFull(AllocCellPtr list, VPNum remoteNumaNode)
{
    AllocCell **remote;
    AllocCell *mylist = list.pointer(remoteNumaNode);
    AllocCell *tmp;

    //err_printf("sending remote list (%p) back home to numanode %ld\n",
    //       list.pointer(numaNode), numanode);

    // lazy init of local cache of globalRemoteListOfListPtrs
    // doesn't really matter if there are race conditions here
    if ((remote = remoteListPtrs[remoteNumaNode]) == NULL) {
	remote=remoteListPtrs[remoteNumaNode]=
	    globalRemoteListOfListsPtrs[remoteNumaNode];
    }
    tassertMsg(remote != NULL,
	       "Bad remote ptr for remoteNumaNode %ld\n", remoteNumaNode);

    // now add this list to remote list of lists atomically
    do {
	tmp = *remote;
	mylist->nextList = AllocCellPtr(0, tmp);
    } while (!CompareAndStoreSynced((uval *)remote, uval(tmp), uval(mylist)));
}

uval
AllocPool::checkForRemote()
{
    AllocCell *nextel, *nextlist, *el;
    uval mallocID;
    uval found;

    // quick check for nothing
    if (*fromRemoteListOfLists == NULL) return 0;

    el = (AllocCell *)FetchAndClearSynced((uval *)fromRemoteListOfLists);

    found = (el != NULL);

    //if (found) err_printf("Freeing list of lists %p from remote\n", el);

    while (el) {
	nextlist = (el->nextList).pointer(numaNode);
	// the first element doesn't have a malloc id, as it needs the field
	// to point to the next list; we get the malloc id from MemDesc
	el->mallocID(vAllocServ->mallocID(el));
	//err_printf("Freeing list %p from remote\n", el);
	while (el) {
	    nextel = (el->next).pointer(numaNode);
	    mallocID = el->mallocID();
	    // we assume it's safe to free directly to lmalloc without using
	    // the appropriate front-level interface

	    // FIXME: we got here using the numa node information to tell us
	    // where to do the free, but for the localstrict case, we need to
	    // do better than the right numa node, we need the right processor
	    // this assumes we never get those, but it would be nice to support
	    // local strict remote frees using the same infrastructure

	    //err_printf("Freeing %p from remote\n", el);
	    tassert(vAllocServ->nodeID(el) ==
		    lml[mallocID].getNodeID(),
		    err_printf("freeRemoteMsg: %ld != %ld\n",
			       vAllocServ->nodeID(el),
			       lml[mallocID].getNodeID()));
	    lml[mallocID].lFree(el);
	    el = nextel;
	}
	el = nextlist;
    }
    return found;
}


/*
 * For now, these are for testing, since free won't work if the block was
 * allocated use largeAlloc (i.e., for anything larger than MAX_BLOCK_SIZE
 * We can safely use the default MemDesc class since this allocator is only
 * used for the default class memory.
 */

extern "C" void *k42malloc(uval size);
void *k42malloc(uval size)
{
    tassert(size <= AllocPool::MAX_BLOCK_SIZE,
	    err_printf("size %ld too large\n", size));
    return allocLocal[AllocPool::DEFAULT].allocGlobal(size);
}

extern "C" void k42free(void *p);
void k42free(void *p)
{
    MemDesc *md = MemDesc::FindMemDesc(uval(p));
    tassert(md->fp()->regPtr()->checkMagic(), err_printf("Bad block %p\n",p));
    // Note mallocID is offset from base, not relative to allocation type.
    allocLocal[AllocPool::DEFAULT].byMallocId(md->mallocID())->lFree(p);
}
