/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SegmentTable.C,v 1.51 2005/07/18 21:49:18 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/SegmentTable.H"
#include "mem/PageAllocatorKern.H"
#include "mem/PageAllocatorKernPinned.H"
#include "proc/Process.H"
#include "mem/SegmentHATPrivate.H"
#include "mem/HATKernel.H"
#include <trace/traceMem.h>

// all that's really required is
// for the V bit to be zero
/*static*/ const SegDesc SegmentTable::InvalidSegDesc =      
		       {0, 0, 0, 0, SegDesc::Memory, 0, 0, 0, 0, 0, 0, 0};   

/*static*/
SysStatus
SegmentTable::Create(SegmentTable*& segmentTable)
{
    SysStatus rc;
    switch (exceptionLocal.pageTable.segLoadType()) {
    case InvertedPageTable::HARDWARE:
	rc = SegmentTable_Hardware::Create(segmentTable);
	break;
    case InvertedPageTable::SOFTWARE:
	rc = SegmentTable_SLB::Create(segmentTable);
	break;
    default:
	passertMsg(0, "Unsupported segment table hardware type\n");
	// never reached
	return -1;
    }
#if 0
    err_printf("Create %lx\n", uval(segmentTable));
#endif
    return rc;
}

/*static*/
SysStatus
SegmentTable::CreateKernel(SegmentTable*& segmentTable,
			      MemoryMgrPrimitiveKern *memory,
			      ExceptionLocal* elocal)
{
    switch (elocal->pageTable.segLoadType()) {
    case InvertedPageTable::HARDWARE:
	return SegmentTable_Hardware::CreateKernel(
	    segmentTable, memory, elocal);
    case InvertedPageTable::SOFTWARE:
	return SegmentTable_SLB::CreateKernel(
	    segmentTable, memory, elocal);
    default:
	passertMsg(0, "Unsupported segment table hardware type\n");
	// never reached
	return -1;
    }
}

/*static*/
SysStatus
SegmentTable::CreateBoot(
    SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
    InvertedPageTable *pageTable, uval mapStart, uval mapEnd)
{
    switch (pageTable->segLoadType()) {
    case InvertedPageTable::HARDWARE:
	return SegmentTable_Hardware::CreateBoot(
	    segmentTable, memory, pageTable, mapStart, mapEnd);
    case InvertedPageTable::SOFTWARE:
	return SegmentTable_SLB::CreateBoot(
	    segmentTable, memory, pageTable, mapStart, mapEnd);
    default:
	passertMsg(0, "Unsupported segment table hardware type\n");
	// never reached
	return -1;
    }
}

void
SegmentTable::enterBoltedPage(
    MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
    uval logPgSz, uval vaddr, uval vMapsRAddr)
{
    uval vsid, paddr;

    getVSID(vaddr, vsid);
    paddr = memory->physFromVirt(vMapsRAddr) & ~((1<<logPgSz)-1);
    pageTable->enterBoltedPage(vaddr, paddr, logPgSz, vsid,
			       AccessMode::noUserWriteSup, memory);
}

void
SegmentTable::setupBoltedPages(
    MemoryMgrPrimitiveKern *memory, ExceptionLocal *elocal)
{
    InvertedPageTable *pageTable = &(elocal->pageTable);
    uval logLargestPageSize = pageTable->getLogLargestPageSize();
    enterBoltedPage(memory, pageTable,
		    // 0th large page size (16MB), if any, or base size (4KB)
		    logLargestPageSize,
		    uval(&ExceptionLocal_CriticalPage),
		    uval(&ExceptionLocal_CriticalPage));
    uval boltedPage = uval(elocal->boltedRfiStatePage);
    uval boltedPageSize = elocal->boltedRfiStatePageSize;
    for(uval i=0;i<boltedPageSize; i += (1<<logLargestPageSize)) {
	enterBoltedPage(memory, pageTable, LOG_PAGE_SIZE,
			boltedPage+i, boltedPage+i);
    }
}

