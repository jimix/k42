/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocRegionManager.C,v 1.12 2002/05/09 15:00:16 mostrows Exp $
 *****************************************************************************/
#include <sys/sysIncs.H>
#include "PageAllocator.H"
#include "DataChunk.H"
#include "AllocRegionManager.H"
#include <sys/KernelInfo.H>

#ifdef ALLOC_STATS
#define ALLOC_GIVEBACK 0
#else
#define ALLOC_GIVEBACK 1
#endif

// true if regions are return when free
uval AllocRegionManager_GiveBack = ALLOC_GIVEBACK;

AllocRegionManager::AllocRegionManager(AllocPool *allocPool)
{
    uval i;

    //err_printf("AllocRegionManager::init(%lx) - %lx\n", allocPool, this);

    regionList = freeRegionDesc = 0;
    ap = allocPool;
    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	hint[i] = 0;
    }
    hintFree = 0;
    lock.init();
}

void
AllocRegionManager::initRegionDescriptors(uval buf, uval len)
{
    int numreg = 0;
    while (sizeof(AllocRegionDesc) <= len) {
	AllocRegionDesc *ar = (AllocRegionDesc *)buf;
	ar->data = 0;
	ar->lock.init();
	ar->next = freeRegionDesc;
	ar->myRegionManager = this;
	freeRegionDesc = ar;
	buf += sizeof(AllocRegionDesc);
	len -= sizeof(AllocRegionDesc);
	numreg++;
    }
}


// Must be called with the lock held
AllocRegionDesc *
AllocRegionManager::createRegion()
{
    SysStatus rc;
    uval ptr;
    uval flags;
    AllocRegionDesc *reg;

    //err_printf("Creating new region\n");

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    if (ap->getPool() == AllocPool::PINNED) {
	// FIXME: probably just rid of this stuff when we do pinned
	// separately and differently anyway
	// we can't handle these calls failing, so for now, we block
	//flags = PageAllocator::PAGEALLOC_USERESERVE
	//| PageAllocator::PAGEALLOC_NOBLOCK;
	flags = 0;
    } else {
	flags = 0;
    }

    while ((reg = freeRegionDesc) == NULL) {
	rc = DREF(ap->getPageAllocator())->allocPages(ptr, PAGE_SIZE, flags);
	passert(_SUCCESS(rc), err_printf(" woops\n"));
	initRegionDescriptors(ptr, PAGE_SIZE);
    }

    // dequeue from free list ap region desc
    freeRegionDesc = reg->next;

    // creates a circular singly linked list, so can start
    // search from arbitrary point and know done when wrap
    // back around
    if (!regionList) {
	reg->next = reg;
    } else {
	reg->next = regionList->next;
	regionList->next = reg;
    }
    rc = DREF(ap->getPageAllocator())->allocPagesAligned(ptr,
	(uval)ALLOC_REGION_PAGES*ALLOC_PAGE_SIZE,
	(uval)ALLOC_REGION_PAGES*ALLOC_PAGE_SIZE, 0, flags);
    passert(_SUCCESS(rc), err_printf(" woops\n"));
    reg->init(ptr);

    regionList = reg;			// make new region the "head"

    stats()->incRegionsAllocated();
    if (!AllocRegionManager_GiveBack) {
	// global stats only make sense if regions not given back
	stats()->incMaxMemDescAllocated();// account for mem descriptors
    }

    return reg;
}

/* Allocate some blocks, return as a list
 *
 * This contains the main logic for allocated chunks from memdesc and regions
 */
