/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocRegionManagerKern.C,v 1.6 2004/07/23 21:53:39 marc Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include <mem/PageAllocatorKernPinned.H>
#include <alloc/DataChunk.H>
#include "AllocRegionManagerKern.H"
#include "MemDescKern.H"
#include "VAllocServicesKern.H"
#include <sys/KernelInfo.H>

#ifdef ALLOC_STATS
#define ALLOC_GIVEBACK 0
#else
#define ALLOC_GIVEBACK 1
#endif

// true if pages are returned when free
uval AllocRegionManagerKern_GiveBack = ALLOC_GIVEBACK;


/* Allocate some blocks, return as a list
 *
 * This contains the main logic for allocated chunks from memdesc and regions
 */
DataChunk *
AllocRegionManagerKern::alloc(uval numBlocks, uval mallocID)
{
    MemDescKern *md, *startmd;
    DataChunk   *dclist;
    uval         addr;
    SysStatus    rc;
    uval         nextIndex;
    uval         numAllocated, isEmpty;
    uval         nodeID;
    VAllocServicesKern *vas;

    /* if we are still in early boot phase, need to use special allocator
     * method
     */
    vas = (VAllocServicesKern *)(MyAP()->getVAllocServ());

    nodeID = MyAP()->mallocIDToNode(mallocID);

    /* The lock protects the list of regions and each region's next pointer
     * allowing us to safely traverse the list, but we still need the locks
     * in the individual regions to walk the memdesc list, as well as the
     * individual memdesc locks to allocate from them.  We hope in general to
     * be able to allocate many blocks at a time with each lock acquisition.
     */
    lock.acquire();

    Stats()->incNumAllocCalls(mallocID);
    Stats()->incCellsAllocated(mallocID, numBlocks);

    // we start by looking for already partially allocated pages in the
    // list of memdescs for the mallocid

    tassert(checkSanity(), err_printf("oops\n"));

    dclist = 0;

 MDSearch:

    // try to find something free with this block size
    startmd = MemDescKern::IndexToMD(list[mallocID]);

    //err_printf("Trying from md %lx/%ld\n", startmd, list[mallocID]);

    md = startmd;
    if (md) do {
	Stats()->incMemDescsSearched(mallocID);
	tassert((md->mallocID() == mallocID), err_printf("woops\n") );
	if (md->nodeID() != nodeID) {
	    // I don't think this should ever happen with current setup
	    err_printf("(%ld/%ld) skipping %ld\n",
		       mallocID, nodeID, md->nodeID());
	    nextIndex = md->nextIndex();
	} else {
	    dclist=md->alloc(dclist,numBlocks,numAllocated,nextIndex,isEmpty);
	    numBlocks -= numAllocated;
	    if (isEmpty) {
		// remove from alloc list
		RemoveFromDLList(list[mallocID], md);
		empty++;
		Stats()->incMemDescsEmptied(mallocID);
		// if we just removed our starting point, restart search
		if (startmd == md) {
		    // hate gotos, but much easier
		    goto MDSearch;
		} else {
		    md = 0;		// mark invalid
		}
	    }
	    if (numBlocks == 0) {
		// we're all done for this call, release locks and return
		tassert(checkSanity(), err_printf("oops\n"));
		// update list so we start from here next time
		if (md) {
		    list[mallocID] = md->getIndex();
		} else {
		    // md was just removed, start with next
		    list[mallocID] = nextIndex;
		}
		lock.release();
		//err_printf("All done - 1\n");
		return dclist;
	    }
	}
	tassert(checkSanity(), err_printf("oops\n"));
	// still need more, so try next md in this region
	md = MemDescKern::IndexToMD(nextIndex);
	// check for wrap around
    } while (md != startmd);

    // didn't get enough blocks, so allocate new pages until we get enough
    while (1) {

	tassert(checkSanity(), err_printf("oops\n"));

	rc=DREF(MyAP()->getPageAllocator())->allocPages(addr,ALLOC_PAGE_SIZE);
	passert(_SUCCESS(rc), err_printf(" woops\n"));

	Stats()->incMemDescsAllocated(mallocID);

	allocated++;
	if (allocated > maxAllocated) {
	    tassert(allocated == maxAllocated + 1, err_printf("oops\n"));
	    maxAllocated++;
	    if (!AllocRegionManagerKern_GiveBack) {
		// global stats only make sense if regions not given back
		Stats()->incMaxMemDescAllocated();
	    }
	}
	md = MemDescKern::AddrToMD(addr);
	md->init(mallocID, nodeID, addr); // init for given mallocID/nodeID

	// now allocate blocks
	dclist = md->alloc(dclist, numBlocks, numAllocated, nextIndex,isEmpty);
	if (isEmpty) {
	    // nothing left
	    Stats()->incMemDescsEmptied(mallocID);
	    empty++;
	} else {
	    // still something left, so put on list
	    AddToDLList(list[mallocID], md);
	}

	numBlocks -= numAllocated;
	if (numBlocks == 0) {
	    // we're all done for this call, release all locks and return
	    tassert(checkSanity(), err_printf("oops\n"));
	    lock.release();
	    return dclist;
	}

	// still need more, so allocate another page
    }

    passert(0, err_printf("Should never get here\n"));
    lock.release();
    return 0;
}