void
SegmentTable::setupBootPages(
    MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
    uval mapStart, uval mapEnd)
{
    uval logLargestPageSize = pageTable->getLogLargestPageSize();
    uval pageSize = 1<<logLargestPageSize;
    /*
     * Map low core through end of code.
     */
    extern code _end;
    uval virtBase = PageAllocatorKernPinned::realToVirt(0);
    uval vsid;

    uval vs = virtBase;
    uval ve = (uval) (&_end);		// end of code/data/bss
    uval lastseg = uval(-1);

#if 0
    err_printf("Pre-mapping %lx to %lx = %lx pages logPageSize %lx \n",
	       vs, ve,
	       ((ve+pageSize)>>logLargestPageSize) - (vs>>logLargestPageSize),
	       logLargestPageSize);
#endif

    for (;vs<ve;vs+=pageSize) {
	if ((vs >> LOG_SEGMENT_SIZE) != (lastseg >> LOG_SEGMENT_SIZE)) {
	    mapVmapsRSegment(vs, uval(-1), uval(-1),
			     logLargestPageSize, pageTable);
	    lastseg = vs;
	}
	getVSID(vs, vsid);
	pageTable->enterPage(vs,vs-virtBase, logLargestPageSize,
			    vsid,AccessMode::noUserWriteSup);
    }

    vs = mapStart & -pageSize;		// round down to large page size
    ve = mapEnd;
    lastseg = uval(-1);

#if 0
    err_printf("Pre-mapping %lx to %lx = %lx pages \n",
	       vs, ve,
	       ((ve+pageSize)>>logLargestPageSize) - (vs>>logLargestPageSize));
#endif

    for (;vs<ve;vs+=pageSize) {
	if ((vs >> LOG_SEGMENT_SIZE) != (lastseg >> LOG_SEGMENT_SIZE)) {
	    mapVmapsRSegment(vs, uval(-1), uval(-1),
			     logLargestPageSize, pageTable);
	    lastseg = vs;
	}
	getVSID(vs, vsid);
	pageTable->enterPage(vs,vs-virtBase, logLargestPageSize,
			     vsid,AccessMode::noUserWriteSup);
    }
}

/*
 *N.B. this routine must be idempotent since we use it
 *to reinitialize the cache when a vp moves to a new processor.
 *its result does depend on which processor (e.g. exceptionLocal)
 *it uses.
 */
uval
SegmentTable::setupBoltedSegments(ExceptionLocal *elocal)
{
    uval nextBolted = 0;
    passertMsg((uval(&ExceptionLocal_CriticalPage) & (PAGE_SIZE-1)) == 0,
	       "ExceptionLocal_CriticalPage is not page-aligned.\n");
    passertMsg((uval(&ExceptionLocal_CriticalPageEnd) -
			uval(&ExceptionLocal_CriticalPage)) <= PAGE_SIZE,
	       "ExceptionLocal_CriticalPage is more than a page in size.\n");
    passertMsg((uval(&exceptionLocal) & (PAGE_SIZE-1)) == 0,
	       "exceptionLocal is not page-aligned.\n");
    uval logLargestPageSize = elocal->pageTable.getLogLargestPageSize();
    /* this segment really covers kernel code */
    mapVmapsRSegment(uval(&ExceptionLocal_CriticalPage),
		     nextBolted /* first bolted */,
		     uval(-1) /* use well known vmapsr vsid */,
		     logLargestPageSize,
		     &(elocal->pageTable));
    nextBolted++;
    if ((uval(&ExceptionLocal_CriticalPage) ^ uval(this)) & (-SEGMENT_SIZE)) {
	/* code and this are in different segments, add another bolted
	 * segment.
	 */
	mapVmapsRSegment(
	    uval(this), nextBolted, uval(-1),
	    logLargestPageSize,
	    &(elocal->pageTable));
	nextBolted++;
    }

    /*
     * make sure the bolted pages are covered by a bolted segment
     */
    uval boltedPage = uval(elocal->boltedRfiStatePage);
    uval boltedPageSize = elocal->boltedRfiStatePageSize;
    for(uval i=0;i<boltedPageSize; i += SEGMENT_SIZE) {
	if (findSegmentTableEntry(boltedPage+i) == uval(-1)) {
	    mapVmapsRSegment(
		boltedPage+i, nextBolted, uval(-1),
		logLargestPageSize,
		&(elocal->pageTable));
	    nextBolted++;
	}
    }

    /* bolt the address of kernelPSR and commonPSR for performance,
     * since we know it will be used frequently by every process
     */
    mapVmapsRSegment(uval(&exceptionLocal), nextBolted,
		     elocal->kernelPSRVSID.VSID, LOG_PAGE_SIZE,
		     &(elocal->pageTable));
    nextBolted++;
    mapVmapsRSegment(commonPSpecificRegionStart, nextBolted,
		     elocal->commonPSRVSID.VSID, LOG_PAGE_SIZE,
		     &(elocal->pageTable));
    nextBolted++;
    return nextBolted;
}

