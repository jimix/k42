/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentHATPrivate.C,v 1.77 2005/01/08 16:15:56 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/SegmentHATPrivate.H"
#include "mem/PageAllocatorKern.H"
#include "proc/Process.H"
#include "InvertedPageTable.H"
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>
#include "mem/PerfStats.H"

SysStatus
SegmentHATPrivate::Create(SegmentHATRef& ref)
{
    SegmentHATPrivate *shd;
    // FIXME: does this and the ROOT allocation need to be PINNED?????
    shd = new SegmentHATPrivate;
    tassert(shd,err_printf("new SegmentHATPrivate failed\n"));
    if (!shd)
	return -1;

    shd->seg0.lock.init();
    shd->seg0.segDesc = SegmentTable::InvalidSegDesc;
    shd->seg0.pp = VPNum(-1);
    shd->seg0.maxPageOffset = 0;
    shd->seg0.mappedIsTree = 0;
    memset((void*)(&(shd->seg0.mapped)), 0, sizeof(Mapped));

    shd->seg[0] = &shd->seg0;
    for (VPNum vp = 1; vp < Scheduler::VPLimit; vp++) {
	shd->seg[vp] = NULL;
    }

    ref = (SegmentHATRef)CObjRootSingleRep::Create(shd);
    shd->logPageSize = 0;
    shd->vpCount = 0;
    return 0;
}

SysStatus
SegmentHATKernel::Create(SegmentHATRef& ref)
{
    SegmentHATKernel *shd;
    shd = new SegmentHATKernel;
    tassert(shd,err_printf("new SegmentHATPrivate failed\n"));
    if (!shd)
	return -1;

    shd->seg0.lock.init();
    shd->seg0.segDesc = SegmentTable::InvalidSegDesc;
    shd->seg0.pp = VPNum(-1);
    shd->seg0.maxPageOffset = 0;
    shd->seg0.mappedIsTree = 0;
    memset((void*)(&(shd->seg0.mapped)), 0, sizeof(Mapped));

    shd->seg[0] = &shd->seg0;
    for (VPNum vp = 1; vp < Scheduler::VPLimit; vp++) {
	shd->seg[vp] = NULL;
    }

    ref = (SegmentHATRef)CObjRootSingleRepPinned::Create(shd);
    shd->logPageSize = LOG_PAGE_SIZE;	// for now no large pages kernel
    shd->vpCount = 0;
    return 0;
}

// used to initialize kernel address space when segment ids already
// exist
SysStatus
SegmentHATKernel::initSegmentHAT(SegDesc segDesc, VPNum vp)
{
    if (seg[vp] == NULL) {
	SegStruct *newseg;
	newseg = (SegStruct *) allocPinnedGlobalPadded(sizeof *newseg);
	newseg->lock.init();
	newseg->segDesc = SegmentTable::InvalidSegDesc;
	newseg->pp = VPNum(-1);
	newseg->maxPageOffset = 0;
	newseg->mappedIsTree = 0;
	if (!CompareAndStoreSynced((uval*)&seg[vp], uval(NULL), uval(newseg))) {
	    freePinnedGlobalPadded(newseg, sizeof *newseg);
	}
    }

    FetchAndAddVolatile(&vpCount, 1);
    seg[vp]->lock.acquire();
    tassert(!seg[vp]->segDesc.V,
	    err_printf("Trying to initialize already specified segDesc "
		       "VSID %llx vp %ld\n", segDesc.VSID, vp));

    seg[vp]->segDesc = segDesc;
    seg[vp]->pp = vp;			// we use kernel vpnums as ppnums
    seg[vp]->maxPageOffset = SEGMENT_SIZE;
    Mapped *m;
    uval i;
    for (i=0;i<MappedNodes;i++) {
	m = (Mapped*) allocMaybePinned(sizeof(Mapped));
	memset((void*)m, -1, sizeof(Mapped));
	seg[vp]->mapped.mapped[i] = m;
    }
    seg[vp]->mappedIsTree = 1;
    seg[vp]->lock.release();
    return 0;
}

/*
 * In hardware terms, make sure the segment table entry (STE) corresponding to
 *  virtAddr selects the segment represented by this hat.
 * Each vp needs its own space of segid numbers.  If two VP's never
 *  ran on the same physical processor, we could reuse the segDesc values.
 *  But this is not always the case.  Given that there are 2**52 sid values
 *  in PwrPC 64, this should be fine.
 */

