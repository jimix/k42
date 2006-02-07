#ifndef __MEM_DESC_H_
#define __MEM_DESC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemDesc.H,v 1.18 2003/05/06 19:32:47 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Memory Descriptors (MemDesc objects) contain information about a
 * page of memory and the blocks allocated from it.
 *
 * To reduce size, we embed MemDesc in a region of contiguous virtual
 * memory.  Each is 64bits and take up the first word of a contiguous
 * region.  Some of the contants declared here really belong as part of
 * the AllocRegion class, but are here for historical and convenience
 * reasons.
 *
 * Lock ordering is somewhat subtle and is better covered in the AllocRegion
 * header file.  In general though, all manipulations are done with the
 * bit-level acquire/release interface, with some read-only information
 * available more directly.
 * **************************************************************************/

#define ALLOC_LOG_PAGE_SIZE	(LOG_PAGE_SIZE)
#define ALLOC_PAGE_MASK         (PAGE_MASK)
#define ALLOC_PAGE_SIZE	        (PAGE_SIZE)
#define ALLOC_LOG_DESC_SIZE     (3)	// 64 bit Bryan type word
#define ALLOC_DESC_SIZE         (uval(1)<<(ALLOC_LOG_DESC_SIZE))
#define ALLOC_LOG_DESC_PER_PAGE (ALLOC_LOG_PAGE_SIZE-ALLOC_LOG_DESC_SIZE)
#define ALLOC_DESC_PER_PAGE     (uval(1)<<ALLOC_LOG_DESC_PER_PAGE)
#define ALLOC_LOG_REGION_PAGES  (ALLOC_LOG_DESC_PER_PAGE)
#define ALLOC_REGION_PAGES      (uval(1)<<(ALLOC_LOG_REGION_PAGES))
#define ALLOC_REGION_PAGES_MASK (ALLOC_REGION_PAGES-1)

// mask that gives you the base address of region given an address
#define _FILTER_ADDR_MASK \
   (~(((ALLOC_REGION_PAGES_MASK<<ALLOC_LOG_PAGE_SIZE)| (ALLOC_PAGE_MASK))))

class DataChunk;
class FirstRegionPage;
class MemDesc;

struct MemDescBits : BitStructure {
    enum { INDEX_BITS = ALLOC_LOG_DESC_PER_PAGE,
	   COUNT_BITS = ALLOC_LOG_PAGE_SIZE-AllocPool::LOG_MIN_BLOCK_SIZE+1,
	   OFFSET_BITS = ALLOC_LOG_PAGE_SIZE,
	   LMALLOC_ID_BITS = AllocPool::LOG_NUM_MALLOCS,
	   NODE_ID_BITS = AllocCell::LOG_MAX_NUMANODES
    };
    enum { INVALID_OFFSET = 1 };	// 1 byte chunk size is not supported

    __BIT_FIELD(64, all, BIT_FIELD_START);

    __BIT_FIELD(INDEX_BITS, nextIndex, BIT_FIELD_START);
    __BIT_FIELD(INDEX_BITS, prevIndex, nextIndex);
    __BIT_FIELD(OFFSET_BITS, freeCellOffset, prevIndex);
    __BIT_FIELD(COUNT_BITS, outstanding, freeCellOffset);
    __BIT_FIELD(LMALLOC_ID_BITS, mallocID, outstanding);
    __BIT_FIELD(NODE_ID_BITS, nodeID, mallocID);
    __BIT_FIELD(2, lockBits, nodeID);

    __LOCK_BIT(lockBits, 1);
    __WAIT_BIT(lockBits, 0);


    // for debugging; counts blocks on free list to compare with stored count
    uval countFreeBlocks(uval page);
    uval checkSanity(uval page);
};

/* Lock protocol is that when manipulating the count of outstanding blocks
 * or the freeCellOffset fields, the memdesc lock must be held.  For
 * manipulating the prev/next index fields, the regiondesc lock must be held.
 * If the two are held together, the region lock must be acquired first.
 *
 * Note that because of the structure all manipulations of the fields must
 * use atomic operations or the acquire/release methods.
 */

// DATA OF MEMDESC, MUST FIT IN 64 BITS
class MemDesc : protected BitBLock<MemDescBits> {
    friend class AllocRegionManager;
    friend class AllocRegionDesc;

    // field accessor functions

