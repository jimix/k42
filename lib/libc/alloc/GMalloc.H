#ifndef __GMALLOC_H_
#define __GMALLOC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: GMalloc.H,v 1.15 2003/05/06 19:32:47 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * The class declaration for the global layer of the kernel memory
 * allocator.  It gets requests from the layer below (the LMalloc
 * class) and interacts with the layer above (the PMalloc class).  This
 * layer's role is to keep lists of lists of free blocks of a certain size,
 * with each list being the same size as the lists desired by the layer
 * below (the LMalloc class).
 *
 * The class maintains a combined counter and list-pointer which can be
 * updated atomically with a single LL/SC.  When the counter reaches
 * twice numListsTarget, half of the list of lists is moved up to the
 * layer above pointed to by pMalloc.  If a request for more memory
 * finds the list empty, more blocks are retrieved from above and added
 * to the list of lists maintaining the required size of each sublist.
 * **************************************************************************/

#include "alloc/AllocCell.H"
class PMalloc;

class GMalloc {
    BitBLock<AllocCellPtr> freeLists;	// locked counter and list of lists
    uval16        listSize;		// size of each sublist
    uval16        numListsTarget;	// target size of list of lists
    uval16        myPool;		// the pool I'm in
    uval          numaNode;		// numa node this GMalloc
    PMalloc       *pMalloc;		// layer above
#ifdef ALLOC_STATS
    uval          allocs, frees;	// alloc/free requests from below
    uval          allocsUp, freesUp;	// alloc/free requests to above
#endif /* #ifdef ALLOC_STATS */

public:
    typedef enum {
	DO_GO_UP,			// if list too big, move some up
	DO_NOT_GO_UP			// if list too big, do nothing
    } GoUp;

    void * operator new(size_t size, AllocBootStrapAllocator *ba) {
	return ba->alloc(size);
    }
    GMalloc() {
	// option of initializing later
    }
    GMalloc(uval lSize, uval nListsTarget, uval mypool, uval nid, PMalloc *pm) {
	init(lSize, nListsTarget, mypool, nid, pm);
    }
    void         init(uval lSize, uval nListsTarget,
		      uval mypool, uval nid, PMalloc *pm);
    void         gFree(AllocCellPtr togfree, GoUp goUp);
    AllocCellPtr gMalloc();			// Gets a list of MemoryBlocks
    void         print();
    void         printAll();
    PMalloc     *getPMalloc()   { return pMalloc; }
    uval         getListSize()  { return listSize; }
    uval         getNumListsTarget()  { return numListsTarget; }
    void         checkMallocID(void *addr);

#ifdef ALLOC_STATS
    uval  getAllocs()		{ return allocs; }
    uval  getFrees()		{ return frees; }
    uval  getAllocsUp()		{ return allocsUp; }
    uval  getFreesUp()		{ return freesUp; }
#endif /* #ifdef ALLOC_STATS */
};


#endif /* #ifndef __GMALLOC_H_ */