/*
 * On processor setting up an intial segment table for the next.
 * The only relevant mappings are the code segment
 * and the transfer area segment if different.  this is in the transfer
 * area.
 */
uval
SegmentTable::setupBootSegments(InvertedPageTable *pageTable)
{
    uval nextBolted = 0;
    uval logLargestPageSize = pageTable->getLogLargestPageSize();
    /* this segment really covers kernel code */
    mapVmapsRSegment(uval(&ExceptionLocal_CriticalPage),
		     nextBolted /* first bolted */,
		     uval(-1) /* use well known vmapsr vsid */,
		     logLargestPageSize,
		     pageTable);
    nextBolted++;
    if ((uval(&ExceptionLocal_CriticalPage) ^ uval(this)) & (-SEGMENT_SIZE)) {
	/* code and this are in different segments, add another bolted
	 * segment.
	 */
	mapVmapsRSegment(
	    uval(this), nextBolted, uval(-1),
	    logLargestPageSize,
	    pageTable);
	nextBolted++;
    }
    return nextBolted;
}

#include "MemDescKern.H"

/*static*/ SysStatus
SegmentTable_Hardware::Create(SegmentTable*& segmentTable)
{
    SysStatus rc;
    SegmentTable_Hardware *sp = new SegmentTable_Hardware;
    if (sp == NULL) {
	return _SERROR(2233, 0, ENOMEM);
    }
    uval sdt;
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	allocPagesAligned(sdt, HW_SEGTAB_SIZE, HW_SEGTAB_SIZE);
    passertMsg(_SUCCESS(rc), "Memory Panic %lx\n", rc);
    sp->segDescTable = (SegDesc *)sdt;
    sp->init();
    segmentTable = sp;
    return 0;
}

/*static*/ SysStatus
SegmentTable_Hardware::CreateKernel(
    SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
    ExceptionLocal* elocal)
{
    SegmentTable_Hardware *sp = new(memory) SegmentTable_Hardware;
    if (sp == NULL) {
	return _SERROR(2233, 0, ENOMEM);
    }
    uval sdt;
    /* primitive allocator panics if no memory*/
    memory->alloc(sdt, HW_SEGTAB_SIZE, HW_SEGTAB_SIZE);
    sp->segDescTable = (SegDesc *)sdt;
    sp->initKernel(memory, elocal);
    segmentTable = sp;
    return 0;
}

/*static*/ SysStatus
SegmentTable_Hardware::CreateBoot(
    SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
    InvertedPageTable *pageTable, uval mapStart, uval mapEnd)
{
    SegmentTable_Hardware *sp = new(memory) SegmentTable_Hardware;
    if (sp == NULL) {
	return _SERROR(2233, 0, ENOMEM);
    }
    uval sdt;
    /* primitive allocator panics if no memory*/
    memory->alloc(sdt, HW_SEGTAB_SIZE, HW_SEGTAB_SIZE);
    sp->segDescTable = (SegDesc *)sdt;
    sp->initBoot(memory, pageTable, mapStart, mapEnd);
    segmentTable = sp;
    return 0;
}

void
SegmentTable_Hardware::init(void)
{
    // Clear a HW segment table to 0s, thus resetting all the
    // "valid" bits (as well as clearing out all the other stuff).
    PageCopy::Memset0(segDescTable, HW_SEGTAB_SIZE);
    setupBoltedSegments(&exceptionLocal);
}

void
SegmentTable_Hardware::initKernel(
    MemoryMgrPrimitiveKern *memory, ExceptionLocal* elocal)
{
    // Clear a HW segment table to 0s, thus resetting all the
    // "valid" bits (as well as clearing out all the other stuff).
    // Can't use PageCopy::Memset0() because KernelInfo isn't accessible yet.
    memset(segDescTable, 0, HW_SEGTAB_SIZE);
    setupBoltedSegments(elocal);
    setupBoltedPages(memory, elocal);
}

