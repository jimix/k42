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
 * $Id: SegmentTable.H,v 1.57 2005/01/19 23:50:01 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * machine dependent object to manage a segment.  This is NOT
 * a clustered object - it is managed by the process HAT
 * Locking is also done by the caller
 * N.B. This object has NO virtual function
 * **************************************************************************/

#include <misc/hardware.H>
#include <sys/memoryMap.H>
#include "exception/ExceptionLocal.H"
#include "mem/PageCopy.H"
#include "mem/PageAllocatorKernPinned.H"


class HATKernel;

class SegmentTable{
public:
    static const SegDesc InvalidSegDesc; // an "invalid" segment table entry;
					 // use this to clear one out

protected:
    /*
     * Given a virtual addr ("effective addr", in PowerPC architecture
     * terminology), return index number of element in segDescTable that
     * maps the address. If no entry in the segment table maps the virtual
     * addr, return -1.
     */
    uval findSegmentTableEntry(uval vaddr){return 0;}

    /*
     * Find an empty slot in the Segment Table.
     * If no such exists, evict an entry at random.
     */
    uval findEmptySegmentTableEntry(uval vaddr){return 0;}

    /*
     * Store the two halves of a segment table entry, in the proper order,
     * with the necessary memory synchronization
     */
    void storeSegmentTableEntry(sval index, SegDesc segDesc){};

    virtual void mapVmapsRSegment(
	uval vaddr, uval bolted, uval vsid,
	uval logPageSize, InvertedPageTable *pageTable)=0;
public:
    // Only relevant for hardware reload machines
    // Returns null otherwise
    virtual uval64 getASR()=0;

    // Get the VSID corresponding to an effective address.
    // This involves searching the STEG for the proper STE.
    // Only used in init - only works if entry happens
    // to be in the cache for the SLB case.
    virtual SysStatus getVSID(uval vaddr, uval &retVSID)=0;;

    /*
     * Map and unmap segments
     */
    virtual void mapSegment(SegDesc segDesc, uval vaddr, uval logPageSize)=0;
    virtual void unmapSegment(uval vaddr)=0;
    uval checkKernelSegmentFault(uval /*vaddr*/) {
	// can't happen on powerpc.
	return 0;
    }
    static SysStatus Create(SegmentTable*& segmentTable);
    static SysStatus CreateKernel(
	SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
	ExceptionLocal *elocal);
    static SysStatus CreateBoot(
	SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
	InvertedPageTable *pageTable, uval mapStart, uval mapEnd);
    void enterBoltedPage(MemoryMgrPrimitiveKern *memory,
			 InvertedPageTable *pageTable,
			 uval logPgSz, uval vaddr, uval vMapsRAddr);
    void setupBoltedPages(
	MemoryMgrPrimitiveKern *memory, ExceptionLocal *elocal);
    uval setupBoltedSegments(ExceptionLocal *elocal);
    void setupBootPages(
	MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
	uval mapStart, uval mapEnd);
    uval setupBootSegments(InvertedPageTable *pageTable);
    virtual void changePP()=0;
    virtual SysStatus destroy()=0;
    /*
     * switchToAddressSpace() does just that:  loads the address of a segment
     * table into the Address Space Register and purges the SLB.
     *
     * N.B. this method is used by ppc.C from exception level.
     */
    virtual void switchToAddressSpace()=0;
    DEFINE_NOOP_NEW(SegmentTable);
    virtual ~SegmentTable() { }
};

class SegmentTable_Hardware: public SegmentTable {
    friend void genConstants(void);
    /*
     * The hardware segment table is a single page (4096 bytes) on a page
     * boundary; it's first in the structure to ensure that it's aligned.
     * It's an array of SegDesc, with SegDesc::NumberOfSegments entries.
     */

    SegDesc *segDescTable;

    /*
     * process migration may cause destruction an a different processor.
     */
    DEFINE_PINNEDGLOBAL_NEW(SegmentTable_Hardware);
    DEFINE_PRIMITIVE_NEW(SegmentTable_Hardware);
    

