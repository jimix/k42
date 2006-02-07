/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionList.C,v 1.71 2005/05/24 02:59:30 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Region definition and management
 * **************************************************************************/

#include "kernIncs.H"
#include <mem/RegionList.H>
#include "mem/memconst.H"
#include "mem/Region.H"
#include <trace/traceMem.h>
#include "sys/memoryMap.H"
#include <alloc/alloc.H>
#include "mem/FRCRW.H"
#include "mem/RegionDefault.H"
#include "mem/RegionPerProcessor.H"
#include "mem/RegionRedZone.H"
#include "proc/ProcessSetKern.H"
#include <meta/MetaProcessServer.H>
#include <cobj/XHandleTrans.H>


template<class ALLOC> RegionHolder<ALLOC>::RegionHolder(
    uval vmbase, uval isize)
{
    TraceOSMemAllocRegHold(vmbase, isize);

    vaddr   = vmbase;
    size    = isize;
    reg     = 0;
    next    = 0;
#ifdef PROTOTYPE_SUBLIST
    subList = 0;
#endif
}

template<class ALLOC> RegionList<ALLOC>::RegionList()
{
    regions = NULL;
    // No regions allowed by default, to force setRegionsBounds() calls.
    regionsStart = 0;
    regionsAllocStart = 0;
    regionsEnd = 0;
}


template<class ALLOC>
void
RegionList<ALLOC>::printRegions()
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    uPrintRegions();
}

template<class ALLOC>
void
RegionList<ALLOC>::uPrintRegions()
{
    RegionHolder<ALLOC> *reg = regions;
    if (!reg) {
	cprintf("no regions\n");
	return;
    }
    while (reg) {
	if (reg->reg) {
	    cprintf("<%lx,%lx> ", reg->vaddr, reg->size);
	} else {
	    cprintf("<%lx,---,%lx> ", reg->vaddr, reg->size);
	}
	reg = reg->next;
    }
    cprintf("\n");
}
#include "PageAllocatorKernPinned.H"
/* a is address we are looking for */
template<class ALLOC>
SysStatus
RegionList<ALLOC>::vaddrToRegion(uval vaddr, RegionRef &reg)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *r = regions;

    while (r) {
	if ((r->vaddr <= vaddr) && (vaddr < (r->vaddr + r->size))) {
#ifdef PROTOTYPE_SUBLIST
            if (r->subList) {
                SysStatus rc = r->subList->vaddrToRegion(vaddr, reg);
                if (_SUCCESS(rc)) {
                    return 0;
                }
            }
#endif
            reg = r->reg;
            return 0;
	}
	r = r->next;
    }
    return _SERROR(1485, 100, EINVAL);
}

#ifdef PROTOTYPE_SUBLIST
template<class ALLOC>
RegionHolder<ALLOC> *
RegionList<ALLOC>::locked_findAndCheckRegion(uval vmbase, uval isize,
                                             SysStatus &rc)
{
    tassert(lock.isLocked(), ;);    
    RegionHolder<ALLOC> *reg = regions;

    while (reg) {
	if ((reg->vaddr <= vmbase) &&
	    ((reg->vaddr + reg->size) >= (vmbase + isize))) {
	    rc=0;
            return reg;
	}
	reg = reg->next;
    }
    rc=_SERROR(1486, 0, EINVAL);
    return 0;
}
#endif

template<class ALLOC>
SysStatus
RegionList<ALLOC>::findRegion(uval vaddr,
			      RegionType::RegionElementInfo& element)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *r = regions;
    while (r) {
	if ((r->vaddr <= vaddr) && (vaddr < (r->vaddr + r->size))) {
	    element.type = r->type;
	    element.start = r->vaddr;
	    element.size = r->size;
	    return 0;
	}
	r = r->next;
    }
    return _SERROR(2831, 100, EINVAL);
}

