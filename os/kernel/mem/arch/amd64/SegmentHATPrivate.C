/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentHATPrivate.C,v 1.16 2003/03/27 17:44:39 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/SegmentHATPrivate.H"
#include "mem/PageAllocatorKern.H"
#include "proc/Process.H"
#include "alloc/AllocPool.H"
#include <scheduler/Scheduler.H>
// #include <cobj/CObjRoot.H>
#include <cobj/CObjRootSingleRep.H>

#define UVALBITS (sizeof(uval)*8)	// should be global
/*
 * The page table lock (at PDE entry level, i.e. for a segment)
 * purpose is to serialize vis a vis the swapper daemon.
 *
 * If we make the page table themselves pageable serialization at
 * that level is not sufficient e.g. because the swapper could
 * try deallocate our PDP page while we are allocating the PTE
 * page.
 *
 * This design is dependant on the fact that an allocated page
 * table page should be reachable from the top PML4.
 *
 * This means that a page can be swapped out only if has no
 * descendant making swapping of PDP, and PDE pages unlikely and
 * we may as well pin those pages which leaves only PTE page
 * spageable. XXX
 *
 * Alternatively we could add a per level page lock and a count
 * of valid entries to swapp the page when the count reaches
 * zero. XXX */

SysStatus
SegmentHATPrivate::Create(SegmentHATRef& ref)
{
    SegmentHATPrivate *shd;
    shd = new SegmentHATPrivate;
    tassertMsg(shd, "new SegmentHATPrivate failed\n");
    if (!shd) {
	return -1;
    }
    /* a SegmentHATPrivate is not really a clustered object
     * because it has only one representative, the same for
     * all vp's.
     */
    ref = (SegmentHATRef)CObjRootSingleRepPinned::Create(shd);
    return 0;
}

/*
 * Kernel differs in that it contains its own ref - this to avoid
 * Page fault regresses in the kernel
 */