SysStatus
SegmentHATPrivate::mapSegment (SegmentTable* segp, uval virtAddr,
			       uval pageSizeArg, VPNum vp)
{
    SysStatus rc = 0;

    if(logPageSize == 0) {
	logPageSize = exceptionLocal.pageTable.getLogPageSize(pageSizeArg);
	tassertMsg(logPageSize != 0, "%lx invalid page size\n", pageSizeArg);
	if (logPageSize == 0) return _SERROR(2805, 0, EINVAL);
    }
    
    if (seg[vp] == NULL) {
	SegStruct *newseg;
	newseg = (SegStruct *) allocMaybePinned(sizeof *newseg);
	newseg->lock.init();
	newseg->segDesc = SegmentTable::InvalidSegDesc;
	newseg->pp = VPNum(-1);
	newseg->maxPageOffset = 0;
	newseg->mappedIsTree = 0;
	if (!CompareAndStoreSynced((uval*)&seg[vp], uval(NULL), uval(newseg))) {
	    freeMaybePinned(newseg, sizeof *newseg);
	}
    }

    seg[vp]->lock.acquire();

    //err_printf("Mapping segment %p\n", getRef());
    if (!seg[vp]->segDesc.V) {
	FetchAndAddVolatile(&vpCount, 1);
	seg[vp]->segDesc.ESID = virtAddr >> LOG_SEGMENT_SIZE;
	seg[vp]->segDesc.T=SegDesc::Memory;
	seg[vp]->segDesc.Ks=0;	        // msrpr 0 -> sup access
	seg[vp]->segDesc.Kp=1;		// msrpr 1 -> user access
	seg[vp]->segDesc.NoExecute=0;	// don't support exec protection
	seg[vp]->segDesc.Class=0;	// don't use class - always 0
	seg[vp]->segDesc.V=1;		// entry valid
	seg[vp]->segDesc.VSID=exceptionLocal.pageTable.allocVSID();
	if (seg[vp]->mappedIsTree) {
	    uval i;
	    for (i = 0; i < MappedNodes; i++) {
		if (seg[vp]->mapped.mapped[i]) {
		    memset((void*)(seg[vp]->mapped.mapped[i]),
			   0, sizeof(Mapped));
		}
	    }
	} else {
	    memset((void*)(&(seg[vp]->mapped)), 0, sizeof(Mapped));
	}
    }

    // Record the physical processor we are mapping this on
    // Because of vp migration, this may have changed, so we update
    // every time
    seg[vp]->pp = Scheduler::GetVP();
    segp->mapSegment(seg[vp]->segDesc, virtAddr, logPageSize);
    seg[vp]->lock.release();
    return rc;
}

/*
 * segments in the pagable part of the kernel address space are mapped
 * here.  we must insert the mapping in the current segment table, which
 * may be borrowed, rather than in the segment table associated with
 * the kernel vp.
 */
SysStatus
SegmentHATKernel::mapSegment (SegmentTable* segp, uval virtAddr,
			      uval pageSizeArg, VPNum vp)
{
    passertMsg(pageSizeArg == PAGE_SIZE, "Large page fault in kernel\n");
    /*
     * We only make SegmentHAT's for pagable kernel segments.
     * The segments with well know mappings are dealt with completely
     * in the fault handler and we never see those addresses
     * in this code.
     * We must insert the mapping into the current, possibly borrowed,
     * segment table, not into the kernel segment table.
     */
    return SegmentHATPrivate::mapSegment(exceptionLocal.currentSegmentTable,
					 virtAddr, pageSizeArg, vp);
}

SysStatus
SegmentHATPrivate::changePP(VPNum vp)
{
    if (seg[vp] != NULL) {
	seg[vp]->lock.acquire();
	seg[vp]->pp = Scheduler::GetVP();
	seg[vp]->lock.release();
    }
    return 0;
}

// for now, make is easy to test alternative.
// will either move this to a kernelinfo bit or decide!
uval SegmentHATPrivate::NoAbandonVsid=1;