    /*
     * Given a virtual addr ("effective addr", in PowerPC architecture
     * terminology), return index number of element in segDescTable that
     * maps the address. If no entry in the segment table maps the virtual
     * addr, return -1.
     */
    uval findSegmentTableEntry(uval vaddr);

    /*
     * Find an empty slot in the Segment Table.
     * If no such exists, evict an entry at random.
     */
    uval findEmptySegmentTableEntry(uval vaddr);

    /*
     * Store the two halves of a segment table entry, in the proper order,
     * with the necessary memory synchronization
     */
    void storeSegmentTableEntry(sval index, SegDesc segDesc);

    virtual void mapVmapsRSegment(
	uval vaddr, uval bolted, uval vsid,
	uval logPageSize, InvertedPageTable *pageTable);

    void init();
    void initKernel(MemoryMgrPrimitiveKern *memory, ExceptionLocal* elocal);
    void initBoot(MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
		  uval mapStart, uval mapEnd);
    virtual void changePP() {init();}

    // Get the VSID corresponding to an effective address.
    // This involves searching the STEG for the proper STE.
    SysStatus getVSID(uval vaddr, uval &retVSID);

public:
    virtual uval64 getASR() {
	// ASR value is the table's physaddr with the low-order (valid) bit set
	return PageAllocatorKernPinned::virtToReal(uval(segDescTable)) | 1;
    }

    /*
     * Map and unmap segments
     */
    void mapSegment(SegDesc segDesc, uval vaddr, uval logPageSize);
    void unmapSegment(uval vaddr);

    uval checkKernelSegmentFault(uval /*vaddr*/) {
	// can't happen on powerpc.
	return 0;
    }

    static SysStatus Create(SegmentTable*& segmentTable);
    static SysStatus CreateKernel(
	SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
	ExceptionLocal* elocal);
    static SysStatus CreateBoot(
	SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
	InvertedPageTable *pageTable, uval mapStart, uval mapEnd);

    SysStatus destroy();

    /*
     * switchToAddressSpace() does just that:  loads the address of a segment
     * table into the Address Space Register and purges the SLB.
     *
     * N.B. this method is used by ppc.C from exception level.
     */
    void switchToAddressSpace()
    {
	uval newASR;

	newASR = getASR();
	// arch book requires context-synchronizing instrs before and
	// after mtasr, slbia
	asm volatile ("isync");
	asm volatile ("mtasr %0" : : "r" (newASR));
	asm volatile ("slbia");
	asm volatile ("isync");
    }
};

class SegmentTable_SLB: public SegmentTable {
    friend void genConstants(void);
public:
    /*
     * Bolted segments:
     * 0 - code addressability - large page if possible
     * 1 - segment table segment if not code segment
     * +1 - exception local - processor specific
     * +1 - commonPSR - ppc page plus
     */
    /*
     * SDET experiments lead to 32 entry cache.  AIX also uses a 32 entry
     * cache, although they manage there's more explicitly - bolting 16
     * entries
     */
    enum {NumCached=32};
protected:
    /*
     * We maintain a cache of recently loaded slb entries to
     * reload agressively on address space switch.  This
     * includes the bolted entries.
     */

    uval slbNext;			// first unused slot
    uval cacheNext;			// next cache slot to use
    uval cacheMax;			// number cached
    /*
     * We bolt two entries an initialization.  There are the segment
     * containing kernel code (we assume one is sufficient)
     * and the segment containing ExceptionLocal.  We must bolt these
     * entries because we use code which does an rfid to resume a process
     * from virtual mode.  That code depends, in a small window, on
     * having NO PAGE FAULTS of any kind.
     *
     * In addition, if the segment containing the SegmentTable instance
     * is not the same as the code segment, we add it to the bolted set
     * as an optimization to avoid taking a segment fault every time
     * we switch to that address space.
     *
     * SBLCache invarients:
     * Only valid entries have the valid bit on - even those beyond the
     * current cacheMax value.
     * Each entry has its own index value in the esid - thus the SLB
     * can be reloaded by loading the entries as is.
     * Races with lolita updates are avoided by doing these updates
     * in assembler in code that only touches bolted segments.  In particular,
     * it does NOT touch the stack.
     */
    uval numBolted;			// number of entries to preserve
    