SysStatus
SegmentHATKernel::Create(SegmentHATRef& ref)
{
    SegmentHATKernel *shd;
    shd = new SegmentHATKernel;
    tassertMsg(shd, "new SegmentHATPrivate failed\n");
    if (!shd) {
	return -1;
    }
    ref = (SegmentHATRef)CObjRootSingleRepPinned::Create(shd);
    return 0;
}
SysStatus
SegmentHATPrivate::mapSegment(SegmentTable* segp, uval virtAddr, VPNum vp)
{
    SysStatus rc = 0;
    uval framePhys;			// physical address of page table
    uval tmp, tmp1, tmp2;

    /* Use canned protection values - in our design
     * all page protection occurs in page entries
     *          1,      //  present
     *		1,      // RW
     *		1,      // user
     *		0,      // page level write thru
     *
     *		0,      // page level cache disable
     *		1,      // accessed
     *		0,      // ignored, would have been page size XXX
     *		0,      // must be zero
     *
     *		0,      // ignored, would have been global XXX
     *		0,
     *		0,
     *		0,
     *
     *		0;
     */
    PML4 segpml4 = {1,1,1,0, 0,1,0,0, 0,0,0,0, 0};	// an entry in the PML4 page
    PDP segpdp = {1,1,1,0, 0,1,0,0, 0,0,0,0, 0};	// an entry in a PDP page

    /* if already mapped nothing to do.
     */
    if (segp->isSegmentMapped(virtAddr)) return 0;

    if(VADDR_TO_PML4_P(segp, virtAddr)->P == 0) {	// we need allocate all 3 levels

	/* needs to allocate PDP PDE and PTE pages
	 * non-blocking version so that it succeedss or fails atomically
	 * to allocate.
	 * If we cannot allocate at any step we cleanup and bail out.
	 * no need to lcok, the wapper does not know about us.
	 */
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(tmp, sizeof(L2_PageTable),
		       PageAllocator::PAGEALLOC_NOBLOCK);
	if (_FAILURE(rc)) {
	    return rc;
	}

	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(tmp1, sizeof(L3_PageTable),
		       PageAllocator::PAGEALLOC_NOBLOCK);

	if (_FAILURE(rc)) {
	    DREFGOBJK(ThePinnedPageAllocatorRef)->
                deallocPages(tmp,sizeof(L2_PageTable));
	    return rc;
	}

	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(tmp2, sizeof(L4_PageTable),
		       PageAllocator::PAGEALLOC_NOBLOCK);

	if (_FAILURE(rc)) {
	    DREFGOBJK(ThePinnedPageAllocatorRef)->
                deallocPages(tmp,sizeof(L2_PageTable));
	    DREFGOBJK(ThePinnedPageAllocatorRef)->
                deallocPages(tmp1,sizeof(L3_PageTable));
	    return rc;
	}

	/* initialize by zeroing the pages.
	 */
	memset((void *)tmp,0,PAGE_SIZE);
	memset((void *)tmp1,0,PAGE_SIZE);
	memset((void *)tmp2,0,PAGE_SIZE);

	/* set pml4 entry
	 */
	framePhys = PageAllocatorKernPinned::virtToReal(uval(tmp));
        segpml4.Frame = framePhys>>LOG_PAGE_SIZE;
	*VADDR_TO_PML4_P(segp, virtAddr) = segpml4;

	/* set pdp entry
	 */
	framePhys = PageAllocatorKernPinned::virtToReal(uval(tmp1));
        segpdp.Frame = framePhys>>LOG_PAGE_SIZE;
	*VADDR_TO_PDP_P(segp, virtAddr) = segpdp;

    }
    else if(VADDR_TO_PDP_P(segp, virtAddr)->P == 0) {		// we need to allocate 2 levels PDE and PTE

	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(tmp1, sizeof(L3_PageTable),
		       PageAllocator::PAGEALLOC_NOBLOCK);
	if (_FAILURE(rc)) {
	    return rc;
	}

	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(tmp2, sizeof(L4_PageTable),
		       PageAllocator::PAGEALLOC_NOBLOCK);
	if (_FAILURE(rc)) {
	    DREFGOBJK(ThePinnedPageAllocatorRef)->
                deallocPages(tmp1,sizeof(L3_PageTable));
	    return rc;
	}

	memset((void *)tmp1,0,PAGE_SIZE);
	memset((void *)tmp2,0,PAGE_SIZE);

	/* set pdp entry
	 */
	framePhys = PageAllocatorKernPinned::virtToReal(uval(tmp1));
        segpdp.Frame = framePhys>>LOG_PAGE_SIZE;
	*VADDR_TO_PDP_P(segp, virtAddr) = segpdp;

    }
    else if(VADDR_TO_PDE_P(segp, virtAddr)->P == 0) {		// we only need to allocate the PTE level page
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(tmp2, sizeof(L4_PageTable),
		       PageAllocator::PAGEALLOC_NOBLOCK);
	if (_FAILURE(rc)) {
	    return rc;
	}
	memset((void *)tmp2,0,PAGE_SIZE);

    }
    else {
	tassert(VADDR_TO_PML4_P(segp, virtAddr)->P &&
		VADDR_TO_PDP_P(segp, virtAddr)->P  &&
		VADDR_TO_PDE_P(segp, virtAddr)->P, err_printf("should be mapped then \n"));
   }

    L4_PageTable *pt = (L4_PageTable *)tmp2;

    L4PageTableP[vp].x.set(pt);

    /* now we can grab the lock
     * but I believe that a higher level lcck must prevent the swapper
     * racing us on the segment lock, hence there is no need to lcok. XXX check pdb
     */
    L4PageTableP[vp].x.acquire(pt);

    FetchAndAddVolatile(&vpCount, 1);
    tassertMsg(vpToPP[vp] == VPNum(-1), "map already set\n");
    vpToPP[vp] = Scheduler::GetVP();


    framePhys = PageAllocatorKernPinned::virtToReal(uval(tmp2));
    segp->mapSegment(framePhys, virtAddr);

    tassertMsg(VADDR_TO_PML4_P(segp, virtAddr)->P, "shpuld have PML4\n");
    tassertMsg(VADDR_TO_PDP_P(segp, virtAddr)->P, "shpuld have PDP\n");
    tassertMsg(VADDR_TO_PDE_P(segp, virtAddr)->P, "shpuld have PDE\n");

    L4PageTableP[vp].x.release();
    return rc;
}


/*
 * destroy is only called when the address space is being blown away.
 * we assume that the hardware unmapping occurs at a higher level.
 * specifiacally, the whole address space will never be dispatched
 * again.
 *
 * destroy runs on one processor
 *
 * if any storage is allocated dynamically, it must be freed here
 * however we deallocate only the PTE pages associated to that segment,
 * higher levels PDE, PDP ... are deallocated in SegmentTable::destroy().
 */
