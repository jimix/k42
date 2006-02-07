#ifndef __ALLOC_REGION_MANAGER_H_
#define __ALLOC_REGION_MANAGER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocRegionManager.H,v 1.6 2001/10/05 21:47:06 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Main interface for the "Regions" holding the MemDescs for the allocator.
 * Requests to allocate and free lists of blocks are handled through this
 * object.
 *
 * The locking hierarchy is AllocRegionManager, AllocRegionDesc, and then
 * MemDesc, but lower-level locks may be acquired independently if they
 * can guarantee that the object will not be destroyed.  In general the
 * lock of a higher level element protects its fields plus the link
 * fields in lower level elements, but not its own link fields.
 * Allocations always proceed from higher to lower, always acquiring all
 * locks in order.  Frees proceed from lower to higher, acquiring the
 * lowest level lock necessary to perform its task.  Free's are
 * guaranteed that the descriptor for its MemDesc or Region will remain
 * valid as long as the block hasn't been added (since the page or
 * region is partially allocated and hence can't be freed).  In
 * addition, once a page or region is found to be free (or found that
 * this is the last element that will then make it free), and the higher
 * level lock is held for it, it can't change state since allocations
 * need the higher-level lock and because all components are free, no
 * one can have anything to free in it.
 *
 * There is currently expected to be one of these per "Node" plus one per
 * vp for strictly local stuff.
 *
 * Note that on frees, the AllocRegionManager class itself finds the
 * correct AllocRegionManager instance, although it is expected to be
 * called on the right numa node (but not required for correctness).
 * **************************************************************************/

#include "MemDesc.H"
#include "AllocStats.H"

struct AllocRegionDesc;
struct FirstRegionPage;
class  AllocRegionManager;

/*****************************************************************************
 * structure that defines the contents of the first page of a region,
 * the first word is a pointer to the corresponding region descriptor.
 * Organized like a union so that I can index memory descriptors in a
 * natural way (not a union because of object constructor problems).
 * **************************************************************************/

struct FirstRegionPage {
    MemDesc md[ALLOC_REGION_PAGES];
    // the region descriptor
    AllocRegionDesc *regPtr() { return *(AllocRegionDesc **)&md[0]; };
    void regPtr(AllocRegionDesc *rp) { *(AllocRegionDesc **)&md[0] = rp; };
};


/*****************************************************************************
 * Describes the contents of a contiguous region of memory used by the
 * Alloc facility.  Note that the lock protects the MemDesc list and freeList
 * but not the next field (it's logically part of the AllocRegion list).
 * MemDescs can be in one of three states: free, allocated and non-empty, or
 * empty.  Free memdescs are always on the freelist, non-empty ones are
 * always on the appropriate "list", and empty ones are on no list, but are
 * counted by the empty counter.  These rules are always maintained
 * precisely (e.g., a non-empty memdesc is always on the list).
 * **************************************************************************/

struct AllocRegionDesc {
    enum { MAGIC = 0x1234567890abcdef };
    uval  magic;			// magic constant for debugging
    BLock lock;				// protects all but next field, plus
					// protects link fields in mds
    FirstRegionPage *data;
    uval  allocated;			// num memdesc not on free list
    uval  maxAllocated;			// max pages ever allocated from reg
    uval  empty;			// num memdesc allocated but empty
    uval  list[AllocPool::NUM_MALLOCS];	// per-alloc circ lists used MemDesc
    uval  freeList;			// free MemDesc
    AllocRegionDesc *next;		// protected by AllocRegion lock
    AllocRegionManager    *myRegionManager; // set of regions it's part of
    uval countFreeMemDescs();		// for debugging
    uval countMemDescsOnLists();	// for debugging

public:
    void init(uval addr);		// actually initialize to real mem
    uval checkMagic() { return magic == MAGIC; }
    uval checkSanity();
    uval memDescIsOnList(MemDesc *mdtarget, uval mallocID);
    uval memDescIsOnFreeList(MemDesc *mdtarget);
    static void removeFromDLList(FirstRegionPage *frp, uval &list,MemDesc *md);
    static void addToDLList(FirstRegionPage *frp, uval &list, MemDesc *md);

    static inline MemDesc *indexToMD(FirstRegionPage *frp, uval mdIndex) {
	if (mdIndex) {
	    return &(frp->md[mdIndex]);
	} else {
	    return 0;
	}
    }

};


class AllocRegionManager {
    AllocPool       *ap;		// my pool (will go away)
    AllocRegionDesc *regionList;	// list of allocated regions
    AllocRegionDesc *freeRegionDesc;	// free regiondescs

    // starting point for allocating blocks of given id
    AllocRegionDesc *hint[AllocPool::NUM_MALLOCS]; // per-alloc start regions

    // starting point for looking for free MemDescs
    AllocRegionDesc *hintFree;		// start for free memdesc

    BLock lock;				// protects this plus "next" in
					// AllocRegionDesc (logically part
					// of the lists above)

    static AllocStats *stats(uval pool) { return allocLocal[pool].getStats(); }
    AllocStats *stats()                 { return ap->getStats(); }

    void invalidateHints(AllocRegionDesc *reg);	// remove stale hints
    uval findRegion(AllocRegionDesc *target); // check if we have it; debug

    AllocRegionDesc *createRegion();

    void tryFreeRegion(AllocRegionDesc *reg);

public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba) {
	return ba->alloc(size);
    }
    AllocRegionManager(AllocPool *ap);
    void initRegionDescriptors(uval buf, uval len);

    // allocate numBlocks and return as list
    DataChunk *alloc(uval numBlocks, uval mallocID);

    /* free single block: note that function is static and determines correct
     * AllocRegionManager instance from the block's address.  pool provided
     * just for debugging/stats; should cost little if not used
     */
    static void free(uval block, uval mallocID, uval pool) {
	MemDesc            *md;
	MemDesc::FreeRC     mdrc;
	stats(pool)->incCellsFreed(mallocID);
	/* ask block's memdesc to free block; if this would be the last one,
	 * the block will not be freed (in order to guarantee the memdesc
	 * won't be freed by someone else, as well as the region it is in),
	 * and we will try to free the block and the memdesc together.
	 */
	md = MemDesc::FindMemDesc(block);
	tassert(md->mallocID() == mallocID,
		err_printf("mallocid mismatch: %ld != %ld, %lx \n",
			   md->mallocID(), mallocID, block));
	mdrc = md->freeIfOk(block);
	if (mdrc == MemDesc::OK) {
	    // free went ok, nothing else to do
	    return;
	}
	slowFree(block, mallocID, md, pool);
    }
    static void slowFree(uval block, uval mallocID, MemDesc *md, uval pool);
};




#endif /* #ifndef __ALLOC_REGION_MANAGER_H_ */