DataChunk *
AllocRegionManager::alloc(uval numBlocks, uval mallocID)
{
    AllocRegionDesc *reg, *startreg;
    FirstRegionPage *frp;
    MemDesc         *md, *startmd;
    DataChunk       *list;
    uval             nextIndex, mdIndex;
    uval             numAllocated, empty;
    uval             nodeID = ap->mallocIDToNode(mallocID);

    /* The lock protects the list of regions and each region's next pointer
     * allowing us to safely traverse the list, but we still need the locks
     * in the individual regions to walk the memdesc list, as well as the
     * individual memdesc locks to allocate from them.  We hope in general to
     * be able to allocate many blocks at a time with each lock acquisition.
     */
    lock.acquire();

    stats()->incNumAllocCalls(mallocID);
    stats()->incCellsAllocated(mallocID, numBlocks);

    // we start by looking for already partially allocated pages in the
    // list of regions

    // we start with the hint memdesc and it's region; both the region list
    // and the memdesc list in the region are circular
    startreg = hint[mallocID];
    //err_printf("AllocRegionManager(%ld,%ld) start %lx\n",numBlocks,mallocID,
    //       startreg == 0 ? 0 : startreg->data);
    if (!startreg) {
	// no hint, start from the beginning
	startreg = regionList;
	if (!startreg) {
	    // need to create one
	    createRegion();
	    startreg = regionList;
	}
    }
    tassert(startreg, err_printf("Should not happen\n"));

    reg = startreg;
    list = 0;

    do {
	stats()->incRegionsSearched(mallocID);

	reg->lock.acquire();
	tassert(reg->checkSanity(), err_printf("oops\n"));

	frp = reg->data;

	//err_printf("AllocRegionManager: trying region %lx, next %lx\n", frp,
	//   reg->next->data);

    MDSearch:

	// try to find something free with this block size
	startmd = AllocRegionDesc::indexToMD(frp, reg->list[mallocID]);

	//err_printf("Trying from md %lx/%ld\n", startmd, reg->list[mallocID]);

	md = startmd;
	if (md) do {
	    stats()->incMemDescsSearched(mallocID);
	    tassert( (md->mallocID() == mallocID),
		     err_printf("woops\n") );
	    if (md->nodeID() != nodeID) {
		// I don't think this should ever happen with current setup
		err_printf("(%ld/%ld) skipping %ld\n",
			   mallocID, nodeID, md->nodeID());
		nextIndex = md->nextIndex();
	    } else {
		list = md->alloc(list,numBlocks,numAllocated,nextIndex,empty);
		numBlocks -= numAllocated;
		if (empty) {
		    // remove from alloc list
		    AllocRegionDesc::removeFromDLList(frp,reg->list[mallocID],
						      md);
		    reg->empty++;
		    stats()->incMemDescsEmptied(mallocID);
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
		    tassert(reg->checkSanity(), err_printf("oops\n"));
		    // update list so we start from here next time
		    if (md) {
			reg->list[mallocID] = md->getIndex();
		    } else {
			// md was just removed, start with next
			reg->list[mallocID] = nextIndex;
		    }
		    reg->lock.release();
		    hint[mallocID] = reg;
		    //err_printf("All done - 1\n");
		    lock.release();
		    return list;
		}
	    }
	    // still need more, so try next md in this region
	    md = AllocRegionDesc::indexToMD(frp, nextIndex);
	    // check for wrap around
	} while (md != startmd);

	// try next region; our allocregion lock protects the region from
	// being destroyed as well as the next field, so we can release
	// the region lock first, before reading the next field and grabbing
	// the lock for the next region

	tassert(reg->checkSanity(), err_printf("oops\n"));
	reg->lock.release();
	reg = reg->next;

	// check for wrap around (no more memory in allocated mds)
    } while (reg != startreg);

    // next we try to find totally free pages in the regions
    startreg = hintFree;
    //err_printf("AllocRegionManager: no allocated blocks, trying free %lx\n",
    //startreg == 0 ? 0 : startreg->data);
    if (!startreg) {
	// no hint, start from the beginning
	startreg = regionList;
	if (!startreg) {
	    // need to create one
	    createRegion();
	    startreg = regionList;
	}
    }
    tassert(startreg, err_printf("Should not happen\n"));

    reg = startreg;

    while (1) {

	stats()->incRegionsSearched(mallocID);

	reg->lock.acquire();
	tassert(reg->checkSanity(), err_printf("oops\n"));

	frp = reg->data;

	//err_printf("Looking for free memdesc for region %lx\n", frp);

	// try to find mem in totally free blocks
	while ((md = AllocRegionDesc::indexToMD(frp, reg->freeList)) != NULL) {

	    stats()->incMemDescsSearched(mallocID);
	    stats()->incMemDescsAllocated(mallocID);

	    // used as quick/dirty way to trace pages allocated: pass through
	    // sort | uniq and count to get max pages actually used.
	    //err_printf("XXX %p %ld\n", reg, reg->freeList);

	    // remove from freelist and add to cell list
	    mdIndex = reg->freeList;	// remember new md index
	    reg->freeList = md->nextIndex(); // dequeue from free list
	    reg->allocated++;		// update count of allocated mds
	    if (reg->allocated > reg->maxAllocated) {
		tassert(reg->allocated == reg->maxAllocated + 1,
			err_printf("oops\n"));
		reg->maxAllocated++;
		if (!AllocRegionManager_GiveBack) {
		    // global stats only make sense if regions not given back
		    stats()->incMaxMemDescAllocated();
		}
	    }
	    md->init(mallocID, nodeID); // init for given mallocID/nodeID

	    tassert(md->mallocID() == mallocID,
		    err_printf("bad mid: %ld != %ld\n",
			       md->mallocID(), mallocID));

	    // now allocate blocks
	    list = md->alloc(list, numBlocks, numAllocated, nextIndex, empty);
	    if (empty) {
		// nothing left
		stats()->incMemDescsEmptied(mallocID);
		reg->empty++;
	    } else {
		// still something left, so put on list
		AllocRegionDesc::addToDLList(frp, reg->list[mallocID], md);
	    }

	    //err_printf("AllocRegionManager: alloc %ld from free md %lx\n",
	    //numAllocated, md->getPage());
	    numBlocks -= numAllocated;
	    if (numBlocks == 0) {
		// we're all done for this call, release all locks and return
		tassert(reg->checkSanity(), err_printf("oops\n"));
		reg->lock.release();
		hint[mallocID] = reg;
		hintFree = reg;
		//err_printf("All done - 2\n");
		lock.release();
		return list;
	    }
	    // still need more, so try another free md in this region
	}

	// no more free mds in this region, try the next.

	// try next region; our allocregion lock protects the region from
	// being destroyed as well as the next field, so we can release
	// the region lock first, before reading the next field and grabbing
	// the lock for the next region

	tassert(reg->checkSanity(), err_printf("oops\n"));
	reg->lock.release();

	if (reg->next == startreg) {
	    // we've wrapped around, no more free mds in any region

	    // we need to allocate a new region, it gets added to the front,
	    // so adjust startreg so that it appears we started with the
	    // region right after the one just allocated, in case, for some
	    // reason, this new region is still not enough
	    reg = createRegion();
	    startreg = reg->next;
	    tassert(startreg, err_printf("list can't be empty here\n"));
	} else {
	    reg = reg->next;
	}
    }

    tassert(0, err_printf("Should never get here\n"));
    return 0;
}