SysStatus
SegmentHATPrivate::destroy()
{
    VPNum vp;
    uval  i;
    L4_PageTable* pt;

    //err_printf("SegmentHATPrivate %p, destroying vpc %ld\n", getRef(),
    //       vpCount);

    // at the moment, we can't have an Xobjects.  And we syncronize
    // above.  But we use the boilerplate code anyhow - its necessary
    // to get past an assert in deleteUnchecked

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    for (vp = 0; vp < Scheduler::VPLimit; vp++) {
	L4PageTableP[vp].x.acquire(pt);			// this seems to set pt XXX pdb
	if (pt != NULL) {
	    /* by now all entries in the PTE page must be unmapped.
	     */
#ifndef NDEBUG
	    for (i=0; i < PTE::EntriesPerPage; i++) {
		tassertMsg((*pt)[i].P == 0, "all pte entries should be unmapped here \n");
		tassertMsg((*pt)[i].Frame == 0, "all pte entries should be unmapped here \n");
	    }
#endif /* #ifndef NDEBUG */
       /* need to free all the levels of pagetables
        * but only the last (pte) is deallocated here. The other ones
        * will be deallocated in SegmentTable::destroy().
	* Note that the pte pages are still treated as valid by the pde entry one level up.
        */
            DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages((uval)pt,sizeof(L4_PageTable));
	}
	/* following not necessary; just debugging
	 */
	L4PageTableP[vp].x.release(NULL);
	vpToPP[vp] = VPNum(-1);
    }
    return destroyUnchecked();
}


SysStatus
SegmentHATPrivate::detachHAT(HATRef hatREF, uval virtAddr, VPNum vp)
{
    L4_PageTable* pt;
    SegmentTable* segp;
    sval oldCount;

    //err_printf("SegmentHATPrivate %p detachHAT, vpc %ld on %ld\n", getRef(),
    //       vpCount, vp);

    L4PageTableP[vp].x.acquire(pt);			// this seems to set pt XXX pdb

    /* not sure if the pt can be null if we are called, but just assume ok
     * nope, not ok pdb
     */
    if (pt != NULL) {

	DREF(hatREF)->getSegmentTable(vp,segp);
	segp->unmapSegment(virtAddr);

	/* now deallocate L4 page table
	 */
	DREFGOBJK(ThePinnedPageAllocatorRef)->
	    deallocPages((uval)pt,sizeof(L4_PageTable));

	vpToPP[vp] = VPNum(-1);
    }
    else
	tassertMsg(0, " the  L4_PageTable pointer should be non NULL \n");

    L4PageTableP[vp].x.release(NULL);

    oldCount = FetchAndAddVolatile(&vpCount, uval(-1));

    if (oldCount == 1) {
	//err_printf("SegmentHATPrivate %p, vpc=0, doing destroy\n", getRef());
	// remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
	destroyUnchecked();
    }

    return 0;
}

/* the underlying assumption in this routine is that the segment
 * has been mapped and we are only mapping the page into the
 * segment. If I had access to the SegmentTable I could assert
 * it... XXX */

SysStatus
SegmentHATPrivate::mapPage(uval physAddr, uval virtAddr,
			   AccessMode::pageFaultInfo pfinfo,
			   AccessMode::mode access, VPNum vp,
			   uval wasMapped)
{
    SysStatus rc = 0;
    uval invalidate = 0;
    L4_PageTable* pt;
    uval index;

    tassertMsg(vp < Scheduler::VPLimit, "vp (%lx) too large (%lx)\n", vp, Scheduler::VPLimit);
    tassert((virtAddr & PAGE_MASK) == 0,
	     err_printf("virtaddr (%lx) not page aligned\n", virtAddr));
    tassertMsg((physAddr & 0xFFF) == 0, " page address should be aligned \n");

    /* locks page tables for given vp */
    L4PageTableP[vp].x.acquire(pt);	//  this seems to set pt XXX pdb

    index = (virtAddr >> PAGE_ADDRESS_SHIFT) & DIRECTORY_INDEX_MASK;

    if((*pt)[index].P) {
	invalidate = 1;			// really only needed for downgrade
	tassert((*pt)[index].Frame == (physAddr >> PAGE_ADDRESS_SHIFT),
		err_printf("try to remap page to diff frame %lx/%lx \n",
			   (*pt)[index].Frame << PAGE_ADDRESS_SHIFT,
			    physAddr));
    }

    /* for performance, do this as one assignment */
    *(uval *) &(*pt)[index] =
	physAddr | AccessMode::RWPerms(access) | PTE::P_bit;

    //err_printf("MapPage: v %lx p %lx\n", virtAddr, physAddr);

#define __flush_tlb_one(addr) __asm__ __volatile__("invlpg %0": :"m" (*(char *) addr))

    if (invalidate) {
	__flush_tlb_one(virtAddr);

    }

    L4PageTableP[vp].x.release();

    return rc;
}

/*
 * Unmap the pages in segment which are in the region for this vp.
 * segmentEnd and regionEnd are addresses of first byte beyond
 */