template<class ALLOC>
SysStatus
RegionList<ALLOC>::findRegion(uval vaddr, RegionRef &reg, 
                              uval &start, uval &size,
			      RegionType::Type &type)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *r = regions;
    while (r) {
	if ((r->vaddr <= vaddr) && (vaddr < (r->vaddr + r->size))) {
            reg  = r->reg;
            type = r->type;
	    start = r->vaddr;
	    size = r->size;
	    return 0;
	}
	r = r->next;
    }
    return _SERROR(2832, 100, EINVAL);
}

#ifdef PROTOTYPE_SUBLIST
template<class ALLOC>
SysStatus
RegionList<ALLOC>::findRegion(uval vaddr, RegionRef &reg, 
                              uval &start, uval &size,
			      RegionType::Type &type,
                              RegionList<ALLOC> **subRegs) 
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *r = regions;
    while (r) {
	if ((r->vaddr <= vaddr) && (vaddr < (r->vaddr + r->size))) {
            reg  = r->reg;
            type = r->type;
	    start = r->vaddr;
	    size = r->size;
            *subRegs = r->subList;
	    return 0;
	}
	r = r->next;
    }
    return _SERROR(2833, 100, EINVAL);
}
#endif

/*
 * used to process all regions in order for some purpose.
 * interface is called with zero.  On each call, it returns
 * the first (smallest address) region which covers or is beyond vaddr.
 * it also sets vaddr to the first address beyond that region, which
 * is a good start for the next search.
 * This is not a very efficient interface but good enough for debugging use.
 * Note that the regions list must always be sorted and eventually support
 * log N cost searches, even though now it is linear.
 *
 * returns 0 when nothing is found
 */
template<class ALLOC>
SysStatus
RegionList<ALLOC>::getNextRegion(uval& vaddr, RegionRef &reg)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *r = regions;
    while (r) {
	if (vaddr < (r->vaddr + r->size)) {
	    reg = r->reg;
	    vaddr = r->vaddr + r->size;
	    return _SRETUVAL(1);
	}
	r = r->next;
    }
    return _SRETUVAL(0);
}

template<class ALLOC>
SysStatus
RegionList<ALLOC>::checkRegion(uval vmbase, uval isize)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *reg = regions;

    while (reg) {
	if ((reg->vaddr <= vmbase) &&
	    ((reg->vaddr + reg->size) >= (vmbase + isize))) {
	    return 0;
	}
	reg = reg->next;
    }
    return _SERROR(1486, 0, EINVAL);
}

template<class ALLOC>
RegionHolder<ALLOC> *
RegionList<ALLOC>::reserveFixedSpace(uval size, SysStatus &rc, uval vmbase)
{
    _ASSERT_HELD(lock);
    RegionHolder<ALLOC> *reg, *tmp;
    rc = 0;

    if ((size == 0) || (size & MIN_PAGE_MASK)) {
	tassertWrn(0, "size (0x%lx) not multiple of MIN_PAGE_SIZE (0x%lx) \n",
		   size, (uval) MIN_PAGE_MASK);
	rc = _SERROR(1487, 10, EINVAL);
	return 0;
    }

    if (vmbase < regionsStart) {
	tassertWrn(0, "Address %lx below start %lx\n",
		   vmbase, regionsStart);
	rc = _SERROR(1488, 10, EINVAL);
	return 0;
    }

    if (!regions) {			// nothing in list
	// Test is constructed to catch case where size is large
	// enough to wrap when added to base
	if ((vmbase>=regionsEnd) || ((regionsEnd-vmbase)<size)) {
	    tassertWrn(0, "Request at %lx for %lx not available\n",
		       vmbase,size);
	    rc = _SERROR(1489, 10, EINVAL);
	    return 0;
	}
	regions = new RegionHolder<ALLOC>(vmbase, size);
	return regions;
    }

    if (regions->vaddr >= vmbase) {	// insert before head
	if (size > (regions->vaddr-vmbase)) {
	    cprintf("RegL::rS region overlaps with existing region: "
		    "<0x%lx, 0x%lx>:\n",
		    vmbase, size);
	    uPrintRegions();
	    rc = _SERROR(1490, 10, EINVAL);
	    return 0;
	}
	reg = new RegionHolder<ALLOC>(vmbase, size);
	reg->next = regions;
	regions = reg;
	return reg;
    }

    reg = regions;			// going after head
    while (1) {
	if ((reg->vaddr + reg->size) > vmbase) {
	    cprintf("RegL::rS bad region, vmbase 0x%lx"
		    "size %lx vaddr 0x%lx\n",
		    reg->vaddr, reg->size, vmbase);
	    uPrintRegions();
	    rc = _SERROR(1491, 10, EINVAL);
	    return 0;
	}
	if (!reg->next) {
	    if ((vmbase>=regionsEnd) || (size > (regionsEnd-vmbase))) {
		tassertWrn(0, "Request at %lx for %lx not available\n",
			   vmbase,size);
		rc = _SERROR(1492, 10, EINVAL);
		return 0;
	    }
	    reg->next = new RegionHolder<ALLOC>(vmbase, size);
	    return reg->next;
	}
	if (reg->next->vaddr > vmbase) { // insert before next
	    if (size > (reg->next->vaddr-vmbase)) {
		err_printf("reg overlaps prev region:"
			   " <0x%lx,0x%lx> <0x%lx>0x%lx:\n",
			   vmbase, size, reg->next->vaddr, vmbase+ size) ;
		uPrintRegions();
		rc = _SERROR(1493, 10, EINVAL);
		return 0;
	    }
	    tmp = new RegionHolder<ALLOC>(vmbase, size);
	    tmp->next = reg->next;
	    reg->next = tmp;
	    return tmp;
	}
	reg = reg->next;
    }
}

