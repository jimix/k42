#ifndef __MEM_DESC_KERN_H_
#define __MEM_DESC_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemDescKern.H,v 1.6 2003/05/06 19:36:36 marc Exp $
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
 * The Kern version of MemDesc uses a hash lookup to get from address
 * to MemDesc.  It uses pointers as indexes.
 * More space than user-level is required, since
 * the MemDesc isn't restricted to cover a page in a small region of
 * memory.
 *
 * Apart from the next/prev and hashNext fields, everything else is
 * roughly similar to the regular MemDesc version.
 *
 * Lock ordering is somewhat subtle and is better covered in the AllocRegion
 * header file.  In general though, all manipulations are done with the
 * bit-level acquire/release interface, with some read-only information
 * available more directly.
 * **************************************************************************/

#include <alloc/MemDesc.H>

class DataChunk;

struct MemDescKernBits : BitStructure {
    enum { COUNT_BITS = ALLOC_LOG_PAGE_SIZE-AllocPool::LOG_MIN_BLOCK_SIZE+1,
	   OFFSET_BITS = ALLOC_LOG_PAGE_SIZE,
	   LMALLOC_ID_BITS = AllocPool::LOG_NUM_MALLOCS,
	   NODE_ID_BITS = AllocCell::LOG_MAX_NUMANODES,
    };
    enum { INVALID_OFFSET = 1 };	// 1 byte chunk size is not supported

    __BIT_FIELD(64, all, BIT_FIELD_START);

    __BIT_FIELD(OFFSET_BITS, freeCellOffset, BIT_FIELD_START);
    __BIT_FIELD(COUNT_BITS, outstanding, freeCellOffset);
    __BIT_FIELD(LMALLOC_ID_BITS, mallocID, outstanding);
    __BIT_FIELD(NODE_ID_BITS, nodeID, mallocID);
    __BIT_FIELD(2, lockBits, nodeID);
    __BIT_FIELD(64 - (OFFSET_BITS+COUNT_BITS+LMALLOC_ID_BITS+
		      NODE_ID_BITS+2), filler, lockBits);
    __LOCK_BIT(lockBits, 1);
    __WAIT_BIT(lockBits, 0);


    // for debugging; counts blocks on free list to compare with stored count
    uval countFreeBlocks(uval page);
    uval checkSanity(uval page);
};

/* Lock protocol is that when manipulating the count of outstanding blocks
 * or the freeCellOffset fields, the memdesc lock must be held.  For
 * manipulating the prev/next index fields, the region lock must be held.
 * If the two are held together, the region lock must be acquired first.
 * Note, reference to region lock is to whatever structure is the head of
 * the list this memdesc is on.
 */

class AllocRegionManagerKern;

/*
 * we assume we can atomically read/write these fields
 * independent of the other fields (protection is provided by "region" lock
 */
class MemDescKern : protected BitBLock<MemDescKernBits> {
    friend class AllocRegionManagerKern;

protected:
    uval nextIdx;
    uval prevIdx;
    uval frameAddress;

    // must hold region lock
    void nextIndex(uval i)             { nextIdx = i; }
    void prevIndex(uval i)             { prevIdx = i; }
    void prevNextIndex(uval p, uval n) { prevIdx = p; nextIdx = n; }

public:

    enum FreeRC { FULL, WASEMPTY, OK };

    /* primary method of allocating blocks: prepends allocated blocks
     * to the list given, allocates up to "numBlocks", returning amount
     * actually allocated in "allocated"; for convenience returns the
     * next md on its list in "nextIndex".  Two versions to handle
     * boot-time stuff where getting page addr is different
     */
    DataChunk *alloc(DataChunk *list, uval numBlocks, uval &allocated,
		     uval &nextIndex, uval &nowEmpty, uval page);
    DataChunk *alloc(DataChunk *list, uval numBlocks, uval &allocated,
		     uval &nextIndex, uval &nowEmpty) {
	return alloc(list,numBlocks,allocated,nextIndex,nowEmpty,getPage());
    }

    // primary method for free a single block; returns OK if everything
    // ok; returns FULL if this is the last block and hence the memdesc
    // itself needs to be free, but **does __not__ free block**. Returns
    // EMPTY if this is the first block returned, and hence need to move
    // memdesc from empty list to regular list; block again is not freed
    FreeRC freeIfOk(uval block);

    // frees block and tests whether this is the first or last block back
    FreeRC freeAndCheck(uval block);

    // initialize a MemDesc, provide addr if known
    void init(uval mallocID, uval nodeID);
    void init(uval mallocID, uval nodeID, uval addr);

    uval getIndex() {
	return uval(this);
    }

    // gets addr of page this memdesc describes
    uval getPage() {
	return uval(frameAddress);
    }

    // returns the memory descriptor for a page of memory given a
    // memory address in that page (not realaddr)
    static MemDescKern *AddrToMD(uval addr) {
	return (MemDescKern *)(DREFGOBJK(ThePinnedPageAllocatorRef)
			       ->getMemDesc(addr));
    }

    // returns memdesc given the index for the memdesc
    static inline MemDescKern *IndexToMD(uval mdIndex) {
	return (MemDescKern *)(mdIndex);
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
    DataChunk *InvalidChunk(uval page) {
	return OffsetToDataChunk(MemDescBits::INVALID_OFFSET, page);
    }

    // fast check for invalid chunk/offset
    static uval invalidChunk(DataChunk *dc) {
	return (uval(dc) & 1);		// invalid if low bit is 1
    }
    static uval invalidOffset(uval offset) {
	return (offset & 1);		// invalid if low bit is 1
    }


    // must hold region lock, read-only so no sync required internally
    uval nextIndex()       { return nextIdx; }
    uval prevIndex()       { return prevIdx; }
    uval mallocID()        { return bits.mallocID(); }
    uval nodeID()          { return bits.nodeID(); }
    uval freeCellOffset()  { return bits.freeCellOffset(); }

};
#endif /* #ifndef __MEM_DESC_KERN_H_ */
