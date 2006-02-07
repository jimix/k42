/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernPinned.C,v 1.70 2004/11/16 20:06:15 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include "mem/PageAllocatorKernPinned.H"
#include "init/MemoryMgrPrimitiveKern.H"
#include "mem/RegionDefault.H"
#include "mem/FRPlaceHolder.H"
#include "mem/VAllocServicesKern.H"
#include "mem/AllocRegionManagerKern.H"
#include __MINC(PageAllocatorKernPinned.H)
#include <misc/BaseRandom.H>
#include <misc/HashNonBlocking.H>

/* static */ uval PageAllocatorKernPinned::MyRoot::virtBase;
/* static */ uval PageAllocatorKernPinned::MyRoot::maxPhysMem;

class Element: public MemDescKern {
    Element* next_hash;
public:
    Element* nextFree() {
	return (Element*)nextIdx;
    }
    void setNextFree(Element* node) {
	nextIdx = (uval)node;
    }
    Element* nextHash() {
	return next_hash;
    }
    void setNextHash(Element* node) {
	next_hash = node;
    }
    uval getKey() {
	return frameAddress;
    }
    void setKey(uval key) {
	frameAddress = key;
    }
    static uval Hash(uval key) {
	return key>>LOG_PAGE_SIZE;
    }

    /*
     * we remove elements exactly when we know they can't be use,
     * namely when the associated frame is no in use, the md
     * is not on any chain, and we haven't yet freed the frame,
     * so no reuse is yet possible.
     * we use the impossible value of -1 in the nextIdx field to mark
     * an md as defunct.  this is needed by the non-blocking hash remove
     * logic.
     */
    sval upLock() {
	return (nextIdx != uval(-1));
    }
    /*
     * returns + if still inuse, 0 if became unused
     * - if already unused
     */
    sval unLock() {
	uval oldIdx;
	do {
	    oldIdx = nextIdx;
	    if(oldIdx == uval(-1)) return -1;;
	} while(!CompareAndStoreSynced(&nextIdx, oldIdx, uval(-1)));
	return 0;
    }
    uval isLocked() {
	return (nextIdx != uval(-1));
    }
    void setLock() {
	nextIdx = 0;			// anything but uval(-1)
    }
};

class AllocForMDH {
public:
    static void* alloc(uval size) {
	uval ptr;
	SysStatus rc;
	if (size == sizeof(Element)) {
	    return DREFGOBJK(ThePinnedPageAllocatorRef)->getNextForMDH(size);
	}
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(ptr, PAGE_ROUND_UP(size));
	passertMsg(_SUCCESS(rc), "no memory\n");
	return (void*)ptr;
    }

    static void free(void* addr, uval size) {
	tassertMsg(size != sizeof(Element),
		   "why are we freeing a memdesc\n");
	if (size == sizeof(MemDescKern)) return;
	tassertMsg((uval(addr)&PAGE_MASK) == 0, "why not page aligned\n");
	DREFGOBJK(ThePinnedPageAllocatorRef)->
	    deallocPages(uval(addr), PAGE_ROUND_UP(size));
	return;
    }
};



class MemDescHashImp: public PageAllocatorKernPinned::MemDescHash {
    HashNonBlockingBase<AllocForMDH, Element, 128> hash;
public:
    DEFINE_PRIMITIVE_NEW(MemDescHashImp);
    virtual uval getMemDesc(uval addr) {
	Element* node;
	uval ret;
	addr &= -PAGE_SIZE;
	// see if its there - normal case
	if (0==hash.find_node(addr, node)) {
	    return (uval)node;
	}
	// get ready to add it
	hash.alloc_node(node);
	node->setKey(addr);
	node->setLock();
	
	// if its now already there, the node we allocated above
	// is freed and the node variable updated to point to found node
	ret = hash.find_or_add_node(node);
#if 0					// debug code - remove
	if (ret == 0) {
	    passertMsg(node->nodeID() <= 1, "marc\n");
	} else {
	    *(uval*)node = uval(-1);
	}
#endif
	return (uval)node;
    }

    virtual void freeMemDesc(uval addr) {
	Element* node;
	SysStatus rc;
	rc = hash.remove_node(addr, node);
	tassertMsg(_SUCCESS(rc), "freeMemDesc fumble %lx\n", addr);
    }
};