template<class ALLOC>
RegionHolder<ALLOC> *
RegionList<ALLOC>::reserveSomeSpace(uval size, SysStatus &rc,
				    uval rangeStart, uval rangeEnd,
				    uval alignment)
{
    _ASSERT_HELD(lock);

    RegionHolder<ALLOC> *reg, *tmp;
    rc = 0;
    uval vmbase;
    uval vaddr;

    if ((size == 0) || (size & MIN_PAGE_MASK)) {
	tassertWrn(0, "size 0x%lx not multiple of MIN_PAGE_SIZE 0x%lx\n",
		   size, (uval) MIN_PAGE_MASK);
	rc = _SERROR(1494, 10, EINVAL);
	return 0;
    }

    if (!alignment) {
	alignment = DFLT_ALIGNMENT;
    } else if (alignment & MIN_PAGE_MASK) {
	tassertWrn(0, "alignment not multiple of MIN_PAGE_SIZE\n");
	rc = _SERROR(1495, 10, EINVAL);
	return 0;
    }

    rangeStart = ALIGN_UP(rangeStart, alignment);
    if (rangeStart >= rangeEnd || size > (rangeEnd - rangeStart) ) {
	tassertWrn(0, "size too big\n");
	rc = _SERROR(1496, 10, EINVAL);
	return 0;
    }

    // try to insert to head of region list
    vmbase = rangeStart;
    if (!regions) {			// nothing in list
	// Test is constructed to catch case where size is large
	// enough to wrap when added to base
	if ((vmbase>=rangeEnd) || ((rangeEnd-vmbase)<size)) {
	    tassertWrn(0, "Request at %lx for %lx not available\n",
		       vmbase,size);
	    rc = _SERROR(1497, 10, EINVAL);
	    return 0;
	}
	regions = new RegionHolder<ALLOC>(vmbase, size);
	return regions;
    }

    reg = 0;
    // There is at least one region - see check above
    // N.B. Check fails is size is large enough to wrap
    if ((regions->vaddr > vmbase) && ((regions->vaddr-vmbase) >= size)) {
	goto found;
    }
    // not before first region, check for holes
    for (reg = regions; reg != 0; reg = reg->next) {
	// continue if hole is all before rangeStart
	if (reg->next && reg->next->vaddr < rangeStart) continue;
	vmbase = reg->vaddr + reg->size;
	if (vmbase < rangeStart) vmbase = rangeStart;
	vmbase = ALIGN_UP(vmbase, alignment);
	vaddr = reg->next == 0 ? rangeEnd : reg->next->vaddr;
	if (vaddr > rangeEnd) vaddr = rangeEnd;
	// is hole big enough
	if ((vaddr > vmbase) && ((vaddr-vmbase) >= size)) {
	    goto found;
	}
    }
    passert(0,err_printf("Can't allocate region\n"); uPrintRegions(); );
    return 0;

found:

    tmp = new RegionHolder<ALLOC>(vmbase, size);
    if (reg) {
	tmp->next = reg->next;
	reg->next = tmp;
    } else {
	tmp->next = regions;
	regions = tmp;
    }
    TraceOSMemRlstAttach(tmp->vaddr, tmp->size, vmbase);
    return tmp;
}