    struct {SLB_VSID vsid; SLB_ESID esid; } SLBCache[NumCached];

    /*
     * migration may cause destroy on a different processor
     */
    DEFINE_PINNEDGLOBAL_NEW(SegmentTable_SLB);
    DEFINE_PRIMITIVE_NEW(SegmentTable_SLB);
    
    /*
     * Given a virtual addr ("effective addr", in PowerPC architecture
     * terminology), return index number of element in segDescTable that
     * maps the address. If no entry in the segment table maps the virtual
     * addr, return -1.
     */
    uval findSegmentTableEntry(uval vaddr);

    /*
     * Find an empty slot in the Segment Table.
     * If no such exists, evict an entry at random.
     */
    uval findEmptySegmentTableEntry(uval vaddr);

    /*
     * Store the two halves of a segment table entry, in the proper order,
     * with the necessary memory synchronization
     */
    void storeSegmentTableEntry(sval index, SegDesc segDesc);

    virtual void mapVmapsRSegment(
	uval vaddr, uval bolted, uval vsid,
	uval logPageSize, InvertedPageTable *pageTable);

    void init();
    void initKernel(MemoryMgrPrimitiveKern *memory, ExceptionLocal* elocal);
    void initBoot(MemoryMgrPrimitiveKern *memory, InvertedPageTable *pageTable,
		  uval mapStart, uval mapEnd);
    virtual void changePP() {init();}

    // Get the VSID corresponding to an effective address.
    // This involves searching the STEG for the proper STE.
    SysStatus getVSID(uval vaddr, uval &retVSID);

    uval nextSlbNdx() {
	uval i = slbNext++;
	tassertMsg(i < SLB_VSID::NumberOfSLBs, "slb index fumble %lx\n" , i);
	if (slbNext == SLB_VSID::NumberOfSLBs) slbNext = cacheMax;
	return i;
    }
    
    uval nextCacheNdx() {
	tassertMsg(cacheNext <= cacheMax, "cache index fumble %lx %lx\n",
		   cacheNext, cacheMax);
	uval i = cacheNext++;
	cacheMax = MAX(cacheMax, cacheNext);
	if (cacheNext == NumCached) cacheNext = numBolted;
	return i;
    }
    
public:
    virtual uval64 getASR() {
	// return the real address of the SegmentTable object for convenience
	return PageAllocatorKernPinned::virtToReal(uval(this));
    }

    /*
     * Map and unmap segments
     */
    void mapSegment(SegDesc segDesc, uval vaddr, uval logPageSize);
    void unmapSegment(uval vaddr);

    uval checkKernelSegmentFault(uval /*vaddr*/) {
	// can't happen on powerpc.
	return 0;
    }

    static SysStatus Create(SegmentTable*& segmentTable);
    static SysStatus CreateKernel(
	SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
	ExceptionLocal* elocal);
    static SysStatus CreateBoot(
	SegmentTable*& segmentTable, MemoryMgrPrimitiveKern *memory,
	InvertedPageTable *pageTable, uval mapStart, uval mapEnd);

    SysStatus destroy();

    /*
     * Load some SLB entries from the cache after invalidating.
     * We must load the bolted entries.
     * N.B. that entry zero is bolted and survives slbia.  It
     * is the code segment.
     *
     * We must take care to make sure we have addressability to the
     * slb cache before touching it after slbia.
     *
     * N.B. this method is used by ppc.C from exception level.
     */
    void switchToAddressSpace()
    {
	/*
	 * this code assumes that the cache contains entries with
	 * useable index values in the esid words.  In fact, we
	 * use the cache index as the slb index since we are reloading
	 * from scratch.
	 */
	tassertMsg(0 ==  hardwareInterruptsEnabled(), "called enabled\n");

	ExceptionLocal_LoadSLB(uval(this));	// load the cache

	tassertMsg(cacheNext <= cacheMax,
		   "cacheNext %lx cacheMax %lx\n", cacheNext, cacheMax);

    }
};
