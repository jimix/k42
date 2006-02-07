/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemDescKern.C,v 1.5 2002/06/21 01:40:40 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Memory Descriptors (MemDesc objects) contain information about a physical
 * (kernel pinned) page of memory and the block allocated from it.
 * **************************************************************************/

#include "kernIncs.H"
#include <mem/PageAllocatorKernPinned.H>
#include <alloc/DataChunk.H>
#include <alloc/AllocPool.H>
#include "MemDescKern.H"
#include <sys/KernelInfo.H>

/* make this page contains block of for mallocID, with the list of blocks
 * ready to be allocated all set up.
 */
void
MemDescKern::init(uval mallocid, uval nodeid)
{
    init(mallocid, nodeid, getPage());
}

/* This function is also used by the bootTime memdescs, and hence cannot
 * do anything that is dependent on getting its address from base framedesc
 * that it is supposed to be in, or anything else that is dependent on the
 * framedesc being valid
 */
void
MemDescKern::init(uval mallocid, uval nodeid, uval page)
{
    MemDescKernBits mdb;
    uval bSize;
    uval numBlocks;
    uval addr;

    tassertMsg(page == frameAddress, "MemDesc mapping fumble\n");
    
    bSize     = AllocPool::MallocIDToSize(mallocid);
    numBlocks = ALLOC_PAGE_SIZE / bSize;

    tassert((bSize & (sizeof(DataChunk)-1)) == 0,
	    err_printf("size alignment not supported\n"));

    mdb.freeCellOffset(0);		// first block at offset 0
    mdb.outstanding(0);			// none allocated
    mdb.mallocID(mallocid);
    mdb.nodeID(nodeid);
    mdb.lockBits(0);			// all lock bits clear
    mdb.filler(0);			// just for cleaniness

    nextIndex(0);			// not on any list to begin with
    prevIndex(0);			// not on any list to begin with

    BitBLock<MemDescKernBits>::init(mdb);

    /* freeCellOffset starts off pointing to first address of page */
    for (addr = page; numBlocks > 0; numBlocks--, addr += bSize) {
	((DataChunk *)addr)->next = (DataChunk *) (addr + bSize);
    }
    ((DataChunk *)(addr - bSize))->next = InvalidChunk(page);
}


DataChunk *
MemDescKern::alloc(DataChunk *list, uval numBlocks, uval &allocated,
		   uval &next, uval &nowEmpty, uval page)
{
    MemDescKernBits mdb;
    uval numreq, numallocated;
    DataChunk *head, *curr, *prev;

    numreq = numBlocks;

    acquire(mdb);			// from template parent class

    // walk list of free blocks until we have enough or reached the end
    // note that end is marked by INVALID_OFFSET

    head = OffsetToDataChunk(mdb.freeCellOffset(), page);
    curr = head;
    prev = InvalidChunk(page);
    while (numBlocks > 0 && !invalidChunk(curr)) {
	numBlocks--;
	prev = curr;
	curr = curr->next;
    }
    if (!invalidChunk(prev)) {
	// attach new blocks to "list" and reset md freelist
	prev->next = list;
	list = head;
	mdb.freeCellOffset(dataChunkToOffset(curr));
    }
    numallocated = numreq - numBlocks;
    mdb.outstanding(mdb.outstanding() + numallocated);
    next = nextIndex();			// inherited opt from MemDesc, no value

    tassert(mdb.checkSanity(page), err_printf("mdb insane\n"));

    release(mdb);			// from template parent class

    allocated = numallocated;
    nowEmpty = invalidChunk(curr);
    return list;
}

MemDescKern::FreeRC
MemDescKern::freeIfOk(uval block)
{
    MemDescKernBits  mdb;
    DataChunk   *dc;
    uval         outstanding;
    uval         headOffset;

    acquire(mdb);			// from template parent class

    outstanding = mdb.outstanding();
    if (outstanding == 1) {
	// this looks like the last block; don't free, just return FULL
	release(mdb);
	return FULL;
    }
    headOffset = mdb.freeCellOffset();
    if (invalidOffset(headOffset)) {
	// this looks like the first block back; don't free, just return EMPTY
	release(mdb);
	return WASEMPTY;
    }
    dc = (DataChunk *)block;
    dc->next = OffsetToDataChunk(headOffset, dataChunkToPage(dc));
    mdb.freeCellOffset(dataChunkToOffset(dc));
    mdb.outstanding(--outstanding);

    tassert(mdb.checkSanity(getPage()), err_printf("mdb insane\n"));

    release(mdb);

    return OK;
}

MemDescKern::FreeRC
MemDescKern::freeAndCheck(uval block)
{
    MemDescKernBits  mdb;
    DataChunk   *dc;
    uval         outstanding;
    uval         headOffset;

    acquire(mdb);			// from template parent class

    outstanding = mdb.outstanding();
    dc = (DataChunk *)block;
    headOffset = mdb.freeCellOffset();
    dc->next = offsetToDataChunk(headOffset);
    mdb.freeCellOffset(dataChunkToOffset(dc));
    mdb.outstanding(--outstanding);

    tassert(mdb.checkSanity(getPage()), err_printf("mdb insane\n"));

    release(mdb);

    if (outstanding == 0) {
	return FULL;
    } else if (invalidOffset(headOffset)) {
	return WASEMPTY;
    } else {
	return OK;
    }
}

uval
MemDescKernBits::countFreeBlocks(uval page)
{
    uval numBlocks;
    DataChunk *curr;

    curr = MemDescKern::OffsetToDataChunk(freeCellOffset(), page);
    numBlocks = 0;
    while (!MemDescKern::invalidChunk(curr)) {
	numBlocks++;
	curr = curr->next;
    }

    return numBlocks;
}

uval
MemDescKernBits::checkSanity(uval page)
{
    uval sane = 1;

    if (KernelInfo::ControlFlagIsSet(KernelInfo::NO_ALLOC_SANITY_CHECK)) {
	return sane;
    }

    uval freeBlocks = countFreeBlocks(page);
    uval numBlocks = ALLOC_PAGE_SIZE / AllocPool::MallocIDToSize(mallocID());

    // code this way to make added more checks easier
    sane = sane && (freeBlocks == (numBlocks - outstanding()));
    tassertWrn(sane, "MDB: %lx - %ld != (%ld - %ld)\n", page,
	       freeBlocks, numBlocks, outstanding());

    return sane;
}