void
SegmentTable_Hardware::initBoot(
    MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
    uval mapStart, uval mapEnd)
{
    // Clear a HW segment table to 0s, thus resetting all the
    // "valid" bits (as well as clearing out all the other stuff).
    // Can't use PageCopy::Memset0() because KernelInfo isn't accessible yet.
    memset(segDescTable, 0, HW_SEGTAB_SIZE);
    setupBootSegments(pageTable);
    setupBootPages(memory, pageTable, mapStart, mapEnd);
}

/*
 * insert a kernel mapping.  If bolted is nonzero, turn on the bolted
 * bit in the segment table for machines that have a segment table.
 * If bolted is nonzero, put the entry into the (bolted-1) slot of
 * the SLB if this machine has an SLB.  Note that this assumes that
 * elsewhere in the design, slots filled with bolted entries are
 * in fact preserved across all SLB manipulations.
 *
 * This code is often called when exceptionLocal may NOT be used.
 * That is why it has funny extra params.
 *
 * N.B. bolted LS is in +1 representation, so 0 can
 * indicate "don't do it"
 *
 * bolted - if not -1 bolt the entry, in slot bolted if software slb
 * LS - if non-zero, user LS-1 as large page index and set large page bit
 */
void
SegmentTable_Hardware::mapVmapsRSegment(
    uval vaddr, uval bolted, uval vsid, uval logPageSize,
    InvertedPageTable *pageTable)
{
    uval steg_index, i;
    tassertMsg(logPageSize == LOG_PAGE_SIZE,
	       "This machine does not support large pages\n");
    
    if(vsid == uval(-1)) {
	vsid = (vaddr >> LOG_SEGMENT_SIZE)|0x0001000000000ul;
    }

    /*
     * is there already an entry?
     */
    i = findSegmentTableEntry(vaddr);
    if (i != uval(-1)) {
	segDescTable[i].Bolted |= (bolted!=uval(-1))?1:0;
	return;
    } else {
    
	/*
	 * Find a free entry.  Start at the end of the STEG so that normal
	 * searches won't have to skip over the bolted entry all the time.
	 */
	// Isolate 5-bit STEG index, shifted left to allow for 8 STEs/STEG
	steg_index = (vaddr >> (LOG_SEGMENT_SIZE - 3)) & 0xF8;
	for (i = steg_index+7; i >= steg_index; i--) {
	    if (!segDescTable[i].V) break;
	}
	passertMsg(i >= steg_index, "No free STE\n");
    }

    /*
     * Just initialize the STE.  No locking or synchronization necessary
     * because this segment table isn't in use yet.  Bolted vaddrs are
     * always mapping-fault addresses, so we reproduce the
     * mapping-fault handler's VSID construction algorithm unless
     * a vsid is provided.
     */
    segDescTable[i].ESID = vaddr >> LOG_SEGMENT_SIZE;
    segDescTable[i].T = SegDesc::Memory;
    segDescTable[i].Ks = 0;	        // msrpr 0 -> sup access
    segDescTable[i].Kp = 1;		// msrpr 1 -> user access
    segDescTable[i].NoExecute = 0;	// don't support exec protection
    segDescTable[i].Class = 0;		// don't use class - always 0
    segDescTable[i].V = 1;		// entry valid
    segDescTable[i].Bolted = (bolted!=uval(-1))?1:0; // entry bolted
    segDescTable[i].VSID = vsid;
}

/*
 * return index if entry that maps vaddr, or -1 if not mapped
 */
uval
SegmentTable_Hardware::findSegmentTableEntry(uval vaddr)
{
    // Isolate 5-bit STEG index, shifted left to allow for 8 STEs/STEG
    uval steg_index;
    uval i;
    steg_index = (vaddr >> (LOG_SEGMENT_SIZE - 3)) & 0xF8;
    for (i=steg_index; i<steg_index+8; i++) {
	if (segDescTable[i].V &&
	    segDescTable[i].ESID == (vaddr >> LOG_SEGMENT_SIZE)) {
	    return i;
	}
    }
    steg_index = steg_index ^ 0xF8;	// secondary hash
    for (i=steg_index; i<steg_index+8; i++) {
	if (segDescTable[i].V &&
	    segDescTable[i].ESID == (vaddr >> LOG_SEGMENT_SIZE)) {
	    return i;
	}
    }
    return uval(-1);
}