/* Free a single block
 *
 * This contains the main logic for freeing a block, as well as freing
 * memdesc and regions.
 *
 * The general strategy is to try to lock only the most directly related
 * object.  First just the memdesc; but if the memdesc would become totally
 * free as a result, then we lock the region and try to free the memdesc;
 * which in turn may result in freeing the region, requiring the allocregion
 * lock.
 */
/* static */ void
AllocRegionManager::slowFree(uval block, uval mallocID, MemDesc *md, uval pool)
{
    MemDesc::FreeRC     mdrc;
    AllocRegionDesc    *reg;
    FirstRegionPage    *frp;
    AllocRegionManager *allocRegion;

    // this is either the first block back or the last block back, try again
    // but with the region lock held
    frp = md->fp();
    reg = frp->regPtr();

    reg->lock.acquire();
    tassert(reg->checkSanity(), err_printf("oops\n"));

    // now try to free block again, requesting md to do so even if last one
    // and to indicate if first or last block back
    mdrc = md->freeAndCheck(block);
    if (mdrc == MemDesc::OK) {
	// no longer last or first block, so we're all done
	tassert(!reg->memDescIsOnFreeList(md), err_printf("oops\n"));
	tassert(reg->memDescIsOnList(md,mallocID), err_printf("oops\n"));
	tassert(reg->checkSanity(), err_printf("oops\n"));
	//err_printf("md no longer free\n");
	reg->lock.release();
	return;
    }

    // md is now fully free or just changed to non-empty,
    // so dequeue from neighbours (if own neighbour, list is now empty)


    if (mdrc == MemDesc::WASEMPTY) {
	// transition from empty to non-empty; put back on list
	tassert(!reg->memDescIsOnList(md,mallocID), err_printf("oops\n"));
	AllocRegionDesc::addToDLList(frp, reg->list[mallocID], md);
	stats(pool)->incMemDescsUnemptied(mallocID);
	reg->empty--;
    } else {
	// transition from non-empty to full, free it
	tassert(mdrc == MemDesc::FULL, err_printf("oops\n"));
	tassert(!reg->memDescIsOnFreeList(md), err_printf("oops\n"));
	tassert(reg->memDescIsOnList(md,mallocID), err_printf("oops\n"));
	AllocRegionDesc::removeFromDLList(frp, reg->list[mallocID], md);
	// add to freelist
	md->nextIndex(reg->freeList);
	reg->freeList = md->getIndex();
	reg->allocated--;
	stats(pool)->incMemDescsFreed(mallocID);
    }

    if (reg->allocated != 0) {
	// nothing more to do
	tassert(reg->checkSanity(), err_printf("oops\n"));
	reg->lock.release();
	return;
    }

    //err_printf("Trying to free region %lx\n", reg->data);

    // region is completely free; however, regions are not double-linked
    // so we need to scan the regionlist to remove this region, so there
    // is no need to keep the region locked for safety; if it's freed by
    // someone else, we simply won't find it on the list

    allocRegion = reg->myRegionManager;
    tassert(reg->checkSanity(), err_printf("oops\n"));
    reg->lock.release();

    allocRegion->tryFreeRegion(reg);
}