template<class ALLOC>
SysStatus
RegionList<ALLOC>::attachDynamicRegion(
    uval &vaddr, uval size, RegionRef reg,
    RegionType::Type regionType, uval alignment)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    SysStatus    rc;
    RegionHolder<ALLOC> *tmp;

    tmp = reserveSomeSpace(size, rc, regionsAllocStart, regionsEnd, alignment);
    if (rc != 0) {
	return rc;
    }

    vaddr = tmp->vaddr;

    rc = tmp->attachRegion(reg, regionType);

    return rc;
}

#ifdef PROTOTYPE_SUBLIST
template<class ALLOC>
SysStatus
RegionList<ALLOC>::locked_attachFixedSubRegion(uval vaddr, uval size, 
                                               RegionRef reg, 
                                               RegionType::Type regionType,
                                               RegionRef &parent)
{
    tassert(lock.isLocked(), ;);
    SysStatus    rc;
    RegionHolder<ALLOC> *tmp;

    tmp = locked_findAndCheckRegion(vaddr, size, rc);
    if (!_SUCCESS(rc)) {
        return rc;
    }
    parent = tmp->reg;
    return tmp->attachSubRegion(vaddr, size, reg, regionType);
}
#endif

template<class ALLOC>
SysStatus
RegionList<ALLOC>::locked_attachFixedRegion(uval vaddr, uval size, 
                                            RegionRef reg,
                                            RegionType::Type regionType)
{
    tassert(lock.isLocked(), ;);
    SysStatus    rc;
    RegionHolder<ALLOC> *tmp;

    tmp = reserveFixedSpace(size, rc, vaddr);
    if (rc != 0) {
        return rc;
    }
    rc = tmp->attachRegion(reg, regionType);

    return rc;
}


template<class ALLOC>
SysStatus
RegionList<ALLOC>::attachWithinRangeRegion(
    uval &vaddr, uval vaddr2, uval size,
    RegionRef reg, RegionType::Type regionType, uval alignment)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    SysStatus    rc;
    RegionHolder<ALLOC> *tmp;

    // clamp vaddr/vaddr2 to limits
    if (vaddr < regionsStart) vaddr = regionsStart;
    if (vaddr2 > regionsEnd) vaddr2 = regionsEnd;

    /* we have a conflict in setting defaults between wanting segment
     * aligned defaults on many machines, and not breaking alocates in
     * constrained ranges.  We punt by using PAGE_SIZE as default here
     */
    if (!alignment) {
	alignment = PAGE_SIZE;
    }

    tmp = reserveSomeSpace(size, rc, vaddr, vaddr2, alignment);
    if (rc != 0) {
	return rc;
    }

    vaddr = tmp->vaddr;

    rc = tmp->attachRegion(reg, regionType);

    return rc;
}

template<class ALLOC>
SysStatus
RegionList<ALLOC>::detachRegion(RegionRef r)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    RegionHolder<ALLOC> *rh = regions;
    RegionHolder<ALLOC> *prev = 0;

    while (rh) {
	if (rh->reg == r) {		// found it
	    if (prev == 0) {		// head
		regions = rh->next;
	    } else {
		prev->next = rh->next;
	    }
	    delete rh;
	    return 0;
	}
	prev = rh;
	rh = rh->next;
    }