/*
 * Find an empty slot in the Segment Table.
 * If no such exists, evict an entry at random.
 */
uval
SegmentTable_Hardware::findEmptySegmentTableEntry(uval vaddr)
{
    uval i, steg_index, steg_index2, rand_bits;

    // Isolate 5-bit STEG index, shifted left to allow for 8 STEs/STEG
    steg_index = (vaddr >> (LOG_SEGMENT_SIZE - 3)) & 0xF8;
    for (i = steg_index; i < steg_index + 8; i++) {
        if (!segDescTable[i].V) return i;
    }

    steg_index2 = steg_index ^ 0xF8;

    for (i = steg_index2; i < steg_index2 + 8; i++) {
        if (!segDescTable[i].V) return i;
    }

    // No free entry.  Choose one at random for eviction.

    asm ("mftb %0" : "=r" (rand_bits));	// read the time base register
    i = steg_index + (rand_bits >> 11) & 7;	// pick out bits 50-52
    
    if (segDescTable[i].Bolted) {
	// Randomly-selected entry is bolted.  Find any non-bolted entry.
	for (i = steg_index; i < steg_index + 8; i++) {
	    if (!segDescTable[i].Bolted) break;
	}
	passertMsg(i < steg_index + 8, "No non-bolted STE\n");
    }
    exceptionLocal.pageTable.countSegEvict();

    return i;	// return index of empty entry
}

/*
 * Store the two halves of a segment table entry, in the proper order,
 * with the necessary memory synchronization
 */
void
SegmentTable_Hardware::storeSegmentTableEntry(sval index, SegDesc segDesc)
{
    // Treat segtable entries as pairs of dwords, like the hardware does.
    uval *from = (uval *) &segDesc;
    uval *to   = (uval *) &segDescTable[index];

    /*
     * We use one of the STE reserved bits as a lock for synchronization
     * with the mapping fault handler in lolita.S.  We disable hardware
     * interrupts to ensure that we have at most one STE locked at a time.
     * The mapping fault handler will avoid the locked STE.  If it didn't,
     * it might change the STE after we store into the VSID dword and
     * before we store into the ESID dword, leading to disaster when we
     * finally get to update the ESID dword.
     */
    to[0] = SegDesc::STE_LOCKED_INVALID;	// clear V bit, set lock bit
    asm volatile ("dcbst 0,%0; sync" : : "r" (to));
    to[1] = from[1];    			// store VSID dword
    asm volatile ("dcbst 0,%0; sync" : : "r" (to));
    to[0] = from[0];    			// store ESID dword (with V
					    // bit set and lock bit clear)
    asm volatile ("dcbst 0,%0; sync" : : "r" (to));
}

/*
 * Map a segment.
 * Caller passes a complete SegDesc - but we fill in the
 * ESID based on the vaddr passed.
 */
void
SegmentTable_Hardware::mapSegment(SegDesc segDesc, uval vaddr, uval logPageSize)
{
    tassertMsg(
	(segDesc.VSID < 0x1000000000) ||
	(vaddr >= exceptionLocal.kernelRegionsEnd),
	"mapping %lx using kernel fixed vsid %llx\n",
	vaddr, segDesc.VSID);

    disableHardwareInterrupts();
    segDesc.ESID = vaddr >> LOG_SEGMENT_SIZE;

    uval index = findSegmentTableEntry(vaddr);
    if (index == uval(-1)) {
	index = findEmptySegmentTableEntry(vaddr);
    }
    // Use canned protection values - in our design
    // all page protection occurs in page entries
    storeSegmentTableEntry(index, segDesc);
    enableHardwareInterrupts();
}

/*
 * unmap segment
 */
void
SegmentTable_Hardware::unmapSegment(uval vaddr)
{
    disableHardwareInterrupts();
    // Delete entry from HW seg table, if such an entry exists
    uval index = findSegmentTableEntry (vaddr);
    if (index != uval(-1)) {
	storeSegmentTableEntry (index, InvalidSegDesc);
    }
    // Flush slb entry whether or not mapping was found in seg table.  The
    // mapping-fault handler and findEmptySegmentTableEntry evict seg table
    // entries without flushing the slb.
    // clear all but esid so class bit is always 0
    vaddr &= -SEGMENT_SIZE;
    asm volatile ("slbie %0" : : "r" (vaddr));
    enableHardwareInterrupts();
}