void
AllocRegionManager::tryFreeRegion(AllocRegionDesc *reg)
{
    SysStatus rc;
    uval      data = 0;			// must be initialized to zero
    AllocRegionDesc *prevReg, *startReg;

    // acquire main regions list lock and scan for "reg"
    lock.acquire();

    // remember that the list is circular, and could be empty
    startReg = regionList;
    if (startReg == NULL) {
	// empty, so nothing to do
	lock.release();
	return;
    }
    prevReg = startReg;
    do {
	// we start by scanning the next one, eventually we'll circle around
	// back to the beginning in case reg is the first one
	if (prevReg->next == reg) {
	    // found it, verify still ready to free
	    reg->lock.acquire();
	    tassert(reg->checkSanity(), err_printf("oops\n"));
	    if (AllocRegionManager_GiveBack && reg->allocated == 0) {
		// ok, free it
		if (reg == reg->next) {
		    tassert(prevReg == reg, err_printf("bad region list\n"));
		    // last one
		    regionList = 0;
		} else {
		    // link around it
		    prevReg->next = reg->next;
		    if (regionList == reg) regionList = reg->next;
		}
		// put region on freeList
		reg->next = freeRegionDesc;
		freeRegionDesc = reg;
		// update hint array
		invalidateHints(reg);
		stats()->incRegionsFreed();
		// get memory address for free below (with lock released)
		data = uval(reg->data);
	    }
	    // all done
	    tassert(reg->checkSanity(), err_printf("oops\n"));
	    reg->lock.release();
	    lock.release();
	    if (data) {
		// now give region memory (pointed to by frp) back
		rc = DREF(ap->getPageAllocator())->
		    deallocPages(data, ALLOC_REGION_PAGES*ALLOC_PAGE_SIZE);
		tassert(_SUCCESS(rc), err_printf(" woops\n"));
	    }
	    return;
	}
	prevReg = prevReg->next;
    } while (prevReg != startReg);

    // didn't find region, no matter
    lock.release();
}

/* static */ void
AllocRegionDesc::removeFromDLList(FirstRegionPage *frp,uval &list,MemDesc *md)
{
    uval     mdIndex, prevIndex, nextIndex;
    MemDesc *prevmd, *nextmd;

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
	nextmd = indexToMD(frp, nextIndex);
	prevmd = indexToMD(frp, prevIndex);
	nextmd->prevIndex(prevIndex);
	prevmd->nextIndex(nextIndex);
	if (list == mdIndex) {
	    list = nextIndex;
	}
    }
}

/* static */ void
AllocRegionDesc::addToDLList(FirstRegionPage *frp, uval &list, MemDesc *md)
{
    uval     mdIndex, prevIndex, nextIndex;
    MemDesc *prevmd, *nextmd;

    mdIndex = md->getIndex();
    // add to queue after starting point
    prevIndex = list;
    prevmd = indexToMD(frp, prevIndex);
    if (prevmd == NULL) {
	// empty, so make circular queue of one
	md->prevNextIndex(mdIndex, mdIndex);
    } else {
	// insert after prev
	nextIndex = prevmd->nextIndex();
	nextmd = indexToMD(frp, nextIndex);
	prevmd->nextIndex(mdIndex);
	nextmd->prevIndex(mdIndex);
	md->prevNextIndex(prevIndex, nextIndex);
    }
    list = mdIndex;			// make new md "head"
}

void
AllocRegionManager::invalidateHints(AllocRegionDesc *reg)
{
    uval i;
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	if (hint[i] == reg) hint[i] = 0;
    }
    if (hintFree == reg) hintFree = 0;
}

// test if given region is on our allocated list; for debugging
uval
AllocRegionManager::findRegion(AllocRegionDesc *targetreg)
{
    AllocRegionDesc *startreg, *reg;

    reg = startreg = regionList;
    if (regionList) do {
	if (reg == targetreg) return 1;	// found it
	reg = reg->next;
    } while (reg != startreg);
    return 0;				// didn't find it
}