SysStatus
SegmentHATPrivate::unmapRange(HATRef hatRef,
			      uval segmentAddr, uval segmentEnd,
			      uval regionAddr, uval regionEnd,
			      VPNum vp)
{
    L4_PageTable* l4pt;
    uval start, end;
    uval flushEntries;
    SegmentTable* segp;
    const uval maxToFlushEntries = 10;	// guess, tradeoff flush entry/wholetlb

    /* -- necessary since end address is one past the end
     */
    segmentEnd--;
    regionEnd--;
    tassertMsg(((segmentEnd - segmentAddr) <= SEGMENT_SIZE), "should be less than segment size\n");

    start = regionAddr > segmentAddr ? regionAddr : segmentAddr;
    end   = regionEnd  < segmentEnd  ? regionEnd  : segmentEnd;

    if ((end - start) <= maxToFlushEntries*PAGE_SIZE || start >= (uval)kernVirtStart) {
	flushEntries = 1;
    } else {
	flushEntries = 0;
    }

    /* hatref can be null if we are to just clean tables, not tlb 	// XXX ask marc pdb
     */
    if (hatRef != NULL) {
	DREF(hatRef)->getSegmentTable(vp, segp);
    } else {
	segp = NULL;			// just to be safe
	/*
	 * that would be ok iff the pde has already been invalidated (zeroed). pdb
	 */
	tassertMsg(0, "should not happen for amd64\n");	// XXX ask marc pdb
    }

    /* could do this earlier to avoid waste above if no pagetable, but
     * then end up holding lock longer.  Not obvious which is more common.
     */
    L4PageTableP[vp].x.acquire(l4pt);
    if(!l4pt) {
	L4PageTableP[vp].x.release();
	return 0;
    }

    tassertMsg(VADDR_TO_PTE_P(segp, start) != (PTE *)NULL, "just checking\n");

    /* first invalidate entries and tlb's
     */
    uval sindex = (start >> LOG_PAGE_SIZE) & (PTE::EntriesPerPage -1);
    uval eindex = (end >> LOG_PAGE_SIZE) & (PTE::EntriesPerPage -1);
    for (uval index = sindex; index <= eindex; index++) {
        if ((*l4pt)[index].P) {
	    *(uval *) &((*l4pt)[index]) = (uval) NULL;

    	    if(flushEntries)
	      {
	        uval vaddr /* address of page to flush */;
	        vaddr = (start & ~(SEGMENT_SIZE - 1)) + index*PAGE_SIZE;
	        __flush_tlb_one(vaddr);
	      }
        }
#ifndef NDEBUG
        else {
	    if(*(uval *) &((*l4pt)[index]))
	        err_printf("I do it explicitly, don't I?, %lx \n", *(uval *) &((*l4pt)[index]));
          }
#endif /* #ifndef NDEBUG */
    }

    /* see if the whole segment goes away.
     */
    if(((start & ~PAGE_MASK) & SEGMENT_MASK) == 0 &&
	(((end + PAGE_SIZE) & ~PAGE_MASK) & SEGMENT_MASK) == 0) {

	tassertMsg((uval)VADDR_TO_PTE_P(segp, start) == (uval)&(*l4pt)[0], "just checking\n");

	DREFGOBJK(ThePinnedPageAllocatorRef)->
	    deallocPages((uval)&(*l4pt)[0],sizeof(L4_PageTable));

	/* blow the pte page mapping away, i.e. the pde
	 * if not already done, assumedly indcated by hatRef == 0.
	 */
	tassertMsg(!flushEntries, "oops\n");
	if(segp)
		*(uval *) VADDR_TO_PDE_P(segp, start) = (uval) NULL;
    }

    /* FIXME: is it correct to release the lock here?  should be, I agree pdb
     */
    L4PageTableP[vp].x.release();

    if (!flushEntries) {
	segp->invalidateAddressSpace();
    }

    return 0;
}

/*
 * Unmap a page,  handle mappings for ALL vp on local physical processor.
 */
