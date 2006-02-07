/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorUser.C,v 1.41 2004/07/11 21:59:23 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: page allocator for user space
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <mem/Access.H>
#include <scheduler/Scheduler.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include "PageAllocatorUser.H"
#include "VAllocServices.H"
#include "MemoryMgrPrimitive.H"
#include "sys/macdefs.H"
#include <sys/memoryMap.H>

#if defined(TARGET_powerpc)
#define NUMASUPPORT
#elif defined(TARGET_mips64)
#define NUMASUPPORT
#elif defined(TARGET_amd64)
#undef NUMASUPPORT
#elif defined(TARGET_generic64)
#undef NUMASUPPORT
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

PageAllocatorUser::MyRoot::MyRoot(RepRef ref) : CObjRootMultiRepBase(ref)
{
    uval i;
    maxNumaNodeNum = 0;
    repByNumaNodeMax = (sizeof initialArray)/(sizeof initialArray[0]);
    repByNumaNode = &(initialArray[0]);
    for (i = 0; i < repByNumaNodeMax;  i++) {
	repByNumaNode[i] = 0;
    }
    if(KernelInfo::ControlFlagIsSet(KernelInfo::NO_NUMANODE_PER_VP)) {
	// treat whole machine as one numa node
	cpusPerNumaNode = KernelInfo::CurPhysProcs();
    } else {
	// for test purposes, lets us set the number of
	// cpus per numanode
	// beware - the very highest byte of control flags is used
	// to specify the page table size so stay away from it.
	uval flags = KernelInfo::GetControlFlags();
	flags = (flags >> KernelInfo::TEST_FLAG+1)&0xff;
	cpusPerNumaNode = flags?flags:1;
    }
}

SysStatus
PageAllocatorUser::MyRoot::getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize)
{
    node = vp / cpusPerNumaNode;
    nodeSize = cpusPerNumaNode;
    return 0;
}

void
PageAllocatorUser::MyRoot::vpInit(VPNum vp, MemoryMgrPrimitive *memory)
{
    VPNum numaNode, d1;
    getNumaInfo(vp, numaNode, d1);
    PageAllocatorUser *rep;
    uval ptr;
    uval size;

    lock.acquire();
    if(numaNode >= repByNumaNodeMax) {
	uval i, newSize;
	PageAllocatorUser** newArray;
	// must grow array first
	newSize = MIN(AllocCell::MAX_NUMANODES, KernelInfo::CurPhysProcs());
	memory->alloc(ptr, newSize*sizeof(PageAllocatorUser *));
	newArray = (PageAllocatorUser**)ptr;
	for(i=0;i<repByNumaNodeMax;i++) {
	    newArray[i] = repByNumaNode[i];
	}
	for(;i < newSize; newArray[i++] = 0);
	repByNumaNode = newArray;
	repByNumaNodeMax = newSize;
    }
    rep = repByNumaNode[numaNode];
    if (rep == NULL) {
	// need to create allocator for this numa node
	rep = new(memory) PageAllocatorUser();
	repByNumaNode[numaNode] = rep;
	if (numaNode > maxNumaNodeNum) maxNumaNodeNum = numaNode;
	rep->numaNode = numaNode;
	memory->allocAll(ptr, size, PAGE_SIZE);
	rep->init(ptr, size);
    }
    lock.release();
}

void
PageAllocatorUser::MyRoot::replistAdd(uval vp, CObjRep * rep)
{
    // just ignore this call
}

uval
PageAllocatorUser::MyRoot::replistFind(uval vp, CObjRep *& rep)
{
    VPNum numaNode, d1;
    getNumaInfo(vp, numaNode, d1);
    
    // only supports call for vp == current vp; uses numa node
    tassert(vp == Scheduler::GetVP(), err_printf("Non-local vp\n"));
    // rep must already be created
    tassert(repByNumaNode[numaNode]!=NULL, err_printf("oops\n"));
    rep = repByNumaNode[numaNode];
    //err_printf("Returning rep %p for vp %ld numanode %ld\n",
    //       rep, vp, numaNode);
    return 1;
}