void
AllocRegionDesc::init(uval addr)
{
    uval i;
    tassert((sizeof(MemDesc)==8), err_printf("fundamental assumption\n"));

    //err_printf("AllocRegionDesc: init %lx\n", addr);
    magic = MAGIC;
    data = (FirstRegionPage *)addr;
    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	list[i] = 0;
    }
    freeList = 1;			// first entry in page
    data->regPtr(this);
    for (i=1; i<(ALLOC_DESC_PER_PAGE-1); i++) {
	data->md[i].initAsFree(i+1);
    }
    data->md[ALLOC_DESC_PER_PAGE-1].initAsFree(0);
    allocated = 0;
    maxAllocated = 0;
    empty = 0;
}

uval
AllocRegionDesc::countFreeMemDescs()
{
    MemDesc         *md;
    uval             mdIndex;
    FirstRegionPage *frp;
    uval             count;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    count = 0;
    frp = data;
    mdIndex = freeList;
    md = indexToMD(frp, mdIndex);
    while (md != NULL) {
	count++;
	mdIndex = md->nextIndex();
	md = indexToMD(frp, mdIndex);
    }

    return count;
}

uval
AllocRegionDesc::countMemDescsOnLists()
{
    MemDesc         *md, *mdstart;
    uval             mdIndex;
    FirstRegionPage *frp;
    uval             count;
    uval             i;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    count = 0;
    frp = data;
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	mdIndex = list[i];
	mdstart = indexToMD(frp, mdIndex);
	md = mdstart;
	if (md != NULL) do {
	    count++;
	    mdIndex = md->nextIndex();
	    md = indexToMD(frp, mdIndex);
	} while (md != mdstart);
    }

    return count;
}

uval
AllocRegionDesc::memDescIsOnFreeList(MemDesc *mdtarget)
{
    MemDesc         *md;
    uval             mdIndex;
    FirstRegionPage *frp;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    frp = data;
    mdIndex = freeList;
    md = indexToMD(frp, mdIndex);
    while (md != NULL) {
	if (md == mdtarget) return 1;
	mdIndex = md->nextIndex();
	md = indexToMD(frp, mdIndex);
    }

    return 0;
}

uval
AllocRegionDesc::memDescIsOnList(MemDesc *mdtarget, uval mallocID)
{
    MemDesc         *md, *mdstart;
    uval             mdIndex;
    FirstRegionPage *frp;

    tassert(lock.isLocked(), err_printf("lock must be held\n"));

    frp = data;
    mdIndex = list[mallocID];
    mdstart = indexToMD(frp, mdIndex);
    md = mdstart;
    if (md != NULL) do {
	if (md == mdtarget) return 1;
	mdIndex = md->nextIndex();
	md = indexToMD(frp, mdIndex);
    } while (md != mdstart);
    return 0;
}

uval
AllocRegionDesc::checkSanity()
{
    uval             i;
    MemDesc         *md, *mdprev, *mdnext, *mdstart;
    uval             mdIndex;
    FirstRegionPage *frp;
    uval sane = 1;

    if (KernelInfo::ControlFlagIsSet(KernelInfo::NO_ALLOC_SANITY_CHECK)) {
	return sane;
    }

    tassert(lock.isLocked(), err_printf("lock must be held\n"));


    sane = sane && checkMagic();
    tassert(sane, err_printf("magic value wrong\n"));

    sane = sane && (countFreeMemDescs()
		    == (ALLOC_REGION_PAGES-1-allocated));
    tassert(sane, err_printf("Bad free count: %ld != %ld (%ld,%ld)\n",
			     countFreeMemDescs(),
			     ALLOC_REGION_PAGES-1-allocated,
			     ALLOC_REGION_PAGES, allocated));

    sane = sane && (countMemDescsOnLists() == (allocated - empty));
    tassert(sane, err_printf("Bad list/empty: %ld != (%ld - %ld)\n",
			     countMemDescsOnLists(), allocated, empty));

    // check all of lists seem reasonable (prev/next pointers match up and
    // mallocIDs all match

    frp = data;
    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	mdIndex = list[i];
	mdstart = indexToMD(frp, mdIndex);
	md = mdstart;
	if (md != NULL) do {
	    mdprev = indexToMD(frp, md->prevIndex());
	    mdnext = indexToMD(frp, md->nextIndex());
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

