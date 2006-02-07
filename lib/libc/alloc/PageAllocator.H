#ifndef __PAGE_ALLOCATOR_H_
#define __PAGE_ALLOCATOR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocator.H,v 1.25 2005/06/20 06:59:49 cyeoh Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PageAllocator is the abstract base class for
 * page frame allocates.  It is the base for both physical and virtual
 * frame managers.
 *
 * Although this is a frame allocator, byte addresses and lengths are
 * passed as parameters and returned.
 *
 * Allocators derived from this should:
 *
 * Accept free's of pages not previously allocated.  This is the
 * correct way to initialize them.  Allocator metadata is maintained
 * in free pages.  These are used in the order provided during
 * initialization.
 *
 * Accept partial frees of allocated ranges.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>

class PageAllocator : public Obj {
protected:
    PageAllocator() { /* empty body */ }
public:
    // Class error codes, will go in clustered object
    enum {
	INSUFFICIENT_MEM = 1,		// insufficient main memory left
	NOSUCH_MEM,			// no such physical memory avialable
	FRAG_TOO_LARGE,			// fragment requested was too large
	SIZE_NOT_MULT_PAGE		// size not multiple page size
    };

    // alloc flags; default is all zeroes
    enum {
	PAGEALLOC_NOBLOCK    = 0x1,	// don't block and wait if no more mem
	PAGEALLOC_FIXED      = 0x2,	// only allocate from specified vp
	PAGEALLOC_USERESERVE = 0x4	// ignore low-water mark
    };

    // allocate from local numa node
    static VPNum const LOCAL_NUMANODE = VPNum(-1);

    /*
     * The size and alignment arguments are in bytes, and, on an
     * allocation request, are rounded up to a multiple of a page
     * size.  On a free request, the address specified must be the
     * starting address of a page.
     */

    // allocates contiguous region of memory,
    virtual SysStatus allocPages(uval &ptr, uval size, uval flags=0,
				 VPNum node=LOCAL_NUMANODE)=0;

    // allocates memory at specified physical address
    virtual SysStatus allocPagesAt(uval paddr, uval size, uval flags=0)=0;

    // method to get aligned memory - ptr mod(align) = offset
    virtual SysStatus allocPagesAligned(uval &ptr, uval size,
					uval align, uval offset=0,
					uval flags=0,
					VPNum node=LOCAL_NUMANODE)=0;

    // free memory at specified physical address
    virtual SysStatus deallocPages(uval paddr, uval size)=0;

    // free all memory held by a primitive memory manager
    virtual SysStatus deallocAllPages(MemoryMgrPrimitive *memory)=0;

    // amount of memory available
    virtual SysStatus getMemoryFree(uval &avail)=0;

    // debugging output - prints free memory information
    virtual void printFreeMem(void)=0;

    // debugging output - prints free memory information
    virtual void printMemoryFragmentation(void)=0;

    virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize)=0;

    // allocate virtual region that will be in local range for given numa node
    virtual SysStatus bindRegionToNode(VPNum node, uval size, uval &vaddr)=0;
};

#endif /* #ifndef __PAGE_ALLOCATOR_H_ */