void *
PageAllocatorUser::MyRoot::replistNext(void *curr, uval& vp, CObjRep*& rep)
{
    passert(0, err_printf("PageAllocatorUser::MyRoot::replistNext NYI\n"));
    return 0;
}


/* virtual */ uval
PageAllocatorUser::MyRoot::replistRemove(uval vp, CObjRep *& rep)
{
    passert(0, err_printf("PageAllocatorUser::MyRoot::replistRemove NYI\n"));
    return 0;
}

/* virtual */ uval
PageAllocatorUser::MyRoot::replistRemoveHead(CObjRep *& rep)
{
    passert(0,
	    err_printf("PageAllocatorUser::MyRoot::replistRemoveHead NYI\n"));
    return 0;
}

/* virtual */ uval
PageAllocatorUser::MyRoot::replistIsEmpty()
{
    passert(0, err_printf("PageAllocatorUser::MyRoot::replistIsEmpty NYI\n"));
    return 0;
}

/* virtual */ uval
PageAllocatorUser::MyRoot::replistHasRep(CObjRep * rep)
{
    passert(0, err_printf("PageAllocatorUser::MyRoot::replistHasRep NYI\n"));
    return 0;
}

CObjRep *
PageAllocatorUser::MyRoot::createRep(VPNum vp)
{
    CObjRep *rep;
    VPNum numaNode, d1;
    getNumaInfo(vp, numaNode, d1);

    // always pre-created, just find local one
    rep = repByNumaNode[numaNode];
    tassert(rep != NULL, err_printf("no rep\n"));
    return rep;
}

SysStatus
PageAllocatorUser::MyRoot::handleMiss(COSTransObject * &co,
				      CORef ref, uval methodNum)
{
    SysStatus rc;
    rc = CObjRootMultiRepBase::handleMiss(co, ref, methodNum);
    return rc;
}

SysStatus
PageAllocatorUser::allocPages(uval &ptr, uval size, uval flags,
			      VPNum node)
{
    PageAllocatorUser *rep;
    SysStatus rc;
    uval      maxnodes, i;

    if (node != LOCAL_NUMANODE) {
	if (node > COGLOBAL(maxNumaNodeNum) ||
	    ((rep = COGLOBAL(repByNumaNode[node])) == NULL)) {
	    err_printf("PAU::allocPages: Bad node %ld\n", node);
	    return -1;
	}
	return rep->allocPages(ptr, size, flags, LOCAL_NUMANODE);
    }

    tassert(node == LOCAL_NUMANODE, err_printf("Oops\n"));

    rc = PageAllocatorDefault::allocPages(ptr, size, flags, node);

    if (_SGENCD(rc) == ENOMEM && !(flags & PAGEALLOC_FIXED)) {
	// try other reps
	maxnodes = COGLOBAL(maxNumaNodeNum);
	for (i = 0; i <= maxnodes; i++) {
	    rep = COGLOBAL(repByNumaNode[i]);
	    if (rep != NULL && rep != this) {
		rc = PageAllocatorDefault::allocPages(ptr, size, flags, node);
		if (_SUCCESS(rc)) return rc;
	    }
	}
    }
    return rc;
}

SysStatus
PageAllocatorUser::allocPagesAligned(uval &ptr, uval size, uval align,
				     uval offset,
				     uval flags /*=0*/,
				     VPNum node /*=LOCAL_NUMANODE*/)
{
    PageAllocatorUser *rep;
    SysStatus rc;
    uval      maxnodes, i;

    if (node != LOCAL_NUMANODE) {
	if (node > COGLOBAL(maxNumaNodeNum) ||
	    ((rep = COGLOBAL(repByNumaNode[node])) == NULL)) {
	    err_printf("PAU::allocPages: Bad node %ld\n", node);
	    return -1;
	}
	rc = rep->allocPagesAligned(ptr, size, align, offset,
				      flags, LOCAL_NUMANODE);
	return (rc);
    }

    tassert(node == LOCAL_NUMANODE, err_printf("Oops\n"));

    rc = PageAllocatorDefault::allocPagesAligned(ptr, size, align, offset,
						 flags, node);

    if (_SGENCD(rc) == ENOMEM && !(flags & PAGEALLOC_FIXED)) {
	// try other reps
	maxnodes = COGLOBAL(maxNumaNodeNum);
	for (i = 0; i <= maxnodes; i++) {
	    rep = COGLOBAL(repByNumaNode[i]);
	    if (rep != NULL && rep != this) {
		rc = PageAllocatorDefault::allocPagesAligned(ptr, size,
							     align, offset,
							     flags, node);
		if (_SUCCESS(rc)) return rc;
	    }
	}
    }
    return rc;
}

