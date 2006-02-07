/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocStats.C,v 1.2 2001/01/17 15:25:37 rosnbrg Exp $
 *****************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include "GMalloc.H"
#include "AllocStats.H"

void
AllocStats::init(uval p)
{
    uval i;
    pool = p;
    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	numAllocCalls[i]     = 0;
	cellsAllocated[i]    = 0;
	cellsFreed[i]	     = 0;
	regionsSearched[i]   = 0;
	memDescsSearched[i]  = 0;
	memDescsAllocated[i] = 0;
	memDescsFreed[i]     = 0;
	memDescsEmptied[i]   = 0;
	memDescsUnemptied[i] = 0;
    }
    regionsAllocated = 0;
    regionsFreed = 0;
    maxMemDescAllocated = 0;
#ifdef ALLOC_STATS
    uval vp, mid;
    for (vp = 0; vp < MY_MAX_PROCS; vp++) {
	for (mid = 0; mid < AllocPool::NUM_MALLOCS; mid++) {
	    gmallocs[vp][mid] = NULL;
	}
    }
#endif
#ifdef ALLOC_TRACK
    totalAllocated = maxAllocated = 0;
    totalSpace = maxSpace = 0;
    for (i=0; i<AllocPool::NUM_MALLOCS; i++) {
	totalRequestedByMID[i] = maxRequestedByMID[i] = 0;
	totalSpaceByMID[i] = maxSpaceByMID[i] = 0;
    }
#endif
}

#ifdef ALLOC_STATS
struct LMallocStatsInfo {
    uval allocs[AllocPool::NUM_MALLOCS];
    uval frees[AllocPool::NUM_MALLOCS];
    uval allocsUp[AllocPool::NUM_MALLOCS];
    uval freesUp[AllocPool::NUM_MALLOCS];
    uval remoteFrees[AllocPool::NUM_MALLOCS];
    uval listSize[AllocPool::NUM_MALLOCS];
    uval gotlm[AllocPool::NUM_MALLOCS];
    uval pool;
    void getInfo() {
	uval mid;
	for (mid = 0; mid < AllocPool::NUM_MALLOCS; mid++) {
	    LMalloc *lm;
	    lm = allocLocal[pool].byMallocId(mid);
	    if (lm->getSize() != 0) {
		gotlm[mid]	 = 1;
		allocs[mid]	 = lm->getAllocs();
		frees[mid]	 = lm->getFrees();
		allocsUp[mid]	 = lm->getAllocsUp();
		freesUp[mid]	 = lm->getFreesUp();
		remoteFrees[mid] = lm->getRemoteFrees();
		listSize[mid]	 = lm->getMaxCount();
	    } else {
		gotlm[mid]	 = 0;
	    }
	}
    }
};
struct LMallocStatsRequestMsg : public MPMsgMgr::MsgSync {
    LMallocStatsInfo *info;
    virtual void handle() {
	info->getInfo();
	reply();
    }
};
#endif

void
AllocStats::printStats()
{
    uval i;

    cprintf("\n\nStats for all AllocRegions, pool %ld (%s)\n", pool,
	    (pool == AllocPool::PAGED) ? "paged" : "pinned" );
    cprintf("mallocID:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld/%ld", i, AllocPool::MallocIDToSize(i));
    }
    cprintf("\nAllocCalls:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", numAllocCalls[i]);
    }
    cprintf("\ncellsAlloc:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", cellsAllocated[i]);
    }
    cprintf("\ncellsFreed:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", cellsFreed[i]);
    }
    cprintf("\nregionsSearched:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", regionsSearched[i]);
    }
    cprintf("\nmemDescsSearched:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", memDescsSearched[i]);
    }
    cprintf("\nmemDescsAlloc:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", memDescsAllocated[i]);
    }
    cprintf("\nmemDescsFreed:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", memDescsFreed[i]);
    }
    cprintf("\nmemDescsEmptied:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", memDescsEmptied[i]);
    }
    cprintf("\nmemDescsUnemptied:");
    for (i = 0; i < AllocPool::NUM_MALLOCS; i++) {
	cprintf(" %ld", memDescsUnemptied[i]);
    }
    cprintf("\n");
    cprintf("Regions allocated %ld\n", regionsAllocated);
    cprintf("Regions freed %ld\n", regionsFreed);
    cprintf("\n");
    cprintf("max number of pages allocated %ld (%ld bytes)\n",
	    maxMemDescAllocated, maxMemDescAllocated*PAGE_SIZE);
