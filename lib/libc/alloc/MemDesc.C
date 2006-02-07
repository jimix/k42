/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemDesc.C,v 1.20 2002/05/09 15:00:16 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Memory Descriptors (MemDesc objects) contain information about a physical
 * page of memory and the block allocated from it.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DataChunk.H"
#include "AllocPool.H"
#include "MemDesc.H"
#include <sys/KernelInfo.H>

// init for free list
void
MemDesc::initAsFree(uval next)
{
    MemDescBits mdb;

    mdb.all(0);				// all fields zero by default
    mdb.mallocID(~uval(0));		// indicate uninited with all 1
    mdb.nextIndex(next);
    BitBLock<MemDescBits>::init(mdb);
}

/* make this page contains block of for mallocID, with the list of blocks
 * ready to be allocated all set up.
 */
void
MemDesc::init(uval mallocid, uval nodeid)
{
    MemDescBits mdb;
    uval bSize;
    uval numBlocks;
    uval addr;

    if (mallocID() == mallocid && nodeID() == nodeid) {
	// already initialized to the right values
	return;
    }

    bSize     = AllocPool::MallocIDToSize(mallocid);
    numBlocks = ALLOC_PAGE_SIZE / bSize;
    addr      = getPage();

    tassert((bSize & (sizeof(DataChunk)-1)) == 0,
	    err_printf("size alignment not supported\n"));

    mdb.all(0);				// all fields zero by default
    mdb.mallocID(mallocid);
    mdb.nodeID(nodeid);

    BitBLock<MemDescBits>::init(mdb);

    /* freeCellOffset starts off pointing to first address of page */
    for (; numBlocks > 0; numBlocks--, addr += bSize) {
	((DataChunk *)addr)->next = (DataChunk *) (addr + bSize);
    }
    ((DataChunk *)(addr - bSize))->next = invalidChunk();
}


DataChunk *
MemDesc::alloc(DataChunk *list, uval numBlocks, uval &allocated, uval &next,
	       uval &nowEmpty)
{
    MemDescBits mdb;
    uval numreq, numallocated;
    DataChunk *head, *curr, *prev;

    numreq = numBlocks;

    acquire(mdb);			// from template parent class

    // walk list of free blocks until we have enough or reached the end
    // note that end is marked by INVALID_OFFSET

    head = offsetToDataChunk(mdb.freeCellOffset());
    curr = head;
    prev = invalidChunk();
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
    next = mdb.nextIndex();

    tassert(mdb.checkSanity(getPage()), err_printf("mdb insane\n"));

    release(mdb);			// from template parent class

    allocated = numallocated;
    nowEmpty = invalidChunk(curr);
    return list;
}

MemDesc::FreeRC
MemDesc::freeIfOk(uval block)
{
    MemDescBits  mdb;
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
//    dc->next = offsetToDataChunk(headOffset);
    dc->next = OffsetToDataChunk(headOffset, dataChunkToPage(dc));
    mdb.freeCellOffset(dataChunkToOffset(dc));
    mdb.outstanding(--outstanding);

    tassert(mdb.checkSanity(getPage()), err_printf("mdb insane\n"));

    release(mdb);

    return OK;
}

MemDesc::FreeRC
MemDesc::freeAndCheck(uval block)
{
    MemDescBits  mdb;
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
MemDescBits::countFreeBlocks(uval page)
{
    uval numBlocks;
    DataChunk *curr;

    curr = MemDesc::OffsetToDataChunk(freeCellOffset(), page);
    numBlocks = 0;
    while (!MemDesc::invalidChunk(curr)) {
	numBlocks++;
	curr = curr->next;
    }

    return numBlocks;
}

uval
MemDescBits::checkSanity(uval page)
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
