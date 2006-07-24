#ifndef __VALLOC_SERVICES_H_
#define __VALLOC_SERVICES_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VAllocServices.H,v 1.5 2003/05/06 19:32:48 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Virtual class internal to alloc for accessing alloc services that are
 * dependent on the type of memory being allocated pinned/paged/user/kernel.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>

class AllocPool;
class AllocBootStrapAllocator;
class PMalloc;
class AllocRegionManager;

// base class; just pure virtual methods
class VAllocServices {
public:
    // create type-specific backend allocator component for strictly local mem
    // void * is used as opaque handle
    virtual void *createLocalAllocRegionManager(
	AllocPool *ap, AllocBootStrapAllocator *ba) = 0;

    // create type-specific backend allocator component for node-shared mem
    // void * is used as opaque handle
    virtual void *createNodeAllocRegionManager(
	AllocPool *ap, AllocBootStrapAllocator *ba) = 0;

    // set the value of the node-shared allocator, for non-primary processors
    // in a node (but can be called on primary with value previous created)
    virtual void setNodeAllocRegionManager(void *allocRegionManager) = 0;

    // create type-specific pmalloc component, (void *) is handle from above
    virtual PMalloc *createPMalloc(uval sizeIndex, uval nodeID,
				   void *allocRegion, uval pool,
				   AllocBootStrapAllocator *ba) = 0;

    // returns ref to correct pageallocator for this type of memory
    virtual PageAllocatorRef getPageAllocator() = 0;

    // make use of left-over bootstrapping memory
    virtual void  useLeftOverMem(AllocBootStrapAllocator *ba) = 0;

    // get info about a block of memory
    virtual uval nodeID(void *block) = 0;
    virtual uval mallocID(void *block) = 0;
    
    DEFINE_NOOP_NEW(VAllocServices);
    virtual ~VAllocServices() { }
};

// version for the regular memory (user/paged kernel) class
class VAllocServicesDefault : public VAllocServices {
    AllocRegionManager *localAllocRegion;
    AllocRegionManager *nodeAllocRegion;
    PageAllocatorRef    pageAllocator;
public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba);
    DEFINE_PRIMITIVE_NEW(VAllocServicesDefault);
    // need by kernel/unpinned version
    void * operator new(size_t size, void *place);

    VAllocServicesDefault(PageAllocatorRef pa);

    virtual void *createLocalAllocRegionManager(AllocPool *ap,
						AllocBootStrapAllocator *ba);

    virtual void *createNodeAllocRegionManager(AllocPool *ap,
					       AllocBootStrapAllocator *ba);

    virtual void setNodeAllocRegionManager(void *allocRegionManager);

    virtual PMalloc *createPMalloc(uval sizeIndex, uval nodeID,
				   void *allocRegion, uval pool,
				   AllocBootStrapAllocator *ba);

    virtual PageAllocatorRef getPageAllocator();

    virtual void  useLeftOverMem(AllocBootStrapAllocator *ba);

    virtual uval nodeID(void *block);
    virtual uval mallocID(void *block);
};

#endif /* #ifndef __VALLOC_SERVICES_H_ */