SysStatus
SegmentHATPrivate::lockedUnmapSegment(HATRef hatRef, uval virtAddr, VPNum vp)
{
    // to turn off unmap by abandon vsid strategy, always return 1 here
    if (NoAbandonVsid) return 1;
    tassertMsg(seg[vp] != NULL, "NULL SegStruct pointer.\n");
    _ASSERT_HELD(seg[vp]->lock);
    //err_printf("Mapping segment %p\n", getRef());
    seg[vp]->segDesc = SegmentTable::InvalidSegDesc;
    // we remove ourselves from the calling process's hat/segtable
    SegmentTable* segp;
    DREF(hatRef)->getSegmentTable(vp,segp);
    segp->unmapSegment(virtAddr);
    return 0;
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
 */
SysStatus
SegmentHATPrivate::destroy()
{
    // at the moment, we can't have an Xobjects.  And we syncronize
    // above.  But we use the boilerplate code anyhow - its necessary
    // to get past an assert in deleteUnchecked

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    for (VPNum vp = 0; vp < Scheduler::VPLimit; vp++) {
	if (seg[vp] != NULL) {
	    if (seg[vp]->mappedIsTree) {
		uval j;
		for (j = 0; j<MappedNodes; j++) {
		    if (seg[vp]->mapped.mapped[j]) {
			freeMaybePinned(seg[vp]->mapped.mapped[j],
					 sizeof(Mapped));
		    }
		}
	    }
	    if (vp > 0) {
		// seg[0] is part of the object
		freeMaybePinned(seg[vp], sizeof *seg[vp]);
	    }
	}
    }

    return destroyUnchecked();
}

/*
 * this code assumes we don't reuse segment id's.  Thus we don't
 * have to remove mappings to detach
 * since once we stop using the segment id the mappings are dead
 */
SysStatus
SegmentHATPrivate::detachHAT(HATRef hatRef, uval segmentAddr, VPNum vp)
{
    sval oldCount;
    if (seg[vp] == NULL) return 0;
    seg[vp]->lock.acquire();
    if (!seg[vp]->segDesc.V) {
	// vp 0 always has a seg[0] so we need to check
	tassertMsg(seg[vp]->maxPageOffset == 0,
		   "No segment but has pages\n");
	seg[vp]->lock.release();
	return 0;
    }

    /*
     * first unmap all the pages.  note that depending on the setting
     * of NoAbandonVsid this may be a nop or actually clean up the
     * page table.
     * N.B. lockedUnmapRange is NOT virtual.  Be careful.
     */
    lockedUnmapRange(
	hatRef, segmentAddr, segmentAddr+SEGMENT_SIZE,
	segmentAddr, segmentAddr+SEGMENT_SIZE, vp);

    oldCount = FetchAndAddVolatile(&vpCount, uval(-1));
    SegmentTable* segp;
    DREF(hatRef)->getSegmentTable(vp, segp);
    segp->unmapSegment(segmentAddr);
    seg[vp]->lock.release();
    if (oldCount == 1) {
	// remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
	for (VPNum vp = 0; vp < Scheduler::VPLimit; vp++) {
	    if (seg[vp] != NULL) {
		if (seg[vp]->mappedIsTree) {
		    uval j;
		    for (j = 0; j<MappedNodes; j++) {
			if (seg[vp]->mapped.mapped[j]) {
			    freeGlobalPadded(seg[vp]->mapped.mapped[j],
					     sizeof(Mapped));
			}
		    }
		}
		if (vp > 0) {
		    // seg[0] is part of the object
		    freeGlobalPadded(seg[vp], sizeof *seg[vp]);
		}
	    }
	}
	destroyUnchecked();
    }
    return 0;
}

/*
 * In hardware terms, put a mapping into the inverted page table.
 * vaddr is the full virtual address, NOT just the offset in the
 * segment.
 */
/*
 * FIXME
 *
 * Its hard or impossible to track if a vaddr is mapped.  This is
 * because of evictions, some of which happen in translate off code to
 * maintain the pinned mappings.  When a mapping is entered, if that
 * vaddr is already mapped, we must not wind up with two mappings.
 *
 * For now, we keep a conservative list of vaddr's we've seen.  If
 * we've ever seen one, we unmap before mapping.  This is of course
 * terrible.
 *
 * We need a much better data structure for keeping track of mapped
 * vaddr's.  Possibly, we should remember the bucket number for a
 * probably mapped vaddr so we can go right to that bucket.
 *
 * We assume that mapPage is never called for an already mapped
 * virtual address except to convert from RO to RW.  That is, it is
 * never used to change the RPN or other permissions.  To do that,
 * the user must unMap first and then make a new mapping.
 * This assumption allows us to avoid tlbie.
 */

SysStatus
SegmentHATPrivate::mapPage(uval physAddr, uval virtAddr, uval pageSizeArg,
			   AccessMode::pageFaultInfo pfinfo,
			   AccessMode::mode access, VPNum vp,
			   uval wasMapped)
{
    (void)pfinfo;
    SysStatus rc = 0;
    ScopeTime timer(HATMapPage);
    /*
     * Because of unmap/page fault races, it is possible
     * the we see a fault which does not indicate segment fault
     * even though there is no segment descriptor.
     * This happens ipage of the segment is unmapped, for
     * example to do IO, at the same time that a fault on another
     * page is in flight.  The new fault had a segment table entry,
     * which then got removed when we unmapped the last page of the
     * segment.
     * Since its inconvenient to deal with it
     * here, and its very low probablilty, we return success
     * without doing anything.  We'll get another fault with the
     * segment missing bit set in pfinfo.
     */

    if (seg[vp] == NULL) return 0;

    seg[vp]->lock.acquire();

    if (!seg[vp]->segDesc.V) {
	tassertMsg(seg[vp]->maxPageOffset == 0,
		   "No segment but has pages\n");
	seg[vp]->lock.release();
	return 0;
    }

    passertMsg((1ull<<logPageSize) == pageSizeArg,
	       "Only one page size per segment for now, is 0x%lx req 0x%lx\n",
	       uval(1ull<<logPageSize), pageSizeArg);

    uval bitNum, mapNum;
    Mapped* m;
    bitNum = (virtAddr&(SEGMENT_SIZE-1))>>LOG_PAGE_SIZE;
    mapNum = bitNum >> 11;		// assumes 2**16 paged segments
    bitNum &= 0x7ff;
    if (mapNum > 0 && seg[vp]->mappedIsTree == 0) {
	// new page needs tree, don't have one yet, convert in line
	// info to zero'th node
	m = (Mapped*)allocMaybePinned(sizeof(Mapped));
	*m = seg[vp]->mapped;
	memset((void*)(&(seg[vp]->mapped)), 0, sizeof(Mapped));
	seg[vp]->mapped.mapped[0] = m;
	seg[vp]->mappedIsTree = 1;
    }

    if (mapNum > 0 && seg[vp]->mapped.mapped[mapNum] == 0) {
	m = (Mapped*)allocMaybePinned(sizeof(Mapped));
	memset((void*)m, 0, sizeof(Mapped));
	seg[vp]->mapped.mapped[mapNum] = m;
    }

    if (((virtAddr&(SEGMENT_SIZE-1))+PAGE_SIZE) > seg[vp]->maxPageOffset) {
	seg[vp]->maxPageOffset = (virtAddr&(SEGMENT_SIZE-1))+PAGE_SIZE;
    }

    if (seg[vp]->mappedIsTree == 0) {
	m = &(seg[vp]->mapped);
    } else {
	m = seg[vp]->mapped.mapped[mapNum];
    }

    wasMapped = wasMapped | (((m->bits[bitNum>>3])>>(bitNum&7))&1);
    m->bits[bitNum>>3] |= 1<<(bitNum&7);

    /*
     * pfinfo contains a DSISR_NOPTE bit but we can't trust it
     * since another fault may have updated the mappings before
     * we get here processing this fault.  So just use out
     * bits.  This topBit optimization is too course to really
     * help here, but we leave it in in case we improve the
     * record keeping, rather than making pageTable.enter
     * always check for an existing entry.
     * Note that we can take a chance on skipping the tlbie,
     * which is the most expensive operation, since the worse that
     * can happen is another fault, this time with NOPTE not set,
     * to get the mapping right.
     */
    if (wasMapped) {
	uval vsid = seg[vp]->segDesc.VSID;
	exceptionLocal.pageTable.invalidatePage
	    (virtAddr, vsid, logPageSize);
	if (0==(pfinfo&DSISR_NOPTE)) {
	    InvertedPageTable::Tlbie(
		(virtAddr&SEGMENT_MASK)|(vsid<<LOG_SEGMENT_SIZE),
		logPageSize);
	}
    }

    exceptionLocal.pageTable.enterPage
	(virtAddr, physAddr, logPageSize, seg[vp]->segDesc.VSID, access);

    seg[vp]->lock.release();
    return rc;

}

/*
 * Unmap the pages in segment which are in the range.
 * In general, a SegmentHAT does not know what its virtual
 * origin is - so that is passed by the HAT on this call.
 */
SysStatus
SegmentHATPrivate::unmapRange(
    HATRef hatRef, uval segmentAddr, uval segmentEnd,
    uval rangeAddr, uval rangeEnd, VPNum vp)
{
    SysStatus rc;
    if (seg[vp] == 0) return 0;
    seg[vp]->lock.acquire();
    rc = lockedUnmapRange(hatRef, segmentAddr, segmentEnd,
			  rangeAddr, rangeEnd, vp);
    seg[vp]->lock.release();
    return rc;
}

SysStatus
SegmentHATPrivate::lockedUnmapRange(
    HATRef hatRef, uval segmentAddr, uval segmentEnd,
    uval rangeAddr, uval rangeEnd, VPNum vp)
{
    uval s,e;
    uval noTlbie = 0;

    if (!seg[vp]->segDesc.V) {
	// vp 0 always has a seg[0] so we need to check
	tassertMsg(seg[vp]->maxPageOffset == 0,
		   "No segment but has pages\n");
	return 0;
    }

    tassert(seg[vp]->pp == Scheduler::GetVP(),
	    err_printf("unmap called on wrong pp\n"));

    s=rangeAddr>segmentAddr?rangeAddr:segmentAddr;
    e=rangeEnd<segmentEnd?rangeEnd:segmentEnd;

    // is total range beyond anything we've mapped
    if (s >= segmentAddr+seg[vp]->maxPageOffset) {
	return 0;
    }

    // adjust end to avoid area we've never mapped
    if (e >= segmentAddr+seg[vp]->maxPageOffset) {
	// end of unmap range covers everything every mapped
	e = segmentAddr+seg[vp]->maxPageOffset;

	// since we're unmapping up to maxPageOffset we reduce maxPageOffset
	// to the remaining possibly mapped range
	seg[vp]->maxPageOffset = s - segmentAddr;

	// funny looking test to make it easy for semi-smart compilers
	// to common with line above
	if ((s -  segmentAddr) == 0) {
	    // beginning of unmap range is beginning of segment,
	    // so everything will be unmapped
	    if (lockedUnmapSegment(hatRef, segmentAddr, vp) == 0) {
		// we don't fix up mapped here - it happens if we
		// ever remap this segment - see mapSegment
		return 0;
	    }
	}
    }


    const uval maxpnCount=128;
    uval pnCount=0;
    uval32 pnList[maxpnCount];
    // s is the first potential page to unmap
    // e is the first page beyond the range to unmap
    // DO NOT UNMAP e

    // get bit corresponding to beginning of unmap range
    uval bitNum, mapNum, byte;
    Mapped* m;
    bitNum = (s&(SEGMENT_SIZE-1))>>LOG_PAGE_SIZE;
    mapNum = bitNum >> 11;		// assumes 2**16 page segments
    bitNum &= 0x7ff;
    if (seg[vp]->mappedIsTree == 0) {
	tassertMsg((e&(SEGMENT_SIZE-1))<=PAGE_SIZE*256*8,
		   "mapped bit messup\n");
	m = &(seg[vp]->mapped);
    } else {
	m = seg[vp]->mapped.mapped[mapNum];
	if (m == 0) {
	    // map node missing, so skip entire chunk
	    s = (s & -(PAGE_SIZE*256*8)) + PAGE_SIZE*256*8;
	    bitNum = 256*8; // trigger the clause below that increments mapNum
	}
    }

    // first process first byte to get to byte boundary
    while (((bitNum&7) != 0) && s < e) {
	byte = m->bits[bitNum>>3];
	if ((byte>>(bitNum&7))&1) {
	    m->bits[bitNum>>3] &= ~(1<<(bitNum&7));
	    exceptionLocal.pageTable.invalidatePage(
		s, seg[vp]->segDesc.VSID, logPageSize);
	    // we can add without checking for overflow, since we
	    // process at most 7 entries here
	    if (!noTlbie) {
		pnList[pnCount++] = uval32(s);
	    }
	}
	s += PAGE_SIZE;
	bitNum++;
    }

    // now process by byte all 8 page pieces - don't do the last
    // partial byte yet. So we round e down to a multiple of 8 pages
    // 256*8 is the number of bits, and thus pages, covered by
    // one maptree node.
    while (s < (e&-(8*PAGE_SIZE))) {
	tassertMsg((s&(8*PAGE_SIZE-1)) == 0, "alignment fumble\n");
	if (bitNum >= 256*8) {
	    tassertMsg(bitNum == 256*8, "why did we overshoot?\n");
	    // on to next map node
	    mapNum++;
	    m = seg[vp]->mapped.mapped[mapNum];
	    if (m == 0) {
		// map node missing, so skip enough pages and try again
		// N.B. bitNum is still 256*8!
		s += 256*8*PAGE_SIZE;
		continue;
	    }
	    bitNum = 0;
	}
	tassertMsg((bitNum&7) == 0, "alignment fumble\n");
	byte = m->bits[bitNum>>3];
	m->bits[bitNum>>3] = 0;		// we are unmapping all 8 pages
	uval s1;
	s1 = s;
	while (byte) {
	    if (byte & 1) {
		exceptionLocal.pageTable.invalidatePage(
		    s1, seg[vp]->segDesc.VSID, logPageSize);
		if (!noTlbie) {
		    pnList[pnCount++] = uval32(s1);
		    if (pnCount == maxpnCount) {
			InvertedPageTable::TlbieList(
			    pnList, pnCount, logPageSize,
			    seg[vp]->segDesc.VSID);
			pnCount = 0;
		    }
		}
	    }
	    s1 += PAGE_SIZE;
	    byte = byte>>1;
	}
	bitNum += 8;
	s += 8*PAGE_SIZE;
    }

    // now finish the last byte
    if (s<e) {
	if (bitNum >= 256*8) {
	    tassertMsg(bitNum == 256*8, "why did we overshoot?\n");
	    // on to next map node
	    mapNum++;
	    m = seg[vp]->mapped.mapped[mapNum];
	    bitNum = 0;
	}
	if (m != 0) {
	    // only doing the last byte, so just fetch byte once
	    byte = m->bits[bitNum>>3];
	    while (s<e) {
		if ((byte>>(bitNum&7))&1) {
		    byte &= ~(1<<(bitNum&7));
		    exceptionLocal.pageTable.invalidatePage(
			s, seg[vp]->segDesc.VSID ,logPageSize);
		    if (!noTlbie) {
			pnList[pnCount++] = uval32(s);
			if (pnCount == maxpnCount) {
			    InvertedPageTable::TlbieList(
				pnList, pnCount, logPageSize,
				seg[vp]->segDesc.VSID);
			    pnCount = 0;
			}
		    }
		}
		s += PAGE_SIZE;
		bitNum++;
	    }
	    m->bits[bitNum>>3] = byte;
	}
    }

    if (!noTlbie && pnCount) {
	InvertedPageTable::TlbieList(pnList, pnCount, logPageSize,
				     seg[vp]->segDesc.VSID);
    }

    return 0;
}

/*
 * Unmap a page from all vp's on this pp.
 * This will be called on each processor by the higher level.
 */
SysStatus
SegmentHATPrivate::unmapPage(HATRef hatRef, uval virtAddr)
{
    VPNum pp = Scheduler::GetVP();	// process we are running on
    VPNum vp;
    uval vsid;

    for (vp=0;vp<Scheduler::VPLimit;vp++) {
	if (seg[vp] == NULL) continue;
	if (seg[vp]->pp != pp) continue;
	seg[vp]->lock.acquire();		// lock guards mapped info

	uval bitNum, mapNum;
	Mapped* m;
	bitNum = (virtAddr&(SEGMENT_SIZE-1))>>LOG_PAGE_SIZE;
	mapNum = bitNum >> 11;		// assumes 2**16 paged segments
	bitNum &= 0x7ff;
	if (seg[vp]->mappedIsTree == 0) {
	    if (mapNum > 0) {
		seg[vp]->lock.release();
		continue;
	    }
	    m = &(seg[vp]->mapped);
	} else {
	    m = seg[vp]->mapped.mapped[mapNum];
	    if (m == 0) {
		seg[vp]->lock.release();
		continue;
	    }
	}
	if (((m->bits[bitNum>>3])>>(bitNum&7))&1) {
	    m->bits[bitNum>>3] &= ~(1<<(bitNum&7));
	} else {
	    seg[vp]->lock.release();
	    continue;
	}

	// only need to unmap if segment is valid
	if (seg[vp]->segDesc.V) {
	    vsid = seg[vp]->segDesc.VSID;
	    exceptionLocal.pageTable.invalidatePage(
		virtAddr, vsid, logPageSize);
	    InvertedPageTable::Tlbie(
		(virtAddr&SEGMENT_MASK)|(vsid<<LOG_SEGMENT_SIZE), logPageSize);
	}
	seg[vp]->lock.release();
    }

    return 0;
}


SysStatus
SegmentHATShared::Create(SegmentHATRef& ref)
{
    SegmentHATShared *shd;
    // FIXME: does this and the ROOT allocation need to be PINNED?????
    shd = new SegmentHATShared;
    tassert(shd,err_printf("new SegmentHATShared failed\n"));
    if (!shd)
	return -1;

    shd->seg0.lock.init();
    shd->seg0.segDesc = SegmentTable::InvalidSegDesc;
    shd->seg0.pp = VPNum(-1);
    shd->seg0.maxPageOffset = 0;
    shd->seg0.mappedIsTree = 0;
    memset((void*)(&(shd->seg0.mapped)), 0, sizeof(Mapped));

    shd->seg[0] = &shd->seg0;
    for (VPNum vp = 1; vp < Scheduler::VPLimit; vp++) {
	shd->seg[vp] = NULL;
    }

    ref = (SegmentHATRef)CObjRootSingleRep::Create(shd);
    shd->logPageSize = 0;
    shd->vpCount = 0;
    //err_printf("Created new SegmentHATShared: %p\n", ref);
    return 0;
}


/*
 * Given vp to pp kludge, we just unmap on this pp
 */
SysStatus
SegmentHATShared::unmapPage(HATRef hatRef, uval virtAddr)
{
    VPNum vp = Scheduler::GetVP();	// process we are running on
    uval vsid;

    if (seg[vp] == NULL) return 0;
    seg[vp]->lock.acquire();		// lock guards mapped info
    if (!seg[vp]->segDesc.V) {
	// vp 0 always has a seg[0] so we need to check
	tassertMsg(seg[vp]->maxPageOffset == 0,
		   "No segment but has pages\n");
	seg[vp]->lock.release();
	return 0;
    }

    tassertMsg(vp == seg[vp]->pp,
	       "shared segments should have vp %ld == pp %ld\n",
	       vp, seg[vp]->pp);

    uval bitNum, mapNum;
    Mapped* m;
    bitNum = (virtAddr&(SEGMENT_SIZE-1))>>LOG_PAGE_SIZE;
    mapNum = bitNum >> 11;		// assumes 2**16 page segments
    bitNum &= 0x7ff;
    if (seg[vp]->mappedIsTree == 0) {
	if (mapNum != 0) {
	    // can't have seen this address
	    seg[vp]->lock.release();
	    return 0;			// not mapped
	}
	m = &(seg[vp]->mapped);
    } else {
	m = seg[vp]->mapped.mapped[mapNum];
	if (m == 0) {
	    // can't have seen this address
	    seg[vp]->lock.release();
	    return 0;			// not mapped
	}
    }
    if (((m->bits[bitNum>>3])>>(bitNum&7))&1) {
	m->bits[bitNum>>3] &= ~(1<<(bitNum&7));
    } else {
	seg[vp]->lock.release();
	return 0;			// not mapped
    }


    // only need to unmap if segment is valid
    if (seg[vp]->segDesc.V) {
	vsid = seg[vp]->segDesc.VSID;
	exceptionLocal.pageTable.invalidatePage
	    (virtAddr, vsid ,logPageSize);
	InvertedPageTable::Tlbie(
	    (virtAddr&SEGMENT_MASK)|(vsid<<LOG_SEGMENT_SIZE), logPageSize);
    }

    seg[vp]->lock.release();
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
SegmentHATShared::detachHAT(HATRef hatRef, uval segmentAddr, VPNum vp)
{
    SegmentTable *segp;

    //err_printf("SegmentHATShared %p detachHAT, vpc %ld on %ld\n", getRef(),
    //       vpCount, vp);

    // we remove ourselves from the calling process's hat/segtable
    DREF(hatRef)->getSegmentTable(vp,segp);
    segp->unmapSegment(segmentAddr);

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

    passert(0, err_printf("SegmentHATShared::destroy called\n"));

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
