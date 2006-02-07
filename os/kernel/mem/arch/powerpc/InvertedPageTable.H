#ifndef __INVERTED_PAGE_TABLE_H_
#define __INVERTED_PAGE_TABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InvertedPageTable.H,v 1.38 2004/11/16 23:14:28 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Class that defines the per-processor inverted
 * page table of PwrPC.
 * **************************************************************************/

#include "mem/Access.H"
#include <sync/SLock.H>
#include <mmu.H>
#define PTE_STATS

struct PTEG;

class InvertedPageTable {
    // very light weight - no tracing, not MCS - just spin
    // can be used disabled
    typedef BitSLock<LockBits> FastSpinLock;
    typedef SLock SpinLock;
    friend  void genConstants(void);
    // this is not an MCS lock, because SLock can't be used disabled.
    // is this a problem?
    static  SpinLock *TLBIELock;	// hardware requirement
    PTEG   *hashTable;			// V~R address of page table
    uval    pte;			// phys address of page table
    SpinLock  *hashTableLock;		// lock when processors share PT
    SpinLock  slock;			// actual lock
    uval   *lolitaLock;			// lock lolita uses - real spin lock
    uval   spinLock;			// actual lock unless shared
    struct largePageInfo {
	uval  numSizes;			// number of large page sizes
	uval  logSizes[8];
    } largePage;
    uval    logNumPTEs;			// log base 2 of page table size
    uval    hashBits;			// log of hash size
    uval    mask;			// mask with ones over hash
    volatile uval segLoad;         	// hardware=0  or software=1
					// or software_locked=2
    uval    num_dsi;			// stats: number of dsi misses
    uval    num_dsgi;			// stats: number of dsegi misses
    uval    num_isi;			// stats: number of isi misses
    uval    num_isgi;			// stats: number of isegi misses
    uval    num_segmiss;		// stats: number of segment misses
    uval    num_map_fault;		// stats: number of mapping faults
    uval    num_map_evict;		// stats: number of map-fault evictions

#ifdef PTE_STATS
    uval    num_seg_evict;
    uval    num_evict;
    uval    num_invalidate;
    uval    num_invalidateNotFound;
    uval    num_maps;
    uval    num_skipped;		// in evict logic, number skipped
    uval    by8[9], by16[17];
#endif
    
    void    printStatsInternal();
    void    initStats();

public:
    uval largePageFixup(uval logPageSize) {
	uval i, rpn;
	rpn = 0;
	for(i=0; i<largePage.numSizes; i++) {
	    if (logPageSize == largePage.logSizes[i])
		return rpn;
	    //rpn is of form 01...1 where rpn i (zero based) has i 1's
	    rpn = (rpn<<1) | 1;
	}
	passertMsg(0, "Can't map page size 2**%ld\n", logPageSize);
	return 0;
    }

    uval getLS(uval logPageSize) {
	// LS is the segment table representation of the pageSize
	uval i;
	for(i=0; i<largePage.numSizes; i++) {
	    if (logPageSize == largePage.logSizes[i])
		return i;
	}
	passertMsg(0, "Can't map page size 2**%ld\n", logPageSize);
	return 0;
    }
	
    uval getLogPageSize(uval pageSize) {
	uval i;
	if(pageSize == PAGE_SIZE) return LOG_PAGE_SIZE;
	for(i=0; i<largePage.numSizes; i++) {
	    if(pageSize == (1ull<<largePage.logSizes[i]))
		return largePage.logSizes[i];
	}
	return 0;
    }

    /*
     * Returns log of 1st large page size or, if no large, base page size.
     */
    uval getLogLargestPageSize() {
	uval i;
	uval logLargest=LOG_PAGE_SIZE;
	for(i=0; i<largePage.numSizes; i++) {
	    logLargest = MAX(logLargest, largePage.logSizes[i]);
	}
	// for now, lolita assumes that its large page size is size 0
	tassertMsg(largePage.numSizes == 0 ||
		   logLargest == largePage.logSizes[0], "Page size fumble\n");
	return logLargest;
    }

    uval getPageTableVaddr() { return uval(hashTable); }
    
    uval allocVSID();

    uval getSDR1() {
 	return pte | (logNumPTEs-BASE_LOG_PTE);
    }

    uval getLogNumPTEs() { return logNumPTEs; }

    uval getHTIndex(uval vaddr, uval vsid, uval logPageSize);

    void printValidPage(uval vaddr, uval vsid);

    void printValidPages();

    void countSegEvict() { num_seg_evict++; }

    enum { HARDWARE=0, SOFTWARE=1, SOFTWARE_LOCKED=2};
    //FIXME if we have other schemes (i.e. paravirtualization)
    //we must distinguish those here and in init
    uval segLoadType() {return segLoad;}
    void setSegLoadType(uval type) {segLoad = type;}

    void printStats();
    void clear();

    /*
     * invalidate one tlb entry
     */
    static void Tlbie(uval vaddr, uval logPageSize);

    /*
     * invalidate a list.  we use 32 bit entries
     * in the list to save space in caller
     * the rest of vpn comes from the vsid.
     */
    static void TlbieList(uval32* pnList, uval count,
			  uval logPageSize, uval vsid);

    /*
     * fix up page table entry but no tlb sync
     * returns 1 if not found in page table
     */
    uval invalidatePage(uval vaddr, uval vsid, uval logPageSize);

    void enterPage(uval vaddr, uval paddr, uval logPageSize, uval vsid,
		    AccessMode::mode perms);

    void enterBoltedPage(uval vaddr, uval paddr, uval logPageSize, uval vsid,
			 AccessMode::mode perms,
			 MemoryMgrPrimitiveKern *memory);

    void init(uval lgNumPTEs, uval vaddr, uval paddr);

    static void ClassInit(MemoryMgrPrimitive *memory) {
	uval lockptr;
	memory->alloc(lockptr, sizeof (*TLBIELock), 8);
	TLBIELock = (SpinLock *)lockptr;
	TLBIELock->init();
    }

    uval isShared() { return uval(hashTableLock); }

    InvertedPageTable* prepareToShare();

    void initShared(InvertedPageTable* newIPT);
};

#endif /* #ifndef __INVERTED_PAGE_TABLE_H_ */