/*
 * Get the VSID corresponding to an effective address.
 * This involves searching the STEG for the proper STE.
 *
 */
SysStatus 
SegmentTable_Hardware::getVSID(uval vaddr, uval &retVSID)
{
    uval index = findSegmentTableEntry(vaddr);
    if (index != uval(-1)) {  // entry for vaddr was found
	retVSID = segDescTable[index].VSID;
	return 0;
    }

    /*
     * getVSID is used in a styalized way during init, and should
     * never fail because of that */
    
    passertMsg(0, "getVSID failed %lx\n", vaddr);
    return _SERROR (1443, 0, ENOENT);	// no matching entry found
}

SysStatus
SegmentTable_Hardware::destroy()
{
    DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(
	uval(segDescTable), HW_SEGTAB_SIZE);
    delete this;
    return 0;
}

/*static*/ SysStatus
SegmentTable_SLB::Create(SegmentTable*& segmentTable)
{
    /*
     * Assuming we've uttered the proper magical incantation, "new" will
     * get us pinned, page-aligned storage for the segment table, ensuring
     * that the HW segment table is page-aligned.
     */
    SegmentTable_SLB *sp;
    if (exceptionLocal.realModeMemMgr != NULL) {
	sp = new(exceptionLocal.realModeMemMgr) SegmentTable_SLB;
    } else {
	sp = new SegmentTable_SLB;
    }
    if (sp == NULL) {
	return _SERROR(2233, 0, ENOMEM);
    }
    sp->init();
    segmentTable = sp;
    return 0;
}

/*static*/ SysStatus
SegmentTable_SLB::CreateKernel(
    SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
    ExceptionLocal* elocal)
{
    /*
     * Assuming we've uttered the proper magical incantation, "new" will
     * get us pinned, page-aligned storage for the segment table, ensuring
     * that the HW segment table is page-aligned.
     */
    SegmentTable_SLB *sp = new(memory) SegmentTable_SLB;
    if (sp == NULL) {
	return _SERROR(2233, 0, ENOMEM);
    }
    sp->initKernel(memory, elocal);
    segmentTable = sp;
    return 0;
}


/*static*/ SysStatus
SegmentTable_SLB::CreateBoot(
    SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
    InvertedPageTable *pageTable, uval mapStart, uval mapEnd)
{
    /*
     * Assuming we've uttered the proper magical incantation, "new" will
     * get us pinned, page-aligned storage for the segment table, ensuring
     * that the HW segment table is page-aligned.
     */
    SegmentTable_SLB *sp = new(memory) SegmentTable_SLB;
    if (sp == NULL) {
	return _SERROR(2233, 0, ENOMEM);
    }
    sp->initBoot(memory, pageTable, mapStart, mapEnd);
    segmentTable = sp;
    return 0;
}

void
SegmentTable_SLB::init(void)
{
    /*
     * entry 1 is conditionally filled in, but unconditionally loaded,
     * so start it out as invalid
     */
    SLBCache[1].vsid = 0;
    SLBCache[1].esid = 1;		// invalid, correct index
    //we don't zero the cache - high water mark cacheMax is always respected
    // setup depends on virtual address of "this" so is different
    // for every process - can't just copy from kernel - must rerun setup
    numBolted = setupBoltedSegments(&exceptionLocal);
    cacheMax = cacheNext = slbNext = numBolted;
}

void
SegmentTable_SLB::initKernel(
    MemoryMgrPrimitiveKern *memory, ExceptionLocal* elocal)
{
    /*
     * entry 1 is conditionally filled in, but unconditionally loaded,
     * so start it out as invalid
     */
    SLBCache[1].vsid = 0;
    SLBCache[1].esid = 1;		// invalid, correct index
    //we don't zero the cache - high water mark cacheMax is always respected
    numBolted = setupBoltedSegments(elocal);
    cacheMax = cacheNext = slbNext = numBolted;
    setupBoltedPages(memory, elocal);
}