PageAllocatorKernPinned::MyRoot::MyRoot(RepRef ref, uval vbase, uval maxMem)
     : CObjRootMultiRepBase(ref)
{
    uval i;
    pagingEnabled      = 0;
    maxNumaNodeNum     = 0;
    numPhysChunks      = 0;
    totalPhysMem       = 0;
    virtBase	       = vbase;
    maxPhysMem	       = maxMem;
    for (i = 0; i < AllocCell::MAX_NUMANODES; i++) {
	repByNumaNode[i] = NULL;
    }
}

/*
 * for now, run physical memory as a single numa node
 */
SysStatus
PageAllocatorKernPinned::MyRoot::getNumaInfo(
    VPNum vp, VPNum& node, VPNum& nodeSize)
{
    node = 0;				// one physical numanode
    nodeSize = KernelInfo::CurPhysProcs();
    return 0;
}

void
PageAllocatorKernPinned::MyRoot::vpInit(VPNum vp,
					MemoryMgrPrimitiveKern *memory)
{
    VPNum numaNode, d1;
    PageAllocatorKernPinned *rep;
    uval ptr;
    uval size;

    getNumaInfo(vp, numaNode, d1);
    lock.acquire();
    rep = repByNumaNode[numaNode];
    if (rep == NULL) {
	// need to create allocator for this numa node
	rep = new(memory) PageAllocatorKernPinned();
	repByNumaNode[numaNode] = rep;
	if (numaNode > maxNumaNodeNum) maxNumaNodeNum = numaNode;

	// move remaining memory from primitive memory mgr to PageAllocator
	memory->allocAll(ptr, size, PAGE_SIZE);
	rep->init(ptr, size);
	// special work for KernPinned allocator
	rep->pinnedInit(numaNode);
    }
    lock.release();

    //uval avail;
    //DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(avail);
    //err_printf("PinnedAllocator: local avail %ld, total %ld\n",
    //       rep->available, avail);
}

SysStatus
PageAllocatorKernPinned::MyRoot::handleMiss(COSTransObject * &co,
					    CORef ref, uval methodNum)
{
    SysStatus rc;
    //err_printf("PageAllocatorKernPinned::MyRoot::handleMiss()\n");
    rc = CObjRootMultiRepBase::handleMiss(co, ref, methodNum);
    //err_printf("PageAllocatorKernPinned::MyRoot::handleMiss() done\n");
    return rc;
}

void
PageAllocatorKernPinned::MyRoot::replistAdd(uval vp, CObjRep * rep)
{
    // just ignore this call
}

uval
PageAllocatorKernPinned::MyRoot::replistFind(uval vp, CObjRep *& rep)
{
    VPNum numaNode, d1;
    getNumaInfo(vp, numaNode, d1);
    // only supports call for vp == current vp; uses numa node
    tassert(vp == Scheduler::GetVP(), err_printf("Non-local vp\n"));
    rep = repByNumaNode[numaNode];
    // rep must already be created
    tassert(rep!=NULL, err_printf("oops\n"));
    //err_printf("Returning rep %p for vp %ld numanode %ld\n",
    //       rep, vp, numaNode);
    return 1;
}

void *
PageAllocatorKernPinned::MyRoot::replistNext(void *curr, uval& vp,
					     CObjRep*& rep)
{
    passertMsg(0, "PageAllocatorKernPinned::MyRoot::replistNext NYI\n");
    return 0;
}

uval
PageAllocatorKernPinned::MyRoot::replistRemove(uval vp, CObjRep *& rep)
{
    passertMsg(0, "PageAllocatorKernPinned::MyRoot::replistRemove NYI\n");
    return 0;

}

uval
PageAllocatorKernPinned::MyRoot::replistRemoveHead(CObjRep *& rep)
{
    passertMsg(0, "PageAllocatorKernPinned::MyRoot::replistRemoveHead NYI\n");
    return 0;

}

uval
PageAllocatorKernPinned::MyRoot::replistIsEmpty()
{
    passertMsg(0, "PageAllocatorKernPinned::MyRoot::repIsEmpty NYI\n");
    return 0;

}


uval
PageAllocatorKernPinned::MyRoot::replistHasRep(CObjRep * rep)
{
    passertMsg(0, "PageAllocatorKernPinned::MyRoot::repHasRep NYI\n");
    return 0;
}

CObjRep *
PageAllocatorKernPinned::MyRoot::createRep(VPNum vp)
{
    CObjRep *rep;
    VPNum numaNode, d1;
    getNumaInfo(vp, numaNode, d1);
    // always pre-created, just find local one
    rep = repByNumaNode[numaNode];
    tassert(rep != NULL, err_printf("no rep\n"));
    return rep;
}

