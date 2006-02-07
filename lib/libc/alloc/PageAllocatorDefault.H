#ifndef __PAGE_ALLOCATOR_DEFAULT_H_
#define __PAGE_ALLOCATOR_DEFAULT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorDefault.H,v 1.39 2005/06/20 06:59:49 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base class for user-level memory allocators
 * Allocator for pages - although it will actually allocate any
 * size memory you like. See .C for details.
 * **************************************************************************/

//#define PAGEALLOCATOR_SANITY
#include <sys/PageAllocator.H>
#include <alloc/AllocPool.H>
#include <defines/mem_debug.H>
#include <sys/LeakProof.H>


class PageAllocatorDefault : public PageAllocator {

  /* declare PageAllocatorDefault::freePages */
  struct freePages {
      struct freePages  *low;
      struct freePages  *high;
      uval start;
      uval size;
  };

#ifdef DEBUG_MEMORY
    LeakProof leakProof;
#endif /* #ifdef DEBUG_MEMORY */
// #ifdef PAGEALLOCATOR_SANITY
    freePages *stack[900];
// #endif /* #ifdef PAGEALLOCATOR_SANITY */
    // merge two trees - address in low all lower than addresses in high
    freePages *merge(freePages *low,freePages *high);

    void extendFreeList(uval& vaddr, uval& size);

protected:
    PageAllocatorDefault() { /* empty body */ }

    freePages *anchor;
    freePages *freeList;
    uval available;

    typedef BLock LockType;
    BLock lock;     // blocking lock for page allocator

    // virtual method which allows us to modify extend free list
    // behaviour is sub classes... don't expect that introducing a virtual
    // dispatch here is a big deal as it is an infrequent path (famous last
    // words :-))  
    virtual void *getFreePagesMemory(uval& vaddr, uval& size);

    // insert node into tree - note that this may return tree or node
    // depending on sizes
    freePages *add(freePages *tree, freePages *node);
    void allocFromBlock(freePages* cur, freePages** top, uval va, uval sz);

#ifdef PAGEALLOCATOR_SANITY
    void sanity(uval notfree=(uval)-1, uval notfreesize=0);
    void sanitybad();
#else /* #ifdef PAGEALLOCATOR_SANITY */
    inline void sanity (uval notfree=(uval)-1, uval notfreesize=0) {
	(void) notfree; (void) notfreesize; return;};
#endif /* #ifdef PAGEALLOCATOR_SANITY */

    // simplifies expanding space
    SysStatus locked_deallocPages(uval vaddr, uval size);

    // subclasses can override this to get more mem when empty
    virtual SysStatus getMoreMem(uval reqSize) { return -1; }

#ifdef marcdebug
    virtual void marcCheckAvail() { return; }
#endif /* #ifdef marcdebug */

public:
    virtual SysStatus isNotFree(uval notfree, uval notfreesize);

    /*
     * The size and alignment arguments are in bytes, and, on an
     * allocation request, are rounded up to a multiple of a page
     * size.  On a free request, the address specified must be the
     * starting address of a page.
     */

    /*
     * This allocator just provides the basic data structures for
     * managing memory in a homogeneous collection.  Numa support as
     * well as support for various policies are provided at higher levels
     * as front ends (over-wridden methods) to this class.  The flags and
     * vp parameters are maintained here to provide a consistent interface,
     * but are not used.
     */

    // allocates contiguous region of memory,
    virtual SysStatus allocPages(uval &ptr, uval size, uval flags=0,
				 VPNum node=LOCAL_NUMANODE);

    // allocates memory at specified physical address
    virtual SysStatus allocPagesAt(uval paddr, uval size, uval flags=0);

    // method to get aligned memory
    virtual SysStatus allocPagesAligned(uval &ptr, uval size,
					uval align, uval offset=0,uval flags=0,
					VPNum node=LOCAL_NUMANODE);

    // free memory at specified address
    virtual SysStatus deallocPages(uval vaddr, uval size);

    // free all memory held by a primitive memory manager
    virtual SysStatus deallocAllPages(MemoryMgrPrimitive *memory);

    // amount of memory available
    virtual SysStatus getMemoryFree(uval &avail);

    // debugging output - prints free memory information
    virtual void printFreeMem(void);

    // debugging output - prints memory fragmentation information
    virtual void printMemoryFragmentation(void);

    // Meaningless in this context, but needed for test purposes
    virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize);

    // allocate virtual region that will be in local range for given numa node
    virtual SysStatus bindRegionToNode(VPNum node, uval size, uval &vaddr);

    // passed an initial chunk of free memory to get started
    SysStatus init(uval start, uval size);

#ifdef DEBUG_MEMORY
    // calls to leak detector
    void leakProofReset() { leakProof.reset(); };

    SysStatus leakProofNext(uval& handle, LeakProof::LeakData& leakData) {
	return leakProof.next(handle,leakData); };

    void leakProofPrint() { leakProof.print(); };
#endif /* #ifdef DEBUG_MEMORY */
    // test code
    friend void testPageAllocator(uval doDefault);
};


#endif /* #ifndef __PAGE_ALLOCATOR_DEFAULT_H_ */