void
SegmentTable_SLB::initBoot(
    MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
    uval mapStart, uval mapEnd)
{
    /*
     * entry 1 is conditionally filled in, but unconditionally loaded,
     * so start it out as invalid
     */
    SLBCache[1].vsid = 0;
    SLBCache[1].esid = 1;		// invalid, correct index
    numBolted = setupBootSegments(pageTable);
    cacheMax = cacheNext = slbNext = numBolted;
    setupBootPages(memory, pageTable, mapStart, mapEnd);
}

/*
 * insert a kernel mapping.  If bolted is nonzero, turn on the bolted
 * bit in the segment table for machines that have a segment table.
 * If bolted is nonzero, put the entry into the (bolted-1) slot of
 * the SLB if this machine has an SLB.  Note that this assumes that
 * elsewhere in the design, slots filled with bolted entries are
 * in fact preserved across all SLB manipulations.
 *
 * This code is often called when exceptionLocal may NOT be used.
 * That is why it has funny extra params.
 *
 * N.B. both bolted and LS are in +1 representation, so 0 can
 * indicate "don't do it"
 *
 * bolted - if not -1 bolt the entry, in slot bolted if software slb
 * vsid - if not -1, the vsid, otherwise conventionally kernel vsid
 * LS - if non-zero, user LS-1 as large page index and set large page bit
 */
void
SegmentTable_SLB::mapVmapsRSegment(
    uval vaddr, uval bolted, uval vsid, uval logPageSize,
    InvertedPageTable *pageTable)
{
    uval i;

    if(vsid == uval(-1)) {
	vsid = (vaddr >> LOG_SEGMENT_SIZE)|0x0001000000000ul;
    }
    if(bolted != uval(-1)) {
	i = bolted;
    } else {
	/*
	 * is there already an entry?
	 */
	i = findSegmentTableEntry(vaddr);
	if (i != uval(-1)) {
	    return;
	}
	i = nextCacheNdx();
    }
    
    vaddr &= -SEGMENT_SIZE;
    // Use canned protection values - in our design
    // all page protection occurs in page entries
    uval vsidword = (vsid << 12) | 0x400;	// Ks=0 Kp=1 NLC=0
    uval esid = vaddr | 0x8000000;
    if(logPageSize != LOG_PAGE_SIZE) {
	//      L bit    LS value
	vsidword |= 0x100 | pageTable->getLS(logPageSize);
    }

    SLBCache[i].vsid = vsidword;
    SLBCache[i].esid = esid | i;
    /*
     * This method never operates on the current active segment
     * table.  It would be nice to assert that, but exceptionLocal
     * may not be available yet to check.
     */
}

/*
 * return index of cache entry that maps vaddr, or -1 if not mapped
 */
uval
SegmentTable_SLB::findSegmentTableEntry(uval vaddr)
{
    uval i;
    for (i=0; i<cacheMax; i++) {
	tassertMsg(SLBCache[i].esid.index == i, "Bad esid index field\n");
	if (SLBCache[i].esid.V &&
	     SLBCache[i].esid.ESID == (vaddr >> LOG_SEGMENT_SIZE)) {
	    return i;
	}
    }
    return uval(-1);
}

/*
 * Map a segment.
 * Caller passes a complete SegDesc - but we fill in the
 * ESID based on the vaddr passed.
 */
