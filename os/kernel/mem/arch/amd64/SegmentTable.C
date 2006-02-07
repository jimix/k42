/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentTable.C,v 1.7 2002/08/22 16:49:10 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/SegmentTable.H"
#include "proc/Process.H"
#include "mem/SegmentHATPrivate.H"
#include "sys/memoryMap.H"
#include "mem/HATKernel.H"

SysStatus
SegmentTable::Create(SegmentTable*& segmentTable)
{
    SysStatus rc;
    uval ptr;

    /* allocate the PML4 page table as the SegmentTable.
     * Except for the pseudo V=R mappings which have to be entered up front
     * there is never a need to allocate at exception level so that we can
     * block trying to allocate the PML4 page. XXX
     *
     * we could let the allocator retry 
     * rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(ptr,sizeof(pml4),0);
     * but initially at least we want to know when the allocation fails. XXX make it f(NDEBUG)
     * This allccation is global as discussed in SegmentTable.H.
     */
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	allocPages(ptr,PAGE_SIZE,PageAllocator::PAGEALLOC_NOBLOCK);
    if (_FAILURE(rc)) return rc;

     SegmentTable * sp =  (SegmentTable *)ptr;

    /* on amd64 we need to copy kernel segment table into user
     * segment table so kernel will be mapped.
     * The amd64 can't take a page fault at all if the exception data
     * page fault code ... isn't mapped.
     */
    SegmentTable * ksp = exceptionLocal.kernelSegmentTable;

    /* N.B. this copy is not protected by a lock!
     * The segment table is always updated safely - that is, the
     * present bit is only on if the entry is good.  
     * The kernel address space is never deleted or swapped out, so ksp
     * can't disappear, at least initially. 
     * Later we intend to page at least the pte pages, which is probably the
     * most significant part of kernel pmap real memory.
     * We would  hard-pin only
     * what is needed to resolve a fault, i.e. code and data associated
     * to page fault handling and only copy that much, and copy the rest
     * lazily, see below. XXX
     */
    uval pte_index, kernel_index;
    static const PML4 emptypte = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0};
    kernel_index = (KERNEL_BOUNDARY >> VADDR_TO_PML4_SHIFT) & DIRECTORY_INDEX_MASK;

    /* initialize to invalid entries user PML4 entries.
     */
    for(pte_index = 0; pte_index < kernel_index; pte_index++)
	sp->pml4[pte_index] = emptypte;

    /* copy exceptionLocal.kernelSegmentTable PML4 entries 
     * to user level PML4 page table for the kernel address space portion.
     */
    for(;pte_index < (1 << PDP::LogEntriesPerPage);pte_index++) {
	sp->pml4[pte_index] = ksp->pml4[pte_index];
    }

    segmentTable = (SegmentTable *)ptr;

    return 0;
}

void
SegmentTable::changePP()
{
    /* on amd64 we need to copy kernel segment table into user
     * segment table so kernel will be mapped.
     * The amd64 can't take a page fault at all if the exception data
     * page fault code ... isn't mapped.
     */
    SegmentTable * ksp = exceptionLocal.kernelSegmentTable;

    uval pte_index, kernel_index;
    kernel_index =
	(KERNEL_BOUNDARY >> VADDR_TO_PML4_SHIFT) & DIRECTORY_INDEX_MASK;
    /* (re)copy exceptionLocal.kernelSegmentTable PML4 entries 
     * to user level PML4 page table for the kernel address space portion.
     * we are on a new pp - these entries are different
     */
    for(pte_index = kernel_index;
	pte_index < (1 << PDP::LogEntriesPerPage);pte_index++) {
	pml4[pte_index] = ksp->pml4[pte_index];
    }

}

