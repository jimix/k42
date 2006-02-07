#ifndef __PMALLOC_H_
#define __PMALLOC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMalloc.H,v 1.12 2003/05/06 19:32:47 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * The current version now just acts as a go-between between the gmalloc
 * layer and AllocRegionManager.
 * **************************************************************************/

class AllocRegionManager;
class DataChunk;

/* virtual base class to insulate common lower levels from different page
 * allocator/management levels (see VMemDesc also)
 */
class PMalloc {
public:
    // toPFree is a list of lists of blocks
    virtual void pFree(AllocCellPtr topfree) = 0;
    // alloc list of chunks
    virtual DataChunk* pMalloc(uval numBlocks) = 0;
    // mallocID this PMalloc is associated with
    virtual uval getMallocID() = 0;
    virtual void checkMallocID(void *addr) = 0;
};

/* Default PMalloc classic; historically, the original version */
class PMallocDefault : public PMalloc {
    uval                 mallocID;
    uval                 numaNode;
    AllocRegionManager  *regions;	// region for mem requests
    uval                 pool;		// really just for debugging/stats
public:
    void * operator new(size_t size, AllocBootStrapAllocator *ba) {
	return ba->alloc(size);
    }
    PMallocDefault(uval sizeIndex, uval nid,
		   AllocRegionManager *regions, uval pool);
    virtual void  pFree(AllocCellPtr topfree);
    virtual DataChunk* pMalloc(uval numBlocks);
    virtual uval getMallocID() { return mallocID; }
    virtual void checkMallocID(void *addr);
};


#endif /* #ifndef __PMALLOC_H_ */
