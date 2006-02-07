/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VAllocServices.C,v 1.6 2003/05/06 19:32:48 marc Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <alloc/MemoryMgrPrimitive.H>
#include "AllocPool.H"
#include "MemDesc.H"
#include "GMalloc.H"
#include "PMalloc.H"
#include "AllocRegionManager.H"
#include "VAllocServices.H"

void *
VAllocServicesDefault::operator new(size_t size, AllocBootStrapAllocator *ba)
{
    return ba->alloc(size);
}

void *
VAllocServicesDefault::operator new(size_t size, void *place)
{
    return place;
}

VAllocServicesDefault::VAllocServicesDefault(PageAllocatorRef pa)
{
    localAllocRegion = 0;
    nodeAllocRegion = 0;
    pageAllocator = pa;
}

uval
VAllocServicesDefault::nodeID(void *block)
{
    return MemDesc::FindMemDesc(uval(block))->nodeID();
}

uval
VAllocServicesDefault::mallocID(void *block)
{
    return MemDesc::FindMemDesc(uval(block))->mallocID();
}

void *
VAllocServicesDefault::createLocalAllocRegionManager(AllocPool *ap,
						   AllocBootStrapAllocator *ba)
{
    localAllocRegion = new(ba) AllocRegionManager(ap);
    return (void *)(localAllocRegion);
}

void
VAllocServicesDefault::setNodeAllocRegionManager(void *allocRegionManager)
{
    // don't need to do anything for this case
}

void *
VAllocServicesDefault::createNodeAllocRegionManager(AllocPool *ap,
						  AllocBootStrapAllocator *ba)
{
    nodeAllocRegion = new(ba) AllocRegionManager(ap);
    return (void *)(nodeAllocRegion);
}

PMalloc *
VAllocServicesDefault::createPMalloc(uval sizeIndex, uval nodeID,
				     void *allocRegion, uval pool,
				     AllocBootStrapAllocator *ba)
{
    PMalloc *retvalue;
    retvalue = new(ba) PMallocDefault(sizeIndex, nodeID,
				      (AllocRegionManager *)allocRegion,
				      pool);
    return (retvalue);
}

PageAllocatorRef
VAllocServicesDefault::getPageAllocator()
{
    return pageAllocator;
}

void
VAllocServicesDefault::useLeftOverMem(AllocBootStrapAllocator *ba)
{
    uval avail, mem;
    ba->getChunk(mem, avail);
    if (avail != 0) {
	if (nodeAllocRegion != NULL) {
	    nodeAllocRegion->initRegionDescriptors(mem, avail);
	} else {
	    tassertSilent(localAllocRegion != NULL, BREAKPOINT);
	    localAllocRegion->initRegionDescriptors(mem, avail);
	}
    }
}
