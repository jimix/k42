/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: GMalloc.C,v 1.26 2003/05/20 19:14:50 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Implementation for the Global Layer of the kernel memory allocator
 * subsystem.  It provides an initialization routine, an allocation
 * routine, a deallocation routine, and a print routine.  See header
 * file for more details about the object itself.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DataChunk.H"
#include "GMalloc.H"
#include "PMalloc.H"

/* initialize the object with the configurable parameters */
void
GMalloc::init(uval lSize, uval nListsTarget, uval mypool, uval nid, PMalloc *pm)
{
    freeLists.init();
    listSize       = lSize;
    numListsTarget = nListsTarget;
    myPool         = mypool;
    numaNode         = nid;
    pMalloc        = pm;
#ifdef ALLOC_STATS
    allocs         = 0;
    frees          = 0;
    allocsUp       = 0;
    freesUp        = 0;
#endif
}

/* return a list of blocks of memory.  The number of elements in the
 * list is in the range [1..listSize]
 */

AllocCellPtr
GMalloc::gMalloc()
{
    AllocCellPtr partList;		// non-full list of elems from
    AllocCellPtr tmp;
    uval         lSize;
    AllocCell   *el;
    DataChunk   *freshChunks;		// real pointered list
    uval         count;
    uval         tried = 0;

#if defined(ALLOC_STATS)
    FetchAndAddVolatile(&allocs, 1);
#endif

    while(1) {
	freeLists.acquire(tmp);
	if(tmp.isEmpty()) {
	    freeLists.release(tmp);
	} else {
	    /* get first list from list of lists and correct the
	     * element number from the correct next element
	     */
	    el = tmp.pointer(numaNode);
	    freeLists.release(el->nextList);
	    count = el->next.count() + 1;
	    return AllocCellPtr(count,el);
	}

	// see if there is any remote stuff to free locally
	if (!tried && allocLocal[myPool].checkForRemote()) {
	    tried = 1;			// mark that we've already tried remote
	    // something was found, so try again in case we can now succeed
	    continue;
	}
	
	lSize = listSize;

	/* now determine the amount of elements we want to put into
	 * freeLists and start asking the upper layer to give us a list
	 * of chunks. We convert these ChunkList into a AllocCellPtr through
	 * the partList. Once the partList is full we append it to the
	 * freeLists. If at the end the partList ain't full we insert it
	 * anyway.
	 */

	uval lcnt = 0; // count in the current list

	partList.zero();

	//Try less agressive initial growth of lists of lists, to
	//avoid touching lots of memory when a small single process program
	//runs on a machine with many CPU's per numa node.

	freshChunks = pMalloc->pMalloc(numListsTarget * listSize);
#if defined(ALLOC_STATS)
	FetchAndAddVolatile(&allocsUp, 1);
#endif

	/* freshChunks now contains a new list that has to be converted
	 * to be put on the full list.
	 */

	while (freshChunks) {
	    DataChunk* next = freshChunks->next;
	    lcnt++;
	    ((AllocCell*)freshChunks)->next = partList;
	    partList = AllocCellPtr(lcnt,freshChunks);
	    freshChunks = next;
	    if ((lcnt == lSize) || (freshChunks == NULL)) {
		gFree(partList,DO_GO_UP);
#if defined(ALLOC_STATS)
		// a little hack; the above will increment frees, but
		// really this isn't a free from below, so we undo the stat
		FetchAndAddVolatile(&frees, uval(-1));
#endif
		partList.zero();
		lcnt = 0;
	    }
	}

	/* go back and try gmalloc again */
    }
}

/* Takes a list containing listSize memory blocks, and adds them to its
 * list of lists.  If the list of lists is too big (at least 2 times
 * bigger than numListsTarget), then move some of them up to the next
 * layer, unless goUp indicates that it should not move lists up for
 * fear of blocking in the higher levels (for interrupt routines) or
 * infinite loops (if called by itself).
 */
void
GMalloc::gFree(AllocCellPtr togfree, GoUp goUp)
{
    AllocCellPtr  tmp;
    sval    count;

#if defined(ALLOC_STATS)
    FetchAndAddVolatile(&frees, 1);
#endif

    /* add togfree to beginning with count */
    freeLists.acquire(tmp);
    count = tmp.count();
    count++;

    /* check if we have too many lists and we should go up to next layer
     * we keep the list locked while we are doing this to avoid frequent
     * rechecking. Since deeper down in the list the entries might not
     * be in the cache anymore this might become a problem. At this point we
     * should rethink finer grain locking.
     */
    if((count >= 2*numListsTarget) && (goUp == DO_GO_UP)) {

	/* better if we remove from the end so that recently
	 * released memory will be quickly reallocated and reused while it
	 * is still in the cache. Go through the list and reset the
	 * counter until we have reached numListTarget elements
	 */

	AllocCellPtr lsttofree;
	AllocCellPtr rtmp;
	uval ncount = numListsTarget-1;

	tmp.count(ncount);   // we gonna reuse tmp later !!!
	rtmp = tmp;
	do {
	    ncount--;
	    if (ncount > 0) {
		rtmp = rtmp.pointer(numaNode)->nextList;
	        rtmp.count(ncount);
	    } else {
		lsttofree = rtmp.pointer(numaNode)->nextList;
		rtmp.pointer(numaNode)->nextList.zero();
		break;
	    }
	} while (1);

	// sublist of lists now holds the list of lists to be freed
	// PMalloc will do the conversion

	togfree.pointer(numaNode)->nextList = tmp;
	togfree.count(numListsTarget);
	freeLists.release(togfree);

	pMalloc->pFree(lsttofree);          /* pushup the list to be freed */

#if defined(ALLOC_STATS)
	FetchAndAddVolatile(&freesUp, 1);
#endif

    } else {
	togfree.pointer(numaNode)->nextList = tmp;
	togfree.count(count);
	freeLists.release(togfree);
    }
}


/* print out the information in the global layer */
void
GMalloc::print()
{
    AllocCellPtr tmp;

    freeLists.acquire(tmp);
    freeLists.release(tmp);
    cprintf("count %ld, listsize %d, target %d\n   - ",
	   tmp.count(), listSize, numListsTarget);

    while (!tmp.isEmpty()) {
	tmp.pointer(numaNode)->print(numaNode);
	cprintf("\n   - ");
	tmp = tmp.pointer(numaNode)->nextList;
    }
    cprintf("\n");
}

// print out global layer information, and information from layer above
void
GMalloc::printAll()
{
    print();
}

void
GMalloc::checkMallocID(void *addr)
{
    pMalloc->checkMallocID(addr);
}