#ifdef ALLOC_TRACK
    cprintf("max mem allocated %ld\n", maxAllocated);
    cprintf("max space %ld\n", maxSpace);
    cprintf("current mem allocated %ld\n", totalAllocated);
    cprintf("current space allocated %ld\n", totalSpace);
    for (uval mid = 0; mid < AllocPool::NUM_MALLOCS; mid++) {
	LMalloc *lm;
	lm = allocLocal[pool].byMallocId(mid);
	if (lm->getSize() != 0) {
	    cprintf("(mid=%ld,bs=%ld): totreq %ld, totspace %ld, "
		    "maxreq %ld, maxspace %ld\n", mid, lm->getSize(),
		    totalRequestedByMID[mid], totalSpaceByMID[mid],
		    maxRequestedByMID[mid], maxSpaceByMID[mid]);
	}
    }
#endif

#ifdef ALLOC_STATS
    uval mid;
    VPNum numvp = DREFGOBJ(TheProcessRef)->vpCount();
    VPNum myvp  = Scheduler::GetVP();
    VPNum vp;
    SysStatus rc;
    LMallocStatsInfo info;
    uval allocs, frees, allocsUp, freesUp, listSize, numListsTarget;
    GMalloc *gm;

    // for lmallocs, memory in per-proc region, so must do remote call to
    // get it
    cprintf("\n");
    info.pool = pool;
    for (vp = 0; vp < numvp; vp++) {
	if (vp == myvp) {
	    info.getInfo();
	} else {
	    MPMsgMgr::MsgSpace msgSpace;
	    LMallocStatsRequestMsg *const msg =
		new(Scheduler::GetEnabledMsgMgr(), msgSpace)
		LMallocStatsRequestMsg;
	    msg->info = &info;
	    rc = msg->send(SysTypes::DSPID(0, vp));
	    passert(_SUCCESS(rc), err_printf("stats send failed\n"));
	}
	for (mid = 0; mid < AllocPool::NUM_MALLOCS; mid++) {
	    if (info.gotlm[mid]) {
		cprintf("LM(vp=%d,mid=%ld,bs=%ld) "
			"a %ld, f %ld, aup %ld, fup %ld, "
			"rf %ld, ls %ld\n", vp, mid,
			AllocPool::MallocIDToSize(mid),
			info.allocs[mid], info.frees[mid],
			info.allocsUp[mid], info.freesUp[mid],
			info.remoteFrees[mid], info.listSize[mid]);
	    }
	}
    }

    cprintf("\n");
    for (vp = 0; vp < numvp; vp++) {
	for (mid = 0; mid < AllocPool::NUM_MALLOCS; mid++) {
	    if ((gm = gmallocs[vp][mid]) != NULL) {
		allocs = gm->getAllocs();
		frees = gm->getFrees();
		allocsUp = gm->getAllocsUp();
		freesUp = gm->getFreesUp();
		listSize = gm->getListSize();
		numListsTarget = gm->getNumListsTarget();
		cprintf("GM(vp=%d,mid=%ld,bs=%ld) "
			"a %ld, f %ld, aup %ld, fup %ld, "
			"lsize %ld, numl %ld\n", vp, mid,
			AllocPool::MallocIDToSize(mid),
			allocs, frees, allocsUp, freesUp, listSize,
			numListsTarget);
	    }
	}
    }
#endif

}