SysStatus
SegmentHATPrivate::unmapPage(HATRef hatRef, uval virtAddr)
{
    VPNum vp;
    L4_PageTable* l4pt;
    SegmentTable* segp;

    VPNum pp = Scheduler::GetVP();	// physical processor we are running on

    for (vp = 0; vp < Scheduler::VPLimit; vp++) {

	if (vpToPP[vp] != pp) continue;

	/* powerpc does not lock, under the assumption that the
	 * worst that can happen is have to remap ...  OK only if
	 * the MOD bit cannot be causing problem.  */
	L4PageTableP[vp].x.acquire(l4pt);
	if(!l4pt) {
	    L4PageTableP[vp].x.release();
	    continue;
	}
	// hatref can be null if we are to just clean tables, not tlb
	if (hatRef != NULL) {
	    DREF(hatRef)->getSegmentTable(vp, segp);
	}
	else
	    segp = NULL;

	uval index = (virtAddr >> LOG_PAGE_SIZE) & (PTE::EntriesPerPage -1);
	if ((*l4pt)[index].P) {
	    *(uval *) &((*l4pt)[index]) = (uval) NULL;
	    if(segp)
		    __flush_tlb_one(virtAddr);
	}
#ifndef NDEBUG
	else
//	    tassertMsg(*(uval *) &((*l4pt)[index]) == (uval)NULL, "I do it explicitly, don't I? \n");
	    if(*(uval *) &((*l4pt)[index]))
	        err_printf("I do it explicitly, don't I?, %lx \n", *(uval *) &((*l4pt)[index]));
#endif /* #ifndef NDEBUG */


	L4PageTableP[vp].x.release();
    }

    return 0;
}


SysStatus
SegmentHATKernel::initSegmentHAT(uval virtAddr, uval framePhysAddr, VPNum vp)
{
    uval tmp;

    SysStatus rc = 0;
    L4_PageTable* l4pt;

    FetchAndAddVolatile(&vpCount, 1);

    L4PageTableP[vp].x.acquire(l4pt);

    tassertMsg(!l4pt, "Trying to init page table, but it already exists - vp %ld\n", vp);

    tassertMsg(vpToPP[vp] == VPNum(-1), "map already set\n");
    vpToPP[vp] = Scheduler::GetVP();

    tmp = PageAllocatorKernPinned::realToVirt(framePhysAddr);
    L4PageTableP[vp].x.release((L4_PageTable *)tmp);

    return rc;
}

/* static */ SysStatus
SegmentHATShared::Create(SegmentHATRef& ref)
{
    SegmentHATShared *shd;
    shd = new SegmentHATShared;
    tassertMsg(shd, "new SegmentHATShared failed\n");
    if (!shd) return -1;
    ref = (SegmentHATRef)CObjRootSingleRepPinned::Create(shd);
    //err_printf("Created new SegmentHATShared: %p\n", ref);
    return 0;
}

SysStatus
SegmentHATShared::unmapRange(HATRef hatRef,
			     uval segmentOff, uval segmentOffEnd,
			     uval regionOff, uval regionOffEnd,
			     VPNum vp)
{
    SysStatus rc;

    // for segments, offsets are as good as vaddrs as long as they are
    // equivalently aligned
    rc = SegmentHATPrivate::unmapRange(HATRef(NULL), segmentOff, segmentOffEnd,
				       regionOff, regionOffEnd,
				       Scheduler::GetVP());
    tassertMsg(_SUCCESS(rc), "oops\n");

    // we now just flush the TLB (see SegmentTable::invalidateAddressSpace)
    exceptionLocal.kernelSegmentTable->invalidateAddressSpace();

    return rc;
}

SysStatus
SegmentHATShared::unmapSharedPage(HATRef hatRef, uval vaddr)
{
    // now clear page table entries
    __flush_tlb_one(vaddr);
    return 0;
}

/*
 * N.B. see discussion of pp to vp kludge in header.  But in this
 * case, we use the vp passed to us, since we are only using it
 * to ask the hat for the correct segment table, and the hat's mapping
 * from vp to pp is correct for this use of the shared segment.
 * We must NOT try to use this vp to access our own seg[vp] data
 * structure!
 */
SysStatus
SegmentHATShared::detachHAT(HATRef hatRef, uval virtAddr, VPNum vp)
{
    SegmentTable *segp;

    //err_printf("SegmentHATShared %p detachHAT, vpc %ld on %ld\n", getRef(),
    //       vpCount, vp);

    // we remove ourselves from the calling process's hat/segtable
    DREF(hatRef)->getSegmentTable(vp,segp);
    segp->unmapSegment(virtAddr);

    // we leave all data structures in place, since we are used across
    // multiple processes
    return 0;
}

SysStatus
SegmentHATShared::destroy()
{
    // this really should never be called, since process should always
    // detach us, not destroy us, when region is destroyed

    //err_printf("SegmentHATShared %p, destroy called\n", getRef());

    passertMsg(0, "SegmentHATShared::destroy called\n");

    // nothing to do, since only destroying wrt to calling process
    // real work done in realDestroy, called by owning fcm
    return 0;
}

SysStatus
SegmentHATShared::sharedSegDestroy()
{
    //err_printf("SegmentHATShared %p, destroy called\n", getRef());

    return SegmentHATPrivate::destroy();
}

