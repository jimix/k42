#ifndef __SEGMENT_TABLE_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __SEGMENT_TABLE_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentTable.H,v 1.14 2002/08/21 22:42:33 marc Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * machine dependent object to manage a segment.  This is NOT
 * a clustered object - it is managed by the process HAT
 * This is the amd64 - a segment is represented by a pde entry (2MB)
 * Locking is also done by the caller
 * N.B. This object has NO virtual function
 * **************************************************************************/


#include "mmu.H"
#include "mem/PageAllocatorKern.H"
#include "mem/PageAllocatorKernPinned.H"
#include "exception/ExceptionLocal.H"
#include <sys/memoryMap.H>

class HATKernel;


//This class assumes that the instance data is aligned
//at the beginning of the memory allocated ? pdb is this true and why XXX?

/* A segment is the smallest unit for sharing efficiently by hardware
 * a contiguous virtual range of pages between address spaces. Of cource sharing
 * may be done at the page level, and (large) page sizes is another excellent mechanism
 * to share eficiently between address spaces, but it does waste real storage.
 * Decresing cost of RAM will make it a more and more attractive solution. XXX
 *
 * The smaller the  segment size the better from a granularity standpoint, hence 2MB
 * or a pde worth for amd64.
 * This machine has currently 4 levels of direct PFT and may end up with
 * a couple more.
 * The first level (from the top)  pml4 will be pinned and the rest pageable
 * or may be only the pte pages... may be later XXX (beware of tassert() in checkKernelSegmentFault when pageable).
 *
 * Ideally one would want at least compile time or may be boot time selection of the segment
 * size, as a multiple of the smallest possible (hardware) segment size. XXX
 */

class SegmentTable {

private:
    	PML4 pml4[PML4::EntriesPerPage];

public:

/* build pointers to pml4, pdp, pde entries given a virtual address and a
 * SegmentTable pointer.
 */

static inline  L1_PTE *  vaddr_to_l1pte_p(SegmentTable * topaddr, uval vaddr)
{
        return ((L1_PTE *)(topaddr) + (((vaddr) >> VADDR_TO_PML4_SHIFT) & DIRECTORY_INDEX_MASK));
}

static inline L2_PTE * vaddr_to_l2pte_p(SegmentTable * topaddr, uval vaddr)
{
	return (
	SegmentTable::vaddr_to_l1pte_p((topaddr), (vaddr))->P == 0? (L2_PTE *)INVALID_PT_ADRESS:
        ((L2_PTE *) (PageAllocatorKernPinned::realToVirt(
        SegmentTable::vaddr_to_l1pte_p((topaddr), (vaddr))->Frame << PAGE_ADDRESS_SHIFT))
        + (((vaddr) >> VADDR_TO_PDP_SHIFT) & DIRECTORY_INDEX_MASK)));
}

static inline L3_PTE * vaddr_to_l3pte_p(SegmentTable * topaddr, uval vaddr)
{
	return ( SegmentTable::vaddr_to_l2pte_p(topaddr, vaddr) == (L2_PTE *)INVALID_PT_ADRESS ||
		SegmentTable::vaddr_to_l2pte_p(topaddr, vaddr)->P == 0 ? (L3_PTE *)INVALID_PT_ADRESS:
        ((L3_PTE *) (PageAllocatorKernPinned::realToVirt(
        SegmentTable::vaddr_to_l2pte_p((topaddr), (vaddr))->Frame << PAGE_ADDRESS_SHIFT))
        + (((vaddr) >> LOG_SEGMENT_SIZE) & DIRECTORY_INDEX_MASK)));
}

static inline L4_PTE * vaddr_to_l4pte_p(SegmentTable * topaddr, uval vaddr)
{
	return ( SegmentTable::vaddr_to_l3pte_p(topaddr, vaddr) == (L3_PTE *)INVALID_PT_ADRESS ||
		SegmentTable::vaddr_to_l3pte_p(topaddr, vaddr)->P == 0 ? (L4_PTE *)INVALID_PT_ADRESS:
        ((L4_PTE *) (PageAllocatorKernPinned::realToVirt(                                  \
        vaddr_to_l3pte_p((topaddr), (vaddr))->Frame << PAGE_ADDRESS_SHIFT))                    \
        + ((vaddr >> PAGE_ADDRESS_SHIFT) & DIRECTORY_INDEX_MASK)));
}

#define VADDR_TO_PML4_P(st, vaddr)   SegmentTable::vaddr_to_l1pte_p(st, vaddr)
#define VADDR_TO_PDP_P(st, vaddr)    SegmentTable::vaddr_to_l2pte_p(st, vaddr)
#define VADDR_TO_PDE_P(st, vaddr)    SegmentTable::vaddr_to_l3pte_p(st, vaddr)
#define VADDR_TO_PTE_P(st, vaddr)    SegmentTable::vaddr_to_l4pte_p(st, vaddr)

/*
 * use these macros only to produce addresses of entries thru their indexes.
 */
#define PML4_TO_VADDR(pml4_index)							\
	((pml4_index) << VADDR_TO_PML4_SHIFT)

#define PDP_TO_VADDR(pml4_index, pdp_index)						\
	((pml4_index) << VADDR_TO_PML4_SHIFT) +						\
	((pdp_index) << VADDR_TO_PDP_SHIFT)

#define PDE_TO_VADDR(pml4_index, pdp_index, pde_index)					\
	((pml4_index) << VADDR_TO_PML4_SHIFT) +						\
	((pdp_index) << VADDR_TO_PDP_SHIFT) +						\
	((pde_index) << VADDR_TO_PDE_SHIFT)

