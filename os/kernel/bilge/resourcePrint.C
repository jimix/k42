/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: resourcePrint.C,v 1.12 2005/06/27 06:15:52 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test place for resource printing
 * will be replaced by appropriate object interface and x objects
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernUnpinned.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PM.H"
#include "mem/PMRoot.H"
#include "mem/PMLeafChunk.H"

void resourcePrint(uval all=1)
{
    uval pinavail, avail;
    DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(pinavail);
    DREFGOBJ(ThePageAllocatorRef)->getMemoryFree(avail);
    cprintf("Available Memory Pinned %lx Paged %lx\n", pinavail, avail);
    cprintf("PMRoot Free List Stats:\n");
    ((PMRoot *)(DREFGOBJK(ThePMRootRef)))->printFreeListStats();
    if (all) {
	DREFGOBJK(ThePMRootRef)->print();
    }
    return;
}

void resourcePrintFragmentation()
{
    static uval printCount = 1;
    cprintf("PMLeafChunk free stats\n");
    cprintf("  Print count: %lu\n", printCount);
    printCount++;
    cprintf("  Chunk size: %li\n", PMLeafChunk::chunkSize);
    cprintf("  Single Page Frees: %li\n", PMLeafChunk::freedSingleCount);
    cprintf("  Chunk Page Frees: %li\n", PMLeafChunk::freedChunkCount);
    cprintf("Flushing free frame lists\n");
    ((PMRoot *)(DREFGOBJK(ThePMRootRef)))->pushAllFreeFramesAllReps();
    DREFGOBJK(ThePinnedPageAllocatorRef)->printMemoryFragmentation();
}