/* Free a single block
 *
 * This contains the main logic for freeing a block, as well as freing
 * memdesc and regions.
 *
 * The general strategy is to try to lock only the most directly related
 * object.  First just the memdesc; but if the memdesc would become totally
 * free as a result, then we lock the regionmanager and try to free the
 * memdesc.
 */
/* static */ void
AllocRegionManagerKern::slowFree(uval block, uval mallocID, MemDescKern *md)
{
    MemDescKern::FreeRC      mdrc;
    AllocRegionManagerKern  *allocRegion;
    SysStatus                rc;
    VAllocServicesKern      *vas;
    uval                     addr;

    // this is either the first block back or the last block back, try again
    // but with the region lock held

    vas = (VAllocServicesKern *)(MyAP()->getVAllocServ());
    allocRegion = vas->getAllocRegion(md);
    tassert(allocRegion != NULL, err_printf("oops\n"));

    allocRegion->lock.acquire();
    tassert(allocRegion->checkSanity(), err_printf("oops\n"));

    // now try to free block again, requesting md to do so even if last one
    // and to indicate if first or last block back
    mdrc = md->freeAndCheck(block);
    if (mdrc == MemDescKern::OK) {
	// no longer last or first block, so we're all done
	tassert(allocRegion->memDescIsOnList(md, mallocID),
		err_printf("oops\n"));
	tassert(allocRegion->checkSanity(), err_printf("oops\n"));
	//err_printf("md no longer free\n");
	allocRegion->lock.release();
	return;
    }

    // md is now fully free or just changed to non-empty,
    // so dequeue from neighbours (if own neighbour, list is now empty)

    if (mdrc == MemDescKern::WASEMPTY) {
	// transition from empty to non-empty; put back on list
	tassert(!allocRegion->memDescIsOnList(md, mallocID),
		err_printf("oops\n"));
	AddToDLList(allocRegion->list[mallocID], md);
	Stats()->incMemDescsUnemptied(mallocID);
	allocRegion->empty--;
    } else {
	// transition from non-empty to full, free it
	tassert(mdrc == MemDescKern::FULL, err_printf("oops\n"));
	tassert(allocRegion->memDescIsOnList(md, mallocID),
		err_printf("oops\n"));
	RemoveFromDLList(allocRegion->list[mallocID], md);
	addr = md->getPage();

	// give memDesc back first - this simplifies locking in the
	// memDesc hash since no one can reuse the memDesc until
	// the associated frame is freed below
	DREFGOBJK(ThePinnedPageAllocatorRef)->freeMemDesc(addr);
	
	// give memory back to system
	rc = DREF(MyAP()->getPageAllocator())->deallocPages(addr,
							    ALLOC_PAGE_SIZE);
	tassert(_SUCCESS(rc), err_printf(" woops\n"));

	allocRegion->allocated--;
	Stats()->incMemDescsFreed(mallocID);
    }

    // nothing more to do
    tassert(allocRegion->checkSanity(), err_printf("oops\n"));
    allocRegion->lock.release();
}