SysStatus
SegmentTable::destroy()
{
    SysStatus rc;
    uval pml4_index, pdp_index;
    uval framePhysAddr;

    /* all mappings have been removed and TLB invalidated by now
     * deallocate the PML4 table, PDP and PDE page table pages.
     * The PTE pages should have been deallocated in the various SegmentHAT::destroy()
     * at least SegmentHATPrivate::destroy() XXX
     * But at that time the pdp entries are still pointing potentially to deallocated
     * PTE pages.
     * N.B destroy is NOT a virtual function
     */
    for(pml4_index = 0; pml4_index < PML4::EntriesPerPage; pml4_index++) {
	PML4 *pml4p = VADDR_TO_PML4_P(this, PML4_TO_VADDR(pml4_index));
	if(pml4p == INVALID_PT_ADRESS)
	    continue;
	if(pml4p->P) {

	    for(pdp_index = 0; pdp_index < (1 << PDP::LogEntriesPerPage); pdp_index++) {
		PDP *pdpp = VADDR_TO_PDP_P(this, PDP_TO_VADDR(pml4_index, pdp_index));
		if(pdpp == INVALID_PT_ADRESS)
		    continue;

		/* deallocate pde pages
		 */
		if(pdpp->P) {
		    framePhysAddr = pdpp->Frame << LOG_PAGE_SIZE;

		    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
			deallocPages(framePhysAddr, PAGE_SIZE);
		    if (rc)
			return rc;
		}
	    }
	    /* now deallocate the pdp page itself.
	     */
	    framePhysAddr = pml4p->Frame << LOG_PAGE_SIZE;
	    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
		deallocPages(framePhysAddr, PAGE_SIZE);
	    if (rc)
	        return rc;
	}
    }
    /* finally deallocate the PML4 page.
     */
    rc = (DREFGOBJK(ThePinnedPageAllocatorRef)->
	  deallocPages((uval)this, PAGE_SIZE));

    return rc;
}

SysStatus
SegmentTable::initKernel()
{
    /* Nothing to do on amd64. XXX check that later pdb XXX
     */
    return 0;
}

/* For each segment of the initial mappings made at boot
 * time (kernel) this routine 
 *	- gets (i.e. create on the first processsor
 * and find on subsequent processor) the SegmentHAT
 *	- connects the existing page table mapping of
 * this segment on this processor to its segmentHAT.
 */

SysStatus
SegmentTable::initKernelSegments(HATKernel * hat, VPNum vp)
{
    uval pml4_index, pdp_index, pde_index;
    uval framePhysAddr, virtAddr;
    SegmentHATRef segmentHATRef;
    SysStatus rc;

    for(pml4_index = 0; pml4_index < PML4::EntriesPerPage; pml4_index++) {
	PML4 *pml4p = VADDR_TO_PML4_P(this, PML4_TO_VADDR(pml4_index));
	if(pml4p == INVALID_PT_ADRESS || pml4p->P == 0)
	    continue;

        /* for each valid pml4 entry look for valid pdp entries in the corresponding pdp page.
         */
        for(pdp_index = 0; pdp_index < (1 << PDP::LogEntriesPerPage); pdp_index++) {
	    PDP *pdpp = VADDR_TO_PDP_P(this, PDP_TO_VADDR(pml4_index, pdp_index));
	    if(pdpp == INVALID_PT_ADRESS || pdpp->P == 0)
	        continue;
    
	    /* for each valid pdp entry look for valid pde entries in the corresponding pde page.
	     */
	    for(pde_index = 0; pde_index < (1 << PDE::LogEntriesPerPage); pde_index++) {
    
	        virtAddr =  PDE_TO_VADDR(pml4_index, pdp_index, pde_index);
	        PDE *pdep =  VADDR_TO_PDE_P(this, virtAddr);
	        if(pdep == INVALID_PT_ADRESS || pdep->P == 0)
		    continue;
	        if(pdep->P) {
    
		    /* found a mapped segment.
		     */
		    framePhysAddr = pdep->Frame << LOG_PAGE_SIZE;
    
		    rc = hat->getSegmentHAT(virtAddr, segmentHATRef);
		    if (rc)
		        return rc;
    
		    rc = ((SegmentHATKernel*)(DREF(segmentHATRef)))->
		        initSegmentHAT(virtAddr, framePhysAddr, vp);
		    if (rc)
		        return rc;
	        }
	    }
        }
    }
    return 0;
}
    