uval
PageAllocatorKernPinned::MyRoot::validPhysAddr(uval paddr)
{
    // no lock required (or can be used?)
    uval i;
    for (i=0; i < numPhysChunks; i++) {
	if (paddr >= physChunks[i].start && paddr <= physChunks[i].end) {
	    return 1;
	}
    }
    return 0;
}

void
PageAllocatorKernPinned::MyRoot::addPhysChunk(uval start, uval end)
{
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    tassert(numPhysChunks < MAX_CHUNKS, err_printf("Out of phys chunks\n"));

    // first verify doesn't overlap with anything already installed
    uval i;
    for (i=0; i < numPhysChunks; i++) {
	if ((start >= physChunks[i].start && start <= physChunks[i].end) ||
	    (end >= physChunks[i].start && end <= physChunks[i].end) ||
	    (start <= physChunks[i].start && end >= physChunks[i].end)) {
	    // overlapped, must be identical
	    tassert(start == physChunks[i].start &&
		    end == physChunks[i].end, err_printf("Chunk overlap\n"));
	    return;
	}
    }

    // no overlap, just add to end
    physChunks[numPhysChunks].start = start;
    physChunks[numPhysChunks].end = end;

    // we update last so that those reading it only see new chunks after all
    // initialized
    numPhysChunks++;

    // similarly for the total memory
    totalPhysMem += end - start + 1;
}

void
PageAllocatorKernPinned::MyRoot::printPhysChunks()
{
    uval i;
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    for (i=0; i < numPhysChunks; i++) {
	err_printf("start %lx, end %lx\n",
		   physChunks[i].start, physChunks[i].end);
    }
}


/* note that this allocator uses the first "free" page for meta
   data.  Once initialized, additional memory can be added by freeing */

/*static*/ SysStatus
PageAllocatorKernPinned::ClassInit(VPNum vp, MemoryMgrPrimitiveKern *memory)
{
    SysStatus rc=0;
    static PageAllocatorKernPinned::MyRoot *theRootPtr;

    if (vp==0) {
	theRootPtr = new(memory)
	    MyRoot((RepRef)GOBJK(ThePinnedPageAllocatorRef),
		   memory->virtBase(), MAX_PHYS_MEM_SIZE);
	theRootPtr->memDescHash = new(memory) MemDescHashImp;
    }

    // while we still have the memory, create allocator-required object
    VAllocServicesKern *vas = new(memory) VAllocServicesKern();

    theRootPtr->vpInit(vp, memory);

    allocLocal[AllocPool::PINNED].init(vp, vas, AllocPool::PINNED);

    return rc;
}

void
PageAllocatorKernPinned::pinnedInit(VPNum numaNodeArg)
{
    numaNode = numaNodeArg;
    nextMemDescLock.init();
    nextMemDesc = 0xfff;		// trigger alloc of new page
					// on first call
}

/*static*/ SysStatus
PageAllocatorKernPinned::ClassInitVirtual(VPNum vp,
					  MemoryMgrPrimitiveKern *memory)
{
    SysStatus rc;
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->initVirtual(vp, memory);
    return rc;
}

SysStatus
PageAllocatorKernPinned::initVirtual(VPNum vp, MemoryMgrPrimitiveKern *memory)
{
#ifdef marcdebug
//    marckludge = 1;
#endif /* #ifdef marcdebug */

    return 0;
}

#ifdef marcdebug
uval marcstopavail = uval(-1);
#endif /* #ifdef marcdebug */

