#ifndef __PMALLOC_KERN_H_
#define __PMALLOC_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMallocKern.H,v 1.5 2003/05/06 19:36:37 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * The current version now just acts as a go-between between the gmalloc
 * layer and AllocRegionManagerKern.
 * **************************************************************************/

#include <alloc/PMalloc.H>

class AllocRegionManagerKern;
class DataChunk;

/* Kernel PMalloc class */
class PMallocKern : public PMalloc {
    uval                     mallocID;
    uval                     numaNode;
    AllocRegionManagerKern  *regions;	// region for mem requests
public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba) {
	return ba->alloc(size);
    }
    PMallocKern(uval sizeIndex, uval nid, AllocRegionManagerKern *regions);
    virtual void  pFree(AllocCellPtr topfree);
    virtual DataChunk* pMalloc(uval numBlocks);
    virtual uval getMallocID() { return mallocID; }
    virtual void checkMallocID(void *addr);
};


#endif /* #ifndef __PMALLOC_KERN_H_ */
