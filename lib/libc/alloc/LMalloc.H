#ifndef __LMALLOC_H_
#define __LMALLOC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LMalloc.H,v 1.25 2003/08/28 18:11:10 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * This is the lowest layer of the kernel memory allocation subsystem.
 * It handles per-cpu allocation and deallocation requests.  It handles
 * only blocks of a fixed size.  Requests for more blocks or for
 * returning extra blocks are always done in lists of a fixed size
 * (maxCount) to the layer above, the global layer, stored in gMalloc.
 *
 * Two lists are maintained.  Each list is stored as combined counter
 * and pointer.  The counter is stored in the upper bits which are known
 * to be zero (see alloc.h for the macros used for manipulating these
 * bits).  The counter part contains the number of elements in the list.
 * Each next pointer for each element in the list is stored in the same
 * way, containing the number of elements after it in the list.
 * Elements are always taken from the front and added to the front of
 * the list.
 *
 * The reason for combining the counter and the list pointer in one
 * element is to allow insertion and deletions from the list to be done
 * in a single atomic sequence.  It also turns out that deletions from
 * the list require no counter update since the next pointer already has
 * the right count.
 *
 * The reason for two lists is that the aux list is staging area for
 * moving elements to and from the layer above.  When the main list runs
 * out, it just moves the aux list to the main list.  When it is too
 * full, it just moves the list to the aux list and possibly moves the
 * aux list up to the next layer as a unit.
 *
 * Although each element of the list is a block of memory of unknown
 * size, we treat it as an element of type DataChunk, which has a next
 * pointer.  This is uniformly true for all layers, and requires that
 * the block of memory be at least as big as the DataChunk class.
 * **************************************************************************/

#include <alloc/AllocCell.H>

class GMalloc;
class LMalloc;

struct SyncedCellPtr : BitBLock<AllocCellPtr> {
    enum {FAILURE=0, SUCCESS=1, REMOTE=2};

#if defined(TARGET_powerpc)
    void acquire(AllocCellPtr &tmp);
    void release(AllocCellPtr tmp);
#elif defined(TARGET_mips64)
    void acquire(AllocCellPtr &tmp){ BitBLock<AllocCellPtr>::acquire(tmp); }
    void release(AllocCellPtr tmp) { BitBLock<AllocCellPtr>::release(tmp); }
#elif defined(TARGET_amd64)
    void acquire(AllocCellPtr &tmp);
    void release(AllocCellPtr tmp);
#elif defined(TARGET_generic64)
    void acquire(AllocCellPtr &tmp);
    void release(AllocCellPtr tmp);
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

    AllocCell *pop(uval numaNode);

    // returns AllocCellPtr on failure to be pushed up
    uval push(void *el, uval maxCount, AllocCellPtr &tmp);

    // if ptr is not zero, atomically returns it and zeroes
    void getAndZero(AllocCellPtr &tmp);

    uval setIfZero(AllocCellPtr nval);
};

#define OUTOFCLASS
#include __MINC(LMalloc.H)
#undef OUTOFCLASS

class LMalloc {
#define INCLASS
#include __MINC(LMalloc.H)
#undef INCLASS

    friend struct SyncedCellPtr;
    friend void genConstants(void);

    SyncedCellPtr  freeList;		// Combined counter and list pointer
    SyncedCellPtr  auxList;		// auxilliary list
    GMalloc       *gMalloc;		// layer above

    uval           nodeID;		// identifies "my" memory
    uval16         maxCount;		// maximum size of list
    uval16         blockSize;		// block size
    uval16         mallocID;		// mallocID for this lmalloc
    uval16         pool;		// get rid of when pools gone

#ifdef ALLOC_STATS
    uval           allocs, frees, remoteFrees;	// num alloc and frees reqs
    uval           allocsUp, freesUp;	// alloc/free requests to above
#endif /* #ifdef ALLOC_STATS */


    // on an alloc, called if freeList empty or fast conditions not met
    void *slowMalloc();

    // on a free, moves chain up to higher levels
    void moveUp(AllocCellPtr tmp);

    // check that addr is in a block with the right mallocID
    void checkMallocID(void *addr);

public:
    void  init(uval mCount, GMalloc *gm, uval sz, uval mid,
	       uval nid, uval pool);

    void *lMalloc();
    void  lFree(void *tolfree);
    void  lFreeNoCheckLocal(void *tolfree);
    uval  getSize()		{ return blockSize; }
    uval  getMallocID()		{ return mallocID; }
    uval  getNodeID()		{ return nodeID; }
    uval  getMaxCount()		{ return maxCount; }
#ifdef ALLOC_STATS
    uval  getAllocs()		{ return allocs; }
    uval  getFrees()		{ return frees; }
    uval  getRemoteFrees()	{ return remoteFrees; }
    uval  getAllocsUp()		{ return allocsUp; }
    uval  getFreesUp()		{ return freesUp; }
#endif /* #ifdef ALLOC_STATS */
};


#endif /* #ifndef __LMALLOC_H_ */
