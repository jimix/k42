/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VAllocServicesKern.C,v 1.8 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include <mem/PageAllocatorKernPinned.H>
#include "VAllocServicesKern.H"
#include "AllocRegionManagerKern.H"
#include "MemDescKern.H"
#include "PMallocKern.H"

void *
VAllocServicesKern::operator new(size_t size, AllocBootStrapAllocator *ba)
{
    return ba->alloc(size);
}

VAllocServicesKern::VAllocServicesKern()
{
    localAllocRegion = 0;
    nodeAllocRegion  = 0;
}

uval
VAllocServicesKern::nodeID(void *ptr)
{
    return MemDescKern::AddrToMD(uval(ptr))->nodeID();
}

uval
VAllocServicesKern::mallocID(void *ptr)
{
    return MemDescKern::AddrToMD(uval(ptr))->mallocID();
}

void *
VAllocServicesKern::createLocalAllocRegionManager(AllocPool *ap,
						  AllocBootStrapAllocator *ba)
{
    // pool is implicit in this class
    localAllocRegion = new(ba) AllocRegionManagerKern();
    return (void *)(localAllocRegion);
}

void *
VAllocServicesKern::createNodeAllocRegionManager(AllocPool *ap,
						 AllocBootStrapAllocator *ba)
{
    // pool is implicit in this class
    nodeAllocRegion = new(ba) AllocRegionManagerKern();
    return (void *)(nodeAllocRegion);
}

void
VAllocServicesKern::setNodeAllocRegionManager(void *allocRegionManager)
{
    passert((nodeAllocRegion==NULL) || (nodeAllocRegion==allocRegionManager),
	    err_printf("oops\n"));
    nodeAllocRegion = (AllocRegionManagerKern *)allocRegionManager;
}

PMalloc *
VAllocServicesKern::createPMalloc(uval sizeIndex, uval nodeID,
				  void *allocRegion, uval pool,
				  AllocBootStrapAllocator *ba)
{
    PMalloc *retvalue;
    // pool is implicit in this class
    retvalue = new(ba) PMallocKern(sizeIndex, nodeID,
			       (AllocRegionManagerKern *)allocRegion);
    return(retvalue);
}

PageAllocatorRef
VAllocServicesKern::getPageAllocator()
{
    return (PageAllocatorRef)GOBJK(ThePinnedPageAllocatorRef);
}

void
VAllocServicesKern::useLeftOverMem(AllocBootStrapAllocator *ba)
{
    // nothing we can use it for
}

AllocRegionManagerKern *
VAllocServicesKern::getAllocRegion(MemDescKern *md)
{
    uval mallocID = md->mallocID();

    if (allocLocal[AllocPool::PINNED].mallocIDIsLocalStrict(mallocID)) {
	// in theory we could handle this by finding someway to look up
	// the vps allocregionmanagerkern
	tassert(md->nodeID() == Scheduler::GetVP(),
		err_printf("LocalStrict md not local: %ld != %ld\n",
			   md->nodeID(), Scheduler::GetVP()));
	return localAllocRegion;
    } else {
#ifndef NDEBUG
	// in theory we could handle this by finding someway to look up
	// the vps allocregionmanagerkern
	VPNum myNumaNode, d1;
	DREF(getPageAllocator())->getNumaInfo(
	    Scheduler::GetVP(), myNumaNode, d1);
	tassert(md->nodeID() == myNumaNode,
		err_printf("md not node local: %ld != %ld\n",
			   md->nodeID(), myNumaNode));
#endif /* #ifndef NDEBUG */
	return nodeAllocRegion;
    }
}