SysStatus
PageAllocatorUser::allocPagesAt(uval paddr, uval size, uval flags)
{
    return PageAllocatorDefault::allocPagesAt(paddr, size, flags);
}


SysStatus
PageAllocatorUser::deallocPages(uval vaddr, uval size)
{
    SysStatus rc;
    PageAllocatorUser *rep;
    tassert(addrToNumaNode(vaddr) == addrToNumaNode(vaddr + size - 1),
	    err_printf("dealloc chunk spans numa nodes: a %lx s %lx\n",
		       vaddr, size));
    rep = this;
    if (!isLocalAddr(vaddr)) {
	//err_printf("Doing remote dealloc of %lx to %ld\n", vaddr,
	//   addrToNumaNode(vaddr));
	tassert(addrToNumaNode(vaddr) <= COGLOBAL(maxNumaNodeNum),
		err_printf("Bad addr: %lx\n", vaddr));
	rep = COGLOBAL(repByNumaNode[addrToNumaNode(vaddr)]);
	tassert(rep != NULL, err_printf("Bad addr: %lx\n", vaddr));
    }
    rc = rep->PageAllocatorDefault::deallocPages(vaddr, size);
    return rc;
}

/*static*/ SysStatus
PageAllocatorUser::ClassInit(VPNum vp, MemoryMgrPrimitive *memory)
{
    SysStatus rc = 0;
    VAllocServicesDefault *vas;

    static PageAllocatorUser::MyRoot *theRootPtr;

    if (vp == 0) {
	theRootPtr = new(memory) MyRoot((RepRef)GOBJ(ThePageAllocatorRef));
    }

    // while we still have the memory, create allocator-required object
    vas = new(memory) VAllocServicesDefault(GOBJ(ThePageAllocatorRef));

    theRootPtr->vpInit(vp, memory);

    /*
     * initialize both pools so common kernel/user code can use
     * pinned storage
     */
    allocLocal[AllocPool::DEFAULT].init(vp, vas, AllocPool::ALL_POOLS);
    return rc;
}

SysStatus
PageAllocatorUser::getMoreMem(uval reqSize)
{
    uval vaddr;
    SysStatus rc;

    _ASSERT_HELD(lock);			// called with lock held

    if (reqSize < PAGE_ALLOCATOR_USER_SIZE) {
	reqSize = PAGE_ALLOCATOR_USER_SIZE;
    }

    rc = bindRegionToNode(numaNode, reqSize, vaddr);

    //err_printf("PageAllocatorUser::getMoreMem -> vaddr %lx, rc %lx \n",
    //       vaddr, rc);

    if (_SUCCESS(rc)) {
	rc = locked_deallocPages(vaddr, reqSize);
	passert(_SUCCESS(rc), err_printf("oops\n"));
    }

    return rc;
}

// creates boot strap memory in right region for per-numa locality
// was static to allow it to be used during early init, but now only
// used internal, so probably could change it back to protected and
// non-static
/* static */ SysStatus
PageAllocatorUser::bindRegionToNode(VPNum node, uval size, uval &vaddr)
{
    SysStatus rc;
    uval vaddr2;
    ObjectHandle frOH;

    rc = StubFRComputation::_CreateFixedNumaNode(frOH, node);
    if (!_SUCCESS(rc)) return rc;

    vaddr = USER_REGIONS_START;
    AllocCell::NodeRegionRange(node, vaddr, vaddr2);

    rc = StubRegionDefault::_CreateFixedLenWithinRangeExt(
	vaddr, vaddr2, size, 0, frOH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::ForkCopy+RegionType::KeepOnExec);
    return rc;
}