SysStatus
PageAllocatorKernPinned::doAllocPagesAllNodes(uval &ptr, uval size, uval align,
					      uval offset, uval flags)
{
    SysStatus rc;
    VPNum nodes, i;
    PageAllocatorKernPinned *rep;

    /* FIXME: note that our checks for available memory are intended to
     * reduce spurious warnings and (primarily) to leave enough memory
     * to allow forward progress (in which case the reserve may be used
     * if requested).  However, since we don't hold a lock during the
     * check and call, there is a race and memory could be depleted even
     * when it shouldn't be.
     */

    // this is not handled properly, so assert for now
    passertMsg((available/PAGE_SIZE > 0), "out of memory\n");

    /* do a quick check if we have enough memory locally, in which case
     * just try locally
     */
    if ((available/PAGE_SIZE >= 0) ||
	((flags & PAGEALLOC_USERESERVE) && (available >= size)) ||
	(!COGLOBAL(pagingEnabled))) {
	rc = allocPagesSimple(ptr, size, align, offset);
    } else {
	// just pretend alloc failed and go through slower process
	rc = _SERROR(1484, 0, ENOMEM);
    }

    if (!_SUCCESS(rc) &&
	(totalAvailable()/PAGE_SIZE >= 0 ||
	 ((flags & PAGEALLOC_USERESERVE) && totalAvailable() > 0))) {
	/* Note, we are allowed to deplete a single rep if there is enough
	 * across all reps.  This is a policy decision which could change
	 * in the future, particularly if the pagout daemon changes to a
	 * per-rep model.
	 */
	//err_printf("Out of aligned memory locally, trying remote\n");
	// check all reps, simple loop from zero over all for now
	nodes = COGLOBAL(maxNumaNodeNum);
	for (i = 0; i <= nodes; i++) {
	    rep = COGLOBAL(repByNumaNode[i]);
	    if (rep != NULL
		&& (!(flags & PAGEALLOC_FIXED) || (rep == this))
		&& rep->available > 0) {
		//err_printf("Trying %ld -> %p\n", i, rep);
		rc = rep->allocPagesSimple(ptr, size, align, offset);
		tassertWrn(!_SUCCESS(rc) || (addrToNumaNode(ptr) == i),
			   "Ooops, allocated remote mem %lx\n", ptr);
		//err_printf("result %lx\n", rc);
		if (_SUCCESS(rc)) return rc;
	    }
	}
    }

    return rc;
}


/* virtual */ SysStatus
PageAllocatorKernPinned::startPaging()
{
    COGLOBAL(pagingEnabled) = 1;
    return 0;
}


SysStatus
PageAllocatorKernPinned::doAllocPages(uval &ptr, uval size, uval align,
				      uval offset, uval flags,
				      VPNum node)
{
    SysStatus rc;
    PageAllocatorKernPinned *rep;
    uval retries=0;
//    static uval last_reported_amount = 100000;

    /*
     * if maxNumaNodeNum is zero, we are running without a pinned
     * numa allocator, so just use LOCAL_NUMANODE
     */
    if (node != LOCAL_NUMANODE) {
	if(COGLOBAL(maxNumaNodeNum) != 0) {
	    if (node > COGLOBAL(maxNumaNodeNum) ||
		((rep = COGLOBAL(repByNumaNode[node])) == NULL)) {
		tassertMsg(0,"PAKP::allocPages: Bad node %ld\n", node);
		return -1;
	    }
	    rc = rep->doAllocPages(ptr,size,align,offset,flags,LOCAL_NUMANODE);
	    passert(_SUCCESS(rc), err_printf("kernel out of memory\n"));
	    return rc;
	} else {
	    node = LOCAL_NUMANODE;
	}
    }

    tassert(node == LOCAL_NUMANODE, err_printf("oops\n"));

    PM::MemLevelState state = GetMemLevelState(available/PAGE_SIZE);
    // if low number pages kick PM to start paging
    if (state == PM::CRITICAL || state == PM::LOW) {
#if 0
	if (last_reported_amount > (available/PAGE_SIZE)) {
	    last_reported_amount = available/PAGE_SIZE;
		err_printf("[K %ld]",last_reported_amount);
	} 
#endif
	if (COGLOBAL(pagingEnabled)) DREFGOBJK(ThePMRootRef)->kickPaging();
    }
    rc = doAllocPagesAllNodes(ptr, size, align, offset, flags);
    if (flags & PAGEALLOC_NOBLOCK) return rc;

    while (!_SUCCESS(rc)) {
	retries++;
	err_printf("alloc failed, kicking PMRoot\n");
	if (COGLOBAL(pagingEnabled)) {
	    DREFGOBJK(ThePMRootRef)->kickPaging();
	}
	Scheduler::DelayMicrosecs(100000);
	rc = doAllocPagesAllNodes(ptr, size, align, offset, flags);
 	if (!_SUCCESS(rc)) {
 	    err_printf("retrying alloc, retries %lx\n", retries);
 	}
    }
    return rc;
}

/* virtual */ SysStatus
PageAllocatorKernPinned::getMemLevelState(PM::MemLevelState &state)
{
    state = GetMemLevelState(available/PAGE_SIZE);
    return 0;
}

SysStatus
PageAllocatorKernPinned::allocPages(uval &ptr, uval size, uval flags,
				    VPNum node)
{
    return doAllocPages(ptr, size, 0, 0, flags, node);
}

