#ifndef __VALLOC_SERVICES_KERN_H_
#define __VALLOC_SERVICES_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: VAllocServicesKern.H,v 1.7 2003/05/06 19:36:37 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Kernel/pinned version of virtual class internal to alloc for accessing
 * alloc services that are dependent on the type of memory being allocated
 * pinned/paged/user/kernel.
 * **************************************************************************/

#include <alloc/VAllocServices.H>
#include <alloc/MemoryMgrPrimitive.H>

class AllocRegionManagerKern;
class PMallocKern;
class MemDescKern;
class MemDescKernBootTime;
class AllocBootStrapAllocator;

class VAllocServicesKern : public VAllocServices {
    AllocRegionManagerKern *localAllocRegion;
    AllocRegionManagerKern *nodeAllocRegion;

public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba);
    DEFINE_PRIMITIVE_NEW(VAllocServicesKern);

    VAllocServicesKern();

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

    // special version to find appropate kernel region manager
    AllocRegionManagerKern *getAllocRegion(MemDescKern *md);
};


#endif /* #ifndef __VALLOC_SERVICES_KERN_H_ */
