/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LMalloc.C,v 1.30 2003/05/06 19:32:47 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Local layer (lowest layer) of the kernel memory allocation subsystem.  It
 * handles per-cpu allocation and deallocation communicating with the next
 * layer up (the GMalloc or global layer) for allocating more and returning
 * extra memory.  See header file for more details.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DataChunk.H"
#include "GMalloc.H"
#include "LMalloc.H"

#ifndef NDEBUG
#include <scheduler/Scheduler.H>
void Alloc_AssertProperCallingContext()
{
    Scheduler::AssertEnabled();
    Scheduler::AssertNonMigratable();
// FIXME: re-enable this assert some day when debugger is fixed
//    to not use same services
//    tassertMsg(Scheduler::GetCurThreadPtr()->isActive(), "Is not active\n");
}
#endif

/* initialize structure; we need the target size of each list (which is
 * also the unit of memory requests for the next layer up, and the id of
 * the next layer up (GMalloc object).  We don't know or need to know
 * the size of the blocks at this layer.
 */

void
LMalloc::init(uval mCount, GMalloc *gm, uval size, uval mid,
	      uval nid, uval p)
{
    freeList.init();
    auxList.init();

    maxCount   = mCount;
    gMalloc    = gm;
    nodeID     = nid;
    blockSize  = size;
    mallocID   = mid;
    pool       = p;
#ifdef ALLOC_STATS
    allocs     = 0;
    frees      = 0;
    remoteFrees= 0;
    allocsUp   = 0;
    freesUp    = 0;
#endif
}

#ifndef TARGET_mips64
/*
 * FIXME: SIGH, these have to be here because of template bug on
 * powerpc gcc compiler if for no other reason
 */
void
SyncedCellPtr::acquire(AllocCellPtr &tmp)
{
    BitBLock<AllocCellPtr>::acquire(tmp);
    // tmp.kosherMainList();
}

void
SyncedCellPtr::release(AllocCellPtr tmp)
{
    // tmp.kosherMainList();
    BitBLock<AllocCellPtr>::release(tmp);
}
#endif

void *
LMalloc::slowMalloc()
{
    AllocCellPtr tmp;
    AllocCell *el;

    while (1) {
	if ((el = freeList.pop(nodeID)) != NULL) return el;

	/* free list is empty, get a new list from aux or layer above */
	auxList.getAndZero(tmp);

	if (tmp.isEmpty()) {
	    /* aux list is empty, get from layer above instead.. */
	    tmp = gMalloc->gMalloc();
#if defined(ALLOC_STATS)
	    FetchAndAddVolatile(&allocsUp, 1);
#endif
	    tassert(!tmp.isEmpty(),
		    err_printf("request to gmalloc level failed\n"));
	}

	/* tmp now contains a new list ready to put in free list */

	// place tmp in freelist if it is still empty
	if (freeList.setIfZero(tmp) == SyncedCellPtr::FAILURE) {
	    /* free list no longer empty, try to store in aux */
	    if (auxList.setIfZero(tmp) == 0) {
		/* aux list no longer empty, give back to upper layer */
		gMalloc->gFree(tmp, GMalloc::DO_GO_UP);
#if defined(ALLOC_STATS)
		// should we count this?
		FetchAndAddVolatile(&freesUp, 1);
#endif
	    }
	}
    }
}

void
LMalloc::moveUp(AllocCellPtr tmp)
{
    /* removed freelist (in tmp), now atomicaly swap into aux */
    if (auxList.setIfZero(tmp) == 0) {
	/* aux list no longer empty, give back to upper layer */
	gMalloc->gFree(tmp, GMalloc::DO_GO_UP);
#if defined(ALLOC_STATS)
	FetchAndAddVolatile(&freesUp, 1);
#endif
    }
}

void
LMalloc::checkMallocID(void *addr)
{
    gMalloc->checkMallocID(addr);
}