//    tassert(0, err_printf("couldn't find region to delete, ref %lx\n", r);
//		 uPrintRegions());
    return _SERROR(1059, 0, EINVAL);
}


template<class ALLOC>
SysStatus RegionList<ALLOC>::locked_truncate(uval start, uval size,
                                             RegionRef &reg)
{
    RegionHolder<ALLOC> *rh = regions;
    RegionHolder<ALLOC> *prev = 0;
    
    tassert(lock.isLocked(), ;);
    while (rh) {
	if (rh->vaddr <= start &&
	    (rh->vaddr + rh->size) >= (start + size)) {
	    // found the region
	    reg = rh->reg;
	    if (rh->vaddr == start && rh->size == size) {
		if (prev == 0) {		// head
		    regions = rh->next;
		} else {
		    prev->next = rh->next;
		}
		delete rh;
		return 0;
	    } else if (rh->vaddr == start) {
		rh->size -= size;
		rh->vaddr += size;
		return 0;
	    } else if ((rh->vaddr + rh->size) == (start + size)) {
		rh->size -= size;
		return 0;
	    } else {
		// at least for now, only truncate beginning or end
		return _SERROR(2356, 0, EINVAL);
	    }
	}
	prev = rh;
	rh = rh->next;
    }
    return _SERROR(2357, 0, EINVAL);
}

template<class ALLOC>
SysStatus
RegionList<ALLOC>::truncateAndInsertInPlace(uval truncVaddr, uval truncSize,
                                       RegionRef truncReg,
                                       uval newVaddr1, uval newSize1, 
                                       RegionRef newReg1, 
                                       RegionType::Type newType1,
                                       uval newVaddr2, uval newSize2,
                                       RegionRef newReg2,
                                       RegionType::Type newType2)
{
    SysStatus rc=0;
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    
    rc=locked_truncate(truncVaddr, truncSize, truncReg);
    if (_SUCCESS(rc)) {
        rc=locked_attachFixedRegion(newVaddr1, newSize1, newReg1, newType1);
        passertMsg(_SUCCESS(rc),"%ld: truncated worked but first insert"
                                "failed:\n truncVaddr=%p, truncSize=%ld, "
                                "truncReg=%p, newVaddr1=%p, newSize1=%ld, "
                                "newReg1=%p, newVaddr2=%p, newSize2=%ld "
                                "newReg2=%p\n",
                                rc, (void *)truncVaddr, truncSize, truncReg,
                                (void *)newVaddr1, newSize1, newReg1,
                                (void *)newVaddr2, newSize2, newReg2);
        if (newReg2) {
            rc=locked_attachFixedRegion(newVaddr2, newSize2, newReg2, newType2);

            passertMsg(_SUCCESS(rc),"%ld: truncated worked but second insert"
                       "failed:\n truncVaddr=%p, truncSize=%ld, "
                       "truncReg=%p, newVaddr1=%p, newSize1=%ld, "
                       "newReg1=%p, newVaddr2=%p, newSize2=%ld "
                       "newReg2=%p\n",
                       rc, (void *)truncVaddr, truncSize, truncReg,
                       (void *)newVaddr1, newSize1, newReg1,
                       (void *)newVaddr2, newSize2, newReg2);
        }
    }
    return rc;     
}

/*
 * The master of region destroy is the region.  This routine just
 * repeatedly calls destroy.  Each call actually causes a region
 * to disappear (unless it has already disappeared because of a race
 * with another call to destroy of the same region.
 */
template<class ALLOC>
SysStatus
RegionList<ALLOC>::deleteRegionsAll()
{
    RegionHolder<ALLOC> *rh;
    RegionRef reg;
    SysStatus    rc;

    lock.acquire();

    while ((rh = regions)) {
	reg = rh->reg;
	lock.release();
	//err_printf("Destroying region %p\n", reg);
	rc = DREF(reg)->destroy();
	passert(!rc,err_printf("detachProcess failed %p\n", rh));
	lock.acquire();
    }

    lock.release();
    return 0;
}