/* static */ void
AllocRegionManagerKern::RemoveFromDLList(uval &list, MemDescKern *md)
{
    uval         mdIndex, prevIndex, nextIndex;
    MemDescKern *prevmd, *nextmd;

    mdIndex = md->getIndex();
    nextIndex = md->nextIndex();
    prevIndex = md->prevIndex();

    if (nextIndex == mdIndex) {
	// only md on the list
	tassert(prevIndex == mdIndex, err_printf("md queue bug\n"));
	tassert(list == mdIndex, err_printf("reg list bug\n"));
	list = 0;
    } else {
	// update neighbours
	nextmd = MemDescKern::IndexToMD(nextIndex);
	prevmd = MemDescKern::IndexToMD(prevIndex);
	nextmd->prevIndex(prevIndex);
	prevmd->nextIndex(nextIndex);
	if (list == mdIndex) {
	    list = nextIndex;
	}
    }
}

/* static */ void
AllocRegionManagerKern::AddToDLList(uval &list, MemDescKern *md)
{
    uval         mdIndex, prevIndex, nextIndex;
    MemDescKern *prevmd, *nextmd;

    mdIndex = md->getIndex();
    // add to queue after starting point
    prevIndex = list;
    prevmd = MemDescKern::IndexToMD(prevIndex);
    if (prevmd == NULL) {
	// empty, so make circular queue of one
	md->prevNextIndex(mdIndex, mdIndex);
    } else {
	// insert after prev
	nextIndex = prevmd->nextIndex();
	nextmd = MemDescKern::IndexToMD(nextIndex);
	prevmd->nextIndex(mdIndex);
	nextmd->prevIndex(mdIndex);
	md->prevNextIndex(prevIndex, nextIndex);
    }
    list = mdIndex;			// make new md "head"
}

AllocRegionManagerKern::AllocRegionManagerKern()
{
    uval i;

    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	list[i] = 0;
    }
    allocated = 0;
    maxAllocated = 0;
    empty = 0;
}

uval
AllocRegionManagerKern::countMemDescsOnLists()
{
    MemDescKern     *md, *mdstart;
    uval             mdIndex;
    uval             count;
    uval             i;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    count = 0;
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	mdIndex = list[i];
	mdstart = MemDescKern::IndexToMD(mdIndex);
	md = mdstart;
	if (md != NULL) do {
	    count++;
	    mdIndex = md->nextIndex();
	    md = MemDescKern::IndexToMD(mdIndex);
	} while (md != mdstart);
    }

    return count;
}

uval
AllocRegionManagerKern::memDescIsOnList(MemDescKern *mdtarget, uval mallocID)
{
    MemDescKern  *md, *mdstart;
    uval          mdIndex;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    mdIndex = list[mallocID];
    mdstart = MemDescKern::IndexToMD(mdIndex);
    md = mdstart;
    if (md != NULL) do {
	if (md == mdtarget) return 1;
	mdIndex = md->nextIndex();
	md = MemDescKern::IndexToMD(mdIndex);
    } while (md != mdstart);
    return 0;
}

uval
AllocRegionManagerKern::checkSanity()
{
    uval         i;
    MemDescKern *md, *mdprev, *mdnext, *mdstart;
    uval         mdIndex;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    uval sane = 1;

    if (KernelInfo::ControlFlagIsSet(KernelInfo::NO_ALLOC_SANITY_CHECK)) {
	return sane;
    }

    sane = sane && (countMemDescsOnLists() == (allocated - empty));
    tassert(sane, err_printf("Bad list/empty: %ld != (%ld - %ld)\n",
			     countMemDescsOnLists(), allocated, empty));

    // check all of lists seem reasonable (prev/next pointers match up and
    // mallocIDs all match

    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	mdIndex = list[i];
	mdstart = MemDescKern::IndexToMD(mdIndex);
	md = mdstart;
	if (md != NULL) do {
	    mdprev = MemDescKern::IndexToMD(md->prevIndex());
	    mdnext = MemDescKern::IndexToMD(md->nextIndex());
	    sane = sane && mdprev != 0;
	    tassert(sane, err_printf("prev field null\n"));
	    sane = sane && mdnext != 0;
	    tassert(sane, err_printf("next field null\n"));
	    sane = sane && mdprev->nextIndex() == mdIndex;
	    tassert(sane, err_printf("prev->next not md\n"));
	    sane = sane && mdnext->prevIndex() == mdIndex;
	    tassert(sane, err_printf("next->prev not md\n"));
	    sane = sane && md->mallocID() == i;
	    tassert(sane, err_printf("md mallocid mismatch\n"));
	    mdIndex = md->nextIndex();
	    md = mdnext;
	} while (md != mdstart);
    }

    return sane;
}

