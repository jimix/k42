#ifndef __ALLOC_REGION_MANAGER_KERN_H_
#define __ALLOC_REGION_MANAGER_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocRegionManagerKern.H,v 1.5 2003/05/06 19:36:36 marc Exp $
 *****************************************************************************/

/*****************************************************************************
 * Main interface for the "Regions" holding the MemDescKerns for the allocator.
 * Requests to allocate and free lists of blocks are handled through this
 * object.
 *
 * The locking hierarchy is AllocRegionManagerKern and then
 * MemDesc, but lower-level locks may be acquired independently if they
 * can guarantee that the object will not be destroyed.  In general the
 * lock of a higher level element protects its fields plus the link
 * fields in lower level elements, but not its own link fields.
 * Allocations always proceed from higher to lower, always acquiring all
 * locks in order.  Frees proceed from lower to higher, acquiring the
 * lowest level lock necessary to perform its task.  Free's are
 * guaranteed that the descriptor for its MemDesc will remain
 * valid as long as the block hasn't been added (since the page or
 * region is partially allocated and hence can't be freed).  In
 * addition, once a page is found to be free (or found that
 * this is the last element that will then make it free), and the higher
 * level lock is held for it, it can't change state since allocations
 * need the higher-level lock and because all components are free, no
 * one can have anything to free in it.
 *
 * There is currently expected to be one of these per "Node"
 *
 * Note that on frees, the AllocRegionManagerKern class itself finds the
 * correct AllocRegionManagerKern instance, although it is expected to be
 * called on the right numa node (but not required for correctness).
 * **************************************************************************/

#include "MemDescKern.H"
#include <alloc/AllocStats.H>

class VAllocServicesKern;

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

struct AllocRegionManagerKern {
    BLock lock;				// protects all but next field, plus
					// protects link fields in mds
    uval  allocated;			// num memdesc on one of the lists
    uval  maxAllocated;			// max pages ever allocated from reg
    uval  empty;			// num memdesc allocated but empty
    uval  numaNode;			// node number for this allocator
    uval  list[AllocPool::NUM_MALLOCS];	// per-alloc circ lists used MemDesc

    uval  countMemDescsOnLists();	// for debugging

    static AllocPool  *MyAP()   { return &allocLocal[AllocPool::PINNED]; }
    // FIXME: double-check this gets compiled away with nodebug
    static AllocStats *Stats()  { return MyAP()->getStats(); }

    uval memDescIsOnList(MemDescKern *mdtarget, uval mallocID);

    static void RemoveFromDLList(uval &list, MemDescKern *md);
    static void AddToDLList(uval &list, MemDescKern *md);


public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba) {
	return ba->alloc(size);
    }
    AllocRegionManagerKern();
    uval checkSanity();

    // allocate numBlocks and return as list
    DataChunk *alloc(uval numBlocks, uval mallocID);

    /* free single block: note that function is static and determines correct
     * AllocRegionManager instance from the block's address.  pool provided
     * just for debugging/stats; should cost little if not used
     */
    static void free(uval block, uval mallocID) {
	MemDescKern         *md;
	MemDescKern::FreeRC  mdrc;
	Stats()->incCellsFreed(mallocID);
	/* ask block's memdesc to free block; if this would be the last one,
	 * the block will not be freed (in order to guarantee the memdesc
	 * won't be freed by someone else, as well as the region it is in),
	 * and we will try to free the block and the memdesc together.
	 */
	md = MemDescKern::AddrToMD(block);
	tassert(md->mallocID() == mallocID,
		err_printf("mallocid mismatch: %ld != %ld, %lx \n",
			   md->mallocID(), mallocID, block));
	mdrc = md->freeIfOk(block);
	if (mdrc == MemDescKern::OK) {
	    // free went ok, nothing else to do
	    return;
	}
	slowFree(block, mallocID, md);
    }
    static void slowFree(uval block, uval mallocID, MemDescKern *md);
};




#endif /* #ifndef __ALLOC_REGION_MANAGER_KERN_H_ */