template<class ALLOC>
SysStatus
RegionList<ALLOC>::preFork(XHandle childXH, ProcessID callerPID)
{
    SysStatus rc = 0;
    ProcessRef pref;
    ObjRef tmp;
    TypeID type;
    rc = XHandleTrans::XHToInternal(childXH, callerPID, MetaObj::attach,
				    tmp, type);
    tassertWrn( _SUCCESS(rc), "woops\n");
    if (!_SUCCESS(rc)) return rc;

    // verify that type is cool
    if (!MetaProcessServer::isBaseOf(type)) {
	tassertWrn(0, "preFork called with XHandle not for a process\n");
	return _SERROR(2713, 0, EINVAL);
    }
    pref = (ProcessRef)tmp;

    // acquire lock at beginning, we are operating on child
    AutoLock<BLock> al(&lock);

    RegionHolder<ALLOC> *next = regions;

    while (next) {
	if ((next->type & ~RegionType::KeepOnExec) !=
	    (RegionType::K42Region & ~RegionType::KeepOnExec)) {
	    rc = DREF(next->reg)->forkCloneRegion(pref, next->type);
	    tassertMsg(_SUCCESS(rc), "woops: %lx\n",rc);
	}
	if (!_SUCCESS(rc)) break;
	next = next->next;
    }
    return rc;
}

template<class ALLOC>
SysStatus
RegionList<ALLOC>::preExec()
{
    RegionHolder<ALLOC> *next, *prev, *destroy, *destroytail, *tmp;
    RegionRef reg;
    SysStatus    rc;

    lock.acquire();
    next = regions;
    prev = 0;
    destroy = 0;
    destroytail = 0;

    while (next) {
	if (!(next->type & RegionType::KeepOnExec)) {
	    tmp = next;
	    // unchain from region list and update next
	    next = next->next;
	    if (prev) {
		prev->next = next;
	    } else {
		regions = next;
	    }
	    // add to destroy.  we work hard to keep the list
	    // in the same order so the detaches below will
	    // go a little faster in the replica case
	    tmp->next = 0;
	    if (destroytail) {
		destroytail->next = tmp;
	    } else {
		destroy = tmp;
	    }
	    destroytail = tmp;
	} else {
	    prev = next;
	    next = next->next;
	}
    }
    lock.release();
    // now destroy the regions
    next = destroy;
    while (next) {
	// get the region ref and destroy the region
	// the region call back to detach will fail silently since
	// its not in the regions list any more
	// N.B. in the ProcessReplicated case, the local replicas
	// of the region list get cleaned up by the detach calls
	reg = next->reg;
	rc = DREF(reg)->destroy();
	tassertMsg(_SUCCESS(rc), "woops\n");
	tmp = next;
	next = next->next;
	delete tmp;
    }
    return 0;
}

/*
 * throw away the list, doing nothing to the regions.
 */
template<class ALLOC>
SysStatus
RegionList<ALLOC>::purge()
{
    RegionHolder<ALLOC> *next, *tmp;

    lock.acquire();
    next = regions;
    regions = 0;
    while (next) {
	tmp = next;
	next = next->next;
	delete tmp;
    }
    lock.release();
    return 0;
}

template<class ALLOC>
void
RegionList<ALLOC>::kosher()
{
    // REALLY TRIVIAL KOSHER TEST, ADD MORE STUFF
    RegionHolder<ALLOC> *reg = regions;
    while (reg) {
	if (reg->reg) {
	    tassert((DREF(reg->reg)->getRef() == reg->reg),
		    err_printf("reference corrupted for ref %p\n", reg->reg));
	} else {
	    cprintf("<%lx,---,%lx> ", reg->vaddr, reg->size);
	}
	reg = reg->next;
    }
}



//template instantiate
template class RegionList<AllocPinnedGlobalPadded>;
template class RegionList<AllocGlobalPadded>;
template class RegionList<AllocGlobal>;