    // must hold region lock, we acquire md lock because of other fields
    void nextIndex(uval i) {
	MemDescBits mdb;
	acquire(mdb); mdb.nextIndex(i); release(mdb);
    }
    void prevIndex(uval i) {
	MemDescBits mdb;
	acquire(mdb); mdb.prevIndex(i); release(mdb);
    }
    void prevNextIndex(uval p, uval n) {
	MemDescBits mdb;
	acquire(mdb); mdb.prevIndex(p); mdb.nextIndex(n); release(mdb);
    }

public:

    enum FreeRC { FULL, WASEMPTY, OK };

    // primary method of allocating blocks: prepends allocated blocks
    // to the list given, allocates up to "numBlocks", returning amount
    // actually allocated in "allocated"; for convenience returns the
    // next md on its list in "nextIndex".
    DataChunk *alloc(DataChunk *list, uval numBlocks, uval &allocated,
		     uval &nextIndex, uval &nowEmpty);

    // primary method for free a single block; returns OK if everything
    // ok; returns FULL if this is the last block and hence the memdesc
    // itself needs to be free, but **does __not__ free block**. Returns
    // EMPTY if this is the first block returned, and hence need to move
    // memdesc from empty list to regular list; block again is not freed
    FreeRC freeIfOk(uval block);

    // frees block and tests whether this is the first or last block back
    FreeRC freeAndCheck(uval block);

    // initialize contents of MemDesc that is free
    void initAsFree(uval nextIndex);

    // re-assign unused MemDesc
    void init(uval mallocID, uval nodeID);


    uval getIndex() {
	return ((((uval)this)&(ALLOC_PAGE_MASK))>>ALLOC_LOG_DESC_SIZE);
    }

    // base address of region containing a pointer
    static uval BaseRegAddr(uval v) { return v&_FILTER_ADDR_MASK; }

    // base address of region containing a pointer
    uval baseRegAddr() { return BaseRegAddr((uval)this); }

    // first page of region contains all meta data
    static FirstRegionPage *fp(uval v) {
	return (FirstRegionPage *)BaseRegAddr(v);
    }

    // first page of region containing all meta data
    FirstRegionPage *fp() { return fp((uval)this); }

    static uval GetIndex(uval addr) {
        uval retvalue;
	retvalue = ((addr&(ALLOC_REGION_PAGES_MASK<<(ALLOC_LOG_PAGE_SIZE)))
		>> (ALLOC_LOG_PAGE_SIZE));
	return(retvalue);
    }

    // gets addr of page this memdesc describes
    uval getPage() {
	return baseRegAddr() + getIndex()*ALLOC_PAGE_SIZE;
    }

    // returns the memory descriptor for a page of memory given a
    // memory address in that page
    static MemDesc *FindMemDesc(uval addr) {
        MemDesc *retvalue;
	retvalue = (MemDesc *)(BaseRegAddr(addr) |
			   (GetIndex(addr) << ALLOC_LOG_DESC_SIZE));
	return(retvalue);
    }

    static DataChunk *OffsetToDataChunk(uval offset, uval page) {
	return  (DataChunk *)(offset | page);
    }
    DataChunk *offsetToDataChunk(uval offset) {
	return  (DataChunk *)(offset | getPage());
    }
    uval dataChunkToOffset(DataChunk *dc) {
	return uval(dc) & ALLOC_PAGE_MASK;
    }
    uval dataChunkToPage(DataChunk *dc) {
	return uval(dc) & ~ALLOC_PAGE_MASK;
    }

    DataChunk *invalidChunk() {
	return offsetToDataChunk(MemDescBits::INVALID_OFFSET);
    }

    // fast check for invalid chunk/offset
    static uval invalidChunk(DataChunk *dc) {
	return (uval(dc) & 1);		// invalid if low bit is 1
    }
    static uval invalidOffset(uval offset) {
	return (offset & 1);		// invalid if low bit is 1
    }


    // must hold region lock, read-only so no sync required internally
    uval nextIndex() { return bits.nextIndex(); }
    uval prevIndex() { return bits.prevIndex(); }
    uval mallocID()  { return bits.mallocID(); }
    uval nodeID()    { return bits.nodeID(); }
    uval freeCellOffset()  { return bits.freeCellOffset(); }

};

#endif /* #ifndef __MEM_DESC_H_ */