    uval isSegmentMapped(uval virtAddr) {
	if( VADDR_TO_PDE_P(this, virtAddr) && VADDR_TO_PDE_P(this, virtAddr)->P)
		return 1;
	else
		return 0;
    }

    /* not localstrict because destruction is done all on one processor:
     * anyway this is essentially used by one processor and therefore
     * in the cache (unless cast out ...)
     */
    DEFINE_PINNEDGLOBALPADDED_NEW(SegmentTable);

    void mapSegment(uval framePhysAddr, uval virtAddr) {
	// Use canned protection values - in our design
	// all page protection occurs in page entries
	PDE segpde =	{1,	// present
			1,	// RW
			1,	// user
			0,	// page level write thru
			0,	// page level cache disable
			1,	// accessed
			0,	// ignored, would have been page size XXX
			0,	// must be zero
			0,	// ignored, would have been global XXX
			0,
			0,
			0};
	segpde.Frame = framePhysAddr>>LOG_PAGE_SIZE;

	tassertMsg(VADDR_TO_PDP_P(this, virtAddr) != (L2_PTE *)INVALID_PT_ADRESS, "PML$ ans PDP should be valid\n");

	*VADDR_TO_PDE_P(this, virtAddr) = segpde;
    }

    void unmapSegment(uval virtAddr) {
	tassertMsg(VADDR_TO_PDE_P(this, virtAddr) != (L3_PTE *)INVALID_PT_ADRESS, "PML4 and PDP  should be valid\n");
	VADDR_TO_PDE_P(this, virtAddr)->P = 0;
	invalidateAddressSpace();
    }


    // When a reference is made to a kernel address while running
    // with an non-kernel segment table
    // we check to see if the kernel had that segment mapped, and if so
    // copy the mapping into the user segment table
    // This is done at exception level
    // This can happen either if the kernel is running with a "borrowed"
    // user segment table OR if a user process touches a kernel address.
    // (Note that the ppcpage is user accessable but in the kernel
    // part of the address space)
    uval checkKernelSegmentFault(uval virtAddr) {
	if (virtAddr >= KERNEL_BOUNDARY && exceptionLocal.kernelSegmentTable->isSegmentMapped(virtAddr)) {
		if(VADDR_TO_PML4_P(this, virtAddr)->P == 0) {
			err_printf(" VADDR_TO_PML4_P is INVALID at 0x%lx \n", (uval) VADDR_TO_PML4_P(this, virtAddr));
			*VADDR_TO_PML4_P(this, virtAddr) = *VADDR_TO_PML4_P(exceptionLocal.kernelSegmentTable, virtAddr);
			return 1;
		}
		else
		    return 0;
	}
	return 0;
    }

    static SysStatus Create(SegmentTable*& segmentTable);

    SysStatus destroy();

    SysStatus initKernel();
    SysStatus initKernelSegments(HATKernel* hat, VPNum vp);
    void changePP();

    uval getcr3() {
	return(PageAllocatorKernPinned::virtToReal((uval) this));
    }

    void switchToAddressSpace() {
	asm volatile ("movq %0, %%cr3" : : "r" (getcr3()));
    }

    /* instead of clearing individual tlb entries, batch changes
     * and then force tlb reload
     * Does not work if any global pages involved XXX
     * and does not work against speculative execution beyond the
     * the global invalidate XXX any safe (and efficient) way to fix? pdb
     * watch out specially for page table modifications.
     */

    void invalidateAddressSpace() {
	uval cr3;
	// invalidate if this is current or kernel
	// if its kernel, current also maps kernel
	if((exceptionLocal.currentSegmentTable == this)
	   || (exceptionLocal.kernelSegmentTable == this)) {
	    asm volatile ("movq %%cr3, %0" : "=r" (cr3) :);
	    asm volatile ("movq %0, %%cr3" : : "r" (cr3));
	}
    }

};