void
SegmentTable_SLB::mapSegment(SegDesc segDesc, uval vaddr, uval logPageSize)
{
    uval vsid, esid;
    tassertMsg(
	(segDesc.VSID < 0x1000000000) ||
	(vaddr >= exceptionLocal.kernelRegionsEnd),
	"mapping %lx using kernel fixed vsid %llx\n",
	vaddr, segDesc.VSID);

    disableHardwareInterrupts();
    vaddr &= -SEGMENT_SIZE; // clear all but esid so class bit is 0
    segDesc.ESID = vaddr >> LOG_SEGMENT_SIZE;
    // Use canned protection values - in our design
    // all page protection occurs in page entries
    vsid = (segDesc.VSID << 12) | 0x400;	// Ks=0 Kp=1 NLC=0
    if (logPageSize != LOG_PAGE_SIZE) {
	// Or-in L bit and LS value
	vsid |= (0x100 |
		 (exceptionLocal.pageTable.getLS(logPageSize) << 4));
    }

    //             V=1         ndx
    esid = vaddr | 0x8000000;
    uval slbTarget = 0;
    if (exceptionLocal.currentSegmentTable == this) {
	// Update the SLB only if this segment table is current
	slbTarget = nextSlbNdx();
	asm volatile ("slbie %0" : : "r" (vaddr));
	asm volatile ("slbmte %0,%1"
		      : : "r" (vsid), "r" (esid | slbTarget));
    }
    /*
     * put new entry into the cache if its not already there.
     * it is possible that we choose a slot which has just
     * been filled by lolita.  This does not harm.
     * We lock the segment table cache so lolita will not
     * mess with it.
     */
    
    exceptionLocal.pageTable.setSegLoadType(
	InvertedPageTable::SOFTWARE_LOCKED);
    // if its already in the cache skip adding it again.
    if ( uval(-1) == findSegmentTableEntry(vaddr)) {
	uval i;
	i = nextCacheNdx();
#if 0
	err_printf("segfault %lx %lx %lx",
		   uval(this)&0xffffffff, vaddr>>LOG_SEGMENT_SIZE, i);
	if(SLBCache[i].esid.V) {
	    err_printf(" evicts %lx", uval(SLBCache[i].esid.ESID));
	}
	err_printf(" slb  %lx\n", slbTarget);
#endif
	TraceOSMemMapSegment(
	    vaddr>>LOG_SEGMENT_SIZE, i, slbTarget,
	    SLBCache[i].esid.V?uval(SLBCache[i].esid.ESID):0);  
	SLBCache[i].vsid = vsid;
	SLBCache[i].esid = esid | i;
    }
    exceptionLocal.pageTable.setSegLoadType(
	InvertedPageTable::SOFTWARE);
    enableHardwareInterrupts();
}

/*
 * unmap segment
 */
void
SegmentTable_SLB::unmapSegment(uval vaddr)
{
    disableHardwareInterrupts();
    exceptionLocal.pageTable.setSegLoadType(
	InvertedPageTable::SOFTWARE_LOCKED);
    /* Delete entry from cache.
     */
    uval index = findSegmentTableEntry (vaddr);
    tassertMsg(index >= numBolted, "unmapping bolted entry\n");
    if ((index != uval(-1))) {
	cacheMax--;
	cacheNext=MIN(cacheNext, cacheMax);
	if(index != cacheMax) {
	    //overwrite unmapped entry with last entry
	    SLBCache[index] = SLBCache[cacheMax];
	    SLBCache[index].esid.index = index;
	}
	//invalidate last cached entry
	//KLUDGE ALERT - we really want to set the V bit off, and we
	//must leave the index correct to avoid accidently invalidating
	//the wrong entry on a cache reload.  But I can't resist just
	//storing the index in the word, since I know that the index
	//is right aligned.
	SLBCache[cacheMax].esid = cacheMax;
    }
    // Flush slb entry whether or not mapping was found in seg table.  The
    // mapping-fault handler and findEmptySegmentTableEntry evict seg table
    // entries without flushing the slb.
    // clear all but esid so class bit is always 0
    vaddr &= -SEGMENT_SIZE;
    asm volatile ("slbie %0" : : "r" (vaddr));
    exceptionLocal.pageTable.setSegLoadType(
	InvertedPageTable::SOFTWARE);
    enableHardwareInterrupts();
}

/*
 * Get the VSID corresponding to an effective address.
 */
SysStatus 
SegmentTable_SLB::getVSID(uval vaddr, uval &retVSID)
{
    uval index = findSegmentTableEntry(vaddr);
    if (index != uval(-1)) {  // entry for vaddr was found
	retVSID = SLBCache[index].vsid.VSID;
	return 0;
    }
    /*
     * should never fail - used only during init
     */
    passertMsg(0, "getVSID failed %lx\n", vaddr);
    return _SERROR (1443, 0, ENOENT);	// no matching entry found
}

SysStatus
SegmentTable_SLB::destroy()
{
    if (exceptionLocal.realModeMemMgr != NULL) {
	exceptionLocal.realModeMemMgr->dealloc(uval(this), sizeof(*this));
    } else {
	delete this;
    }
    return 0;
}