SysStatus
PageAllocatorKernPinned::allocPagesAligned(uval &ptr, uval size, uval align,
					   uval offset, uval flags, VPNum node)
{
    return doAllocPages(ptr, size, align, offset, flags, node);
}


SysStatus
PageAllocatorKernPinned::allocPagesAt(uval paddr, uval size, uval flags)
{
    SysStatus rc;
    PageAllocatorKernPinned *rep;

#if 0
    if (!(flags & PAGEALLOC_NOBLOCK)) {
	/* for this to work, would have to force writeback of page, if
	 * possible don't support this yet, so we don't do anything
	 * special in this case
	 */
    }
#endif /* #if 0 */

    tassert(addrToNumaNode(paddr) == addrToNumaNode(paddr + size - 1),
	    err_printf("allocpagesat spans numa nodes: a %lx s %lx\n",
		       paddr, size));
    tassert(addrToNumaNode(paddr) <= COGLOBAL(maxNumaNodeNum),
	    err_printf("Bad addr: %lx\n", paddr));
    rep = COGLOBAL(repByNumaNode[addrToNumaNode(paddr)]);
    tassert(rep != NULL, err_printf("Bad addr: %lx\n", paddr));
    rc = rep->PageAllocatorKern::allocPagesAt(paddr, size);
    return rc;
}

SysStatus
PageAllocatorKernPinned::deallocPages(uval vaddr, uval size)
{
    SysStatus rc;
    PageAllocatorKernPinned *rep;
    tassert(addrToNumaNode(vaddr) == addrToNumaNode(vaddr + size - 1),
	    err_printf("dealloc chunk spans numa nodes: a %lx s %lx\n",
		       vaddr, size));
    rep = this;
    if (!isLocalAddr(vaddr)) {
	tassert(addrToNumaNode(vaddr) <= COGLOBAL(maxNumaNodeNum),
		err_printf("Bad addr: %lx\n", vaddr));
	rep = COGLOBAL(repByNumaNode[addrToNumaNode(vaddr)]);
	tassert(rep != NULL, err_printf("Bad addr: %lx\n", vaddr));
    }

    rc = rep->PageAllocatorKern::deallocPages(vaddr, size);
    return rc;
}

SysStatus
PageAllocatorKernPinned::getMemoryFree(uval &avail)
{
    avail = totalAvailable();
    return 0;
}

SysStatus
PageAllocatorKernPinned::countStuff(uval &valid, uval &accessed, uval &total,
				    uval &avail)
{
    // we really should acquire a lock, but we could dead lock ourselves, and
    // this is only for debugging and the stuff only changes at boot time

    total = COGLOBAL(totalPhysMem)/PAGE_SIZE;
    avail = totalAvailable()/PAGE_SIZE;
    valid = 0;
    accessed = 0;

    return 0;
}

uval
PageAllocatorKernPinned::totalAvailable()
{
    uval i, nodes;
    PageAllocatorKernPinned *rep;
    uval avail = 0;
    // sum over all reps
    nodes = COGLOBAL(maxNumaNodeNum);
    for (i = 0; i <= nodes; i++) {
	rep = COGLOBAL(repByNumaNode[i]);
	if (rep != NULL) avail += rep->available;
    }
    return avail;
}


/*virtual*/ SysStatus
PageAllocatorKernPinned::deallocListOfPages(FreeFrameList *ffl)
{
    uval addr;
    SysStatus rc;
    while((addr = ffl->getFrame()) != 0) {
	rc = deallocPages(addr, PAGE_SIZE);
	tassertMsg(_SUCCESS(rc), "failed free of frame from list\n");
	_IF_FAILURE_RET(rc);
    }
    return 0;
}

#ifdef marcdebug
void
PageAllocatorKernPinned::marcCheckAvail()
{
    freePages *cur = anchor;
    freePages *next,*prev;
    uval tavail=0;
    *(freePages**)(cur->start) = 0;
    while (cur) {
	tavail += cur->size;
	if (cur->low) next=cur->low;
	else if (cur->high) next=cur->high;
	else while (1) {
	    prev = cur;
	    cur = *(freePages**)(cur->start);
	    if (!cur) goto done;
	    if (cur->high && cur->high != prev) {
		next = cur->high;
		break;
	    }
	}
	*(freePages**)(next->start) = cur;
	cur = next;
    }
done:
    tassert(available == tavail, err_printf("avail opps\n"));
}
#endif /* #ifdef marcdebug */

#include <misc/HashNonBlocking.I>
template class HashNonBlockingBase<AllocForMDH, Element, 128>;
