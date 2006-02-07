/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InvertedPageTable.C,v 1.97 2005/04/29 01:00:17 mergen Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "exception/ExceptionLocal.H"
#include "InvertedPageTable.H"
#include <mem/SegmentTable.H>
#include <trace/traceMem.h>
#include <bilge/arch/powerpc/BootInfo.H>
#include <misc/baseStdio.H>
#include <sys/hcall.h>
#include "bilge/MIP.H"

#ifdef PTE_STATS
uval num_mask = (uval) -1;
#endif

static sval lastVsid = -1;		// hand 'em out starting with 0

// this is not an MCS lock, because SLock can't be used disabled.
// is this a problem?
InvertedPageTable::SpinLock *InvertedPageTable::TLBIELock;


/*
 * The powerpc pteg hash is and xor of the low order bits of the vsid
 * and the low order bits of the page index in the segment.  Since
 * address spaces tend to reuse the same ranges of pages, if the vsid
 * also varies most quickly in its low order bits, the xor just
 * permutes the values, and many segments wind up using the same pte
 * groups.  To avoid this, we construct vsids in which the low order
 * bits which matter in the hash are bit wise reversed, so the numbers
 * vary fastest in the high order bits.
 *
 * N.B. kernel vsids are made out of whole cloth in the mapping fault
 * handler.  They use values with a high order 0001 so they don't
 * interfere with these values.
 * N.B. this allocator is called BEFORE init() from initKernelMappings.
 */

uval
InvertedPageTable::allocVSID()
{
    uval newVsid, rNewVsid;
    /*
     * we are trying to compute the log of the number of PTEG's
     * This is log of the  number of pte's divided by the number of pte's
     * in a pteg.  The number of pte's in a pteg is the size of a pteg
     * divided by the size of a pte.  The log of this is
     * LOG_PTEG_SIZE-LOG_PTE_SIZE.
     * The log of number of pte's divided by number of pte's in pteg is
     * logNumPTSs - (LOG_PTED_SIZE-LOG_PTE_SIZE) which is what we
     * compute expanded below.
     */
    newVsid=(uval)FetchAndAddSignedVolatile(&lastVsid,1) + 1;
    // this code could be in assembler
    // first bit reverse the whole newVsid value
    rNewVsid = (newVsid>>32) | (newVsid<<32);
    rNewVsid = ((rNewVsid>>16)&0xffff0000ffff)|
	((rNewVsid<<16)&0xffff0000ffff0000);
    rNewVsid = ((rNewVsid>>8)&0xff00ff00ff00ff)|
	((rNewVsid<<8)&0xff00ff00ff00ff00);
    rNewVsid = ((rNewVsid>>4)&0xf0f0f0f0f0f0f0f)|
	((rNewVsid<<4)&0xf0f0f0f0f0f0f0f0);
    rNewVsid = ((rNewVsid>>2)&0x3333333333333333)|
	((rNewVsid<<2)&0xcccccccccccccccc);
    rNewVsid = ((rNewVsid>>1)&0x5555555555555555)|
	((rNewVsid<<1)&0xaaaaaaaaaaaaaaaa);
    // now combine so only low order vsidLogNumPTEs-BASE_LOG_PTE are reversed
    newVsid = (newVsid&~mask)|
	((rNewVsid>>(64-hashBits))&mask);
    passertMsg(newVsid < 0x0001000000000,
	       "new vsid %lx uses more than 36 bits\n", newVsid);
    return newVsid;
}

uval
InvertedPageTable::getHTIndex(uval vaddr, uval vsid, uval logPageSize)
{
    uval hash_index;
    uval htabmask;
    uval hash_offset;
    uval ea_mask;

    ea_mask = (1<<(LOG_SEGMENT_SIZE-logPageSize))-1;
    hash_offset = (VSID_HASH_MASK & vsid) ^
	          ((vaddr >> logPageSize) & ea_mask);

    htabmask = (1ul<<(logNumPTEs-BASE_LOG_PTE)) - 1;
    htabmask = (htabmask << HTABMASK_SHIFT) | 0x7FF;
    hash_index = hash_offset & htabmask;

    return hash_index;
}

void
InvertedPageTable::printValidPage(uval /* addr */, uval /* vsid */)
{
#if 0 // FIXME to get inverted mapping
    uval hash_index = getHTIndex(addr, vsid);

    for (uval pteg_index=0; pteg_index<NUM_PTES_IN_PTEG; pteg_index++) {

	PTE* pteToSet = &(hashTable[hash_index].entry[pteg_index]);

	if (PTE_PTR_V_GET(pteToSet)) {
	    uval vsid  = PTE_PTR_VSID_GET(pteToSet);
	    uval vaddr = ((vsid & VSID_HASH_MASK) ^ hash_index);
	    uval htabmask = (1<<(13-BASE_LOG_PTE)) - 1;
	    htabmask = (htabmask << HTABMASK_SHIFT) | 0x3FF;

	    vaddr &= htabmask;
	    vaddr |= (PTE_PTR_API_GET(pteToSet) << 10);

	    uval paddr = PTE_PTR_RPN_GET(pteToSet) << 12;
	    vaddr <<= 12;

	    // we only have certain vsid right now
	    uval print_val = 0;
	    switch (vsid-KERN_VSID_BASE) {
	    case 0xC: case 0xD: case 0xE: case 0x4:
		print_val = 1;
		vaddr |= (vsid-KERN_VSID_BASE) << 28;
		break;
	    case 0x0:
		vaddr |= (vsid-KERN_VSID_BASE) << 28;
		break;
	    }
	    if (print_val)
		cprintf("%lx[%ld]: v=%lx p=%lx vsid=%lx\n",
			&(hashTable[hash_index]),pteg_index,
			vaddr,paddr,vsid);
	}
    }
#endif
}

void
InvertedPageTable::printValidPages()
{
//FIXME NEEDS to be redone for 64 bits

#if 0 // FIXME to get inverted mapping
    int print_val = 0;
    cprintf("-----printing valid entries in Page Table\n");

    // FIXME: fix hard coded constents in this
    for (uval hash_index=0; hash_index< (1<<10); hash_index++) {
	for (uval pteg_index=0; pteg_index<NUM_PTES_IN_PTEG; pteg_index++) {
	    PTE* pteToSet = &(hashTable[hash_index].entry[pteg_index]);
	    if (PTE_PTR_V_GET(pteToSet)) {
//		uval vaddr = PTE_PTR_API_GET(pteToSet) << 12;
		uval vsid  = PTE_PTR_VSID_GET(pteToSet);
		uval vaddr = ((vsid & VSID_HASH_MASK) ^ hash_index);

		uval htabmask = (1<<(13-BASE_LOG_PTE)) - 1;
		htabmask = (htabmask << HTABMASK_SHIFT) | 0x3FF;

		vaddr &= htabmask;
		vaddr |= (PTE_PTR_API_GET(pteToSet) << 10);

		uval paddr = PTE_PTR_RPN_GET(pteToSet) << 12;
		vaddr <<= 12;

		// we only have certain vsid right now
		print_val = 0;
		if (vsid < lastVsid) print_val = 1;
#if 0
		switch (vsid-KERN_VSID_BASE) {
		case 0xC: case 0xD: case 0xE: case 0x4:
		    print_val = 1;
		    vaddr |= (vsid-KERN_VSID_BASE) << 28;
		    break;
		case 0x0:
		    vaddr |= (vsid-KERN_VSID_BASE) << 28;
		    break;
		}
#endif
		if (print_val)
		    cprintf("%lx[%ld]: v=%lx p=%lx vsid=%lx\n",
			    &(hashTable[hash_index]),pteg_index,
			    vaddr,paddr,vsid);
	    }
	}
    }
    cprintf("-----done printing valid entries in Page Table\n");
#endif
}

/*
 * invalidate one tlb entry
 */
void
InvertedPageTable::Tlbie(uval vaddr, uval logPageSize)
{
    if (_BootInfo->onHV) return;

    if (logPageSize != LOG_PAGE_SIZE) {
	vaddr |= exceptionLocal.pageTable.largePageFixup(logPageSize)
	    << LOG_PAGE_SIZE;
    }
    disableHardwareInterrupts();
    TLBIELock->acquire();
    // architecture now requires top 16 bits of vaddr to be clear
    if (logPageSize == LOG_PAGE_SIZE) {
	asm("rldicl %0,%0,0,16; tlbie %0; eieio; tlbsync" : : "r" (vaddr));
    } else {
	asm("rldicl %0,%0,0,16; tlbie %0,1; eieio; tlbsync" : : "r" (vaddr));
    }
    // lock.release does the required sync
    TLBIELock->release();
    enableHardwareInterrupts();
}

uval marctlbieflag=1;
/*
 * invalidate a list.  we use 32 bit entries
 * in the list to save space in caller
 * tlbie only cares about the logical page number in the segment
 */
void
InvertedPageTable::TlbieList(
    uval32* pnList, uval count, uval logPageSize, uval vsid)
{
    if (_BootInfo->onHV) return;
    if (marctlbieflag) {
	uval vpn;
	vsid = vsid << LOG_SEGMENT_SIZE;
	if (logPageSize != LOG_PAGE_SIZE) {
	    vsid |= exceptionLocal.pageTable.largePageFixup(logPageSize)
		<< LOG_PAGE_SIZE;
	}
	disableHardwareInterrupts();
	TLBIELock->acquire();
	while (count--) {
	    vpn = (*pnList & SEGMENT_MASK) | vsid;
	    // architecture now requires top 16 bits of vaddr to be clear
	    if (logPageSize == LOG_PAGE_SIZE) {
		asm("rldicl %0,%0,0,16; tlbie %0" : : "r" (vpn));
	    } else {
		asm("rldicl %0,%0,0,16; tlbie %0,1" : : "r" (vpn));
	    }
	    pnList++;
	}
	asm("eieio; tlbsync");
	// lock.release does the required sync
	TLBIELock->release();
	enableHardwareInterrupts();
    } else {
	asm(".long 0x7c0002e4");		// tlbia
    }
}



/*
 * this is done without disabling or locking
 * since the worst thing that can happen is
 * that we invalidate our entry after
 * its been evicted, in which case we invalidate some
 * random valid entry.
 * We do NOT invalidate the tlb - we let the caller
 * do that because we want to batch tlb invalidates
 * (re-evaluate if ref/change bits are used)
 * We must not accidently invalidate a bolted entry,
 * but bolted entries are made early when races can't happen.
 */
uval
InvertedPageTable::invalidatePage(uval vaddr, uval vsid, uval logPageSize)
{
    PTE *pteToSet = 0;
    uval pteg_index;
    uval hash_index = getHTIndex(vaddr, vsid, logPageSize);
    uval curVsidWord, ret, hv_pte[8], ptex = 0;

    /* now we have a index into our hash table, first generate the vsid_word
     * and then start searching for it in the primary PTE table
     */

    // api pageindex (12bits) and 10 bits are dropped for abreviation
    uval api  = VADDR_TO_API(vaddr);

    uval vsidWord = (PTE_V_MASK |                        // valid bit on
		     (vsid << PTE_VSID_SHIFT) |          // vsid
		     (api << PTE_API_SHIFT));            // api
#ifdef PTE_STATS
    num_invalidate++;
#endif

    for (pteg_index=0; pteg_index<NUM_PTES_IN_PTEG; pteg_index++) {

	if (_BootInfo->onHV) {
	    ptex = (hash_index << LOG_NUM_PTES_IN_PTEG) | pteg_index;
	    ret = hcall_read(hv_pte, 0, ptex);
	    curVsidWord = hv_pte[0];
	} else {
	    pteToSet = &(hashTable[hash_index].entry[pteg_index]);
	    curVsidWord = pteToSet->vsidWord;
	}

	// compare to vsid/api/valid value - ignore H and Software bits
	curVsidWord &= PTE_VSID_MASK|PTE_API_MASK|PTE_V_MASK;
	if (curVsidWord == vsidWord) {

	    if (_BootInfo->onHV) {
		ret = hcall_remove(hv_pte, 0, ptex, 0);
//		err_printf("H_REMOVE %lx %llx %llx %lx\n",
//			   ret, hv_pte[0], hv_pte[1], ptex);
	    } else {
		pteToSet->vsidWord &= ~PTE_V_MASK;        // invalidate
	    }

	    TraceOSMemInvPage(vaddr, hash_index, pteg_index, api, vsidWord);
	    return 0;
	}
    }
    //FIXME - we need to add the second hashing function here
    //FIXME hash_funct_ident = 1 if we do this

//    err_printf("invalidate did not find vaddr=0x%lx %lx\n",vaddr, vsid);
#ifdef PTE_STATS
    num_invalidateNotFound++;
#endif
    return 1;
}


/*
 * disables around actual search/update
 */
void
InvertedPageTable::enterPage(uval vaddr, uval paddr, uval logPageSize,
				uval vsid, AccessMode::mode perms)
{
    struct PTE temp_pte;
    uval pteg_index;
    uval api;
    /* FIXME should be of unsigned 32 bit type */
    uval rpn;
    struct PTEG *ptegEntry;

    struct PTE *pteToSet = 0;
    uval valid_bit=0;
    uval hash_funct_ident=0;
    uval64 hash_index;
    uval64 htab, htaborg, ptegaddr;
    uval evict;
    uval ret, retval[8];

    hash_funct_ident=0;

    tassert(!(vsid & PTE_VSID_LOCK),
	    err_printf("vsid values must not use high order bit %lx\n", vsid));

    // If we are out side of standard physical memory it is probably an error
    passert(AccessMode::isCacheInhibit(perms) ||
	    (paddr < _BootInfo->physEnd) ||
            (paddr < _BootInfo->physEnd+MIP::ExtendMemory(_BootInfo->physEnd)),
	    err_printf("tried to map %lx to %lx\n", vaddr, paddr));

    TraceOSMemEnterPage(vaddr, logPageSize, paddr);
    hash_index = getHTIndex(vaddr, vsid, logPageSize);

    // disable first so we don't get interupted while holding the lock
    disableHardwareInterrupts();	// protect against interrupts
    if (hashTableLock) hashTableLock->acquire();	// protect against other cpus
#ifdef PTE_STATS
    num_maps++;
#endif

  if (!_BootInfo->onHV) {

    /* now we have a index into our hash table, find the first page that
     * is invalid and use it.  If we find more than eight entries i.e.,
     * the first hash function maps to a full pteg then use the second
     * hash function and try to find an invalid page.  If this also fails
     * return a fatal error.  In reality we should never have to search
     * beyond the first pte.
     */

    htab = (uval64)hashTable;

    htaborg = htab;

    ptegaddr = htaborg|(hash_index<<LOG_PTEG_SIZE);

    ptegEntry = (struct PTEG *)ptegaddr;

    evict = 0;
    for (pteg_index=0; pteg_index<NUM_PTES_IN_PTEG; pteg_index++) {
	pteToSet = &(ptegEntry->entry[pteg_index]);
	valid_bit = PTE_PTR_V_GET(pteToSet);
	if (valid_bit == 0) {
	    break;
	}
	if (PTE_PTR_IS_LAST(pteToSet)) {
	    evict = pteg_index;
	}
    }

    if (valid_bit) {
#ifdef PTE_STATS
	num_evict++;
#endif

	/* We did not find a free entry under our hash.  We must evict an
	 * entry from the PageTable.  We look for an entry whose reference
	 * bit is not set, clearing the reference bits of the entries we
	 * examine.  We start with the last entry made, which will
	 * probably have its ref bit on, but we need to clear it now.
	 */
	while (1) {
	    pteToSet = &(ptegEntry->entry[evict]);
	    pteg_index = evict;
	    if (!PTE_PTR_IS_REFERENCED(pteToSet) &&
			!PTE_PTR_IS_BOLTED(pteToSet)) break;
#ifdef PTE_STATS
	    num_skipped++;
#endif
	    /*
	     * Turn off ref bit and last bit
	     * mapping fault handler could change this entry out
	     * from under us, but if it does the vsidWord will change
	     * since the mapping handler never remaps an existing
	     * virtual address to a new real address.
	     */
	    do {
		temp_pte.vsidWord =
		    ((volatile struct PTE*)pteToSet)->vsidWord;
		temp_pte.rpnWord =
		    ((volatile struct PTE*)pteToSet)->rpnWord;
	    } while (temp_pte.vsidWord !=
			((volatile struct PTE*)pteToSet)->vsidWord);
	    PTE_R_CLEAR(temp_pte);
	    PTE_LAST_CLEAR(temp_pte);
	    PTE_SET(temp_pte, pteToSet);

	    /* no action needs to be taken by HV  */
	    /* since the page table is kept in the HV the reference
	     * information is not yet used in K42 the last bit is set
	     * and replacement is solely driven by this bit */

	    evict = (evict+1) & (NUM_PTES_IN_PTEG - 1);
	    /*
	     * Loop will terminate because the second time around we'll find
	     * an entry with a cleared reference bit.
	     */
	}

#if 0
	err_printf(
	    "Evicting PTE[%lx][%ld] to map %lx\n",uval(hash_index),evict,vaddr);
#endif

    }
  }

    temp_pte.clear();
    PTE_V_SET(temp_pte, 1);
    PTE_VSID_SET(temp_pte, vsid);
    // if valid_bit is one, we are evicting, and we set a new last
    PTE_LAST_SET(temp_pte, valid_bit);
    PTE_H_SET(temp_pte, hash_funct_ident);

    api = VADDR_TO_API(vaddr);
    PTE_API_SET(temp_pte, api);
    rpn = (paddr) >> RPN_SHIFT;
    if (logPageSize != LOG_PAGE_SIZE) {
	PTE_L_SET(temp_pte, 1);
	rpn |= largePageFixup(logPageSize);
    }
    PTE_RPN_SET(temp_pte, rpn);

    /*
     * we are not using ref/change bits.
     * the hardware is more efficient if we set the
     * bits to 1 so it never has to update them.
     */
    PTE_R_SET(temp_pte, 1);
    PTE_C_SET(temp_pte, 1);
    /* w=0 no write-through
     * i=0 allow caching
     * m=0 no coherency enforced
     * g=0 no guarding
     */
    if (AccessMode::isCacheInhibit(perms)) {
	PTE_WIMG_SET(temp_pte, 6);	// don't store in cache, coherent
    } else {
	PTE_WIMG_SET(temp_pte, 2);	// store in cache, coherent
    }
    PTE_PP_SET(temp_pte, AccessMode::RWPerms(perms));

  if (!_BootInfo->onHV) {
    /*
     * the following must synchonize correctly with the mapping fault
     * handler.  To make this work, the update occurs by first setting
     * the entry invalid and locked.  Then, the rpn word is set, and
     * finally, the vsid word is set to the new value.
     * The mapping fault handler avoids locked entries.  Since we
     * are disabled, only one entry can be locked.
     */
    PTE_SET(temp_pte, pteToSet);
  } else {
	uval ptex = hash_index << LOG_NUM_PTES_IN_PTEG;
	while (1) {
	    ret = hcall_enter(retval, 0, ptex,
			      temp_pte.vsidWord, temp_pte.rpnWord);
//	    err_printf("H_ENTER %lx %lx %lx %llx %llx\n", ret, retval[0], ptex,
//		       temp_pte.vsidWord, temp_pte.rpnWord);
	    if (ret == H_Success) break;

	    /* we need to evict a pte from this pteg */
	    for (pteg_index=0 /* FIXME should be random */; ;
		 pteg_index=(pteg_index+1)&(NUM_PTES_IN_PTEG-1)) {
		ret = hcall_clear_ref(retval, 0, ptex | pteg_index);
		if (!(retval[0] & PTE_R_MASK)) {
		    ret = hcall_read(retval, 0, ptex | pteg_index);
		    if (!(retval[0] & PTE_BOLTED_MASK)) break;
		}
	    }
	    ret = hcall_remove(retval, 0, ptex | pteg_index, 0);
	}
  }

    // since we are disabled, the value of hashTableLock cannot change
    // between the guarded acquire above and here, since prepareToShare
    // must be called on the processor owning this InvertedPageTable object
    if (hashTableLock) hashTableLock->release();	// protect against other cpus
    enableHardwareInterrupts();
#ifdef PTE_STATS
    if ((num_maps & num_mask) == 0) printStats();
#endif
}

void
InvertedPageTable::enterBoltedPage(uval vaddr, uval paddr, uval logPageSize,
				   uval vsid, AccessMode::mode perms,
				   MemoryMgrPrimitiveKern *memory)
{
    struct PTE temp_pte;
    sval pteg_index;
    uval api;
    /* FIXME should be of unsigned 32 bit type */
    uval rpn;

    struct PTE *pteToSet = 0;
    uval hash_funct_ident;
    uval64 hash_index;
    uval ret, retval[8], ptex = 0;

    hash_funct_ident=0;

    passert(!(vsid&PTE_VSID_LOCK),
	    err_printf("vsid values must not use high order bit %lx\n", vsid));

    passert(AccessMode::isCacheInhibit(perms) ||
	    (paddr < _BootInfo->physEnd) ||
            (paddr < _BootInfo->physEnd+MIP::ExtendMemory(_BootInfo->physEnd)),
	    err_printf("tried to map %lx to %lx\n", vaddr, paddr));

    hash_index = getHTIndex(vaddr, vsid, logPageSize);

    temp_pte.clear();
    PTE_V_SET(temp_pte, 1);
    PTE_BOLTED_SET(temp_pte, 1);
    PTE_VSID_SET(temp_pte, vsid);
    PTE_H_SET(temp_pte, hash_funct_ident);

    api = VADDR_TO_API(vaddr);
    PTE_API_SET(temp_pte, api);
    rpn = (paddr) >> RPN_SHIFT;
    if (logPageSize != LOG_PAGE_SIZE) {
	PTE_L_SET(temp_pte, 1);
	rpn |= largePageFixup(logPageSize);
    }
    PTE_RPN_SET(temp_pte, rpn);

    /*
     * we are not using ref/change bits.
     * the hardware is more efficient if we set the
     * bits to 1 so it never has to update them.
     */
    PTE_R_SET(temp_pte, 1);
    PTE_C_SET(temp_pte, 1);
    /* w=0 no write-through
     * i=0 allow caching
     * m=0 no coherency enforced
     * g=0 no guarding
     */
    if (AccessMode::isCacheInhibit(perms)) {
	PTE_WIMG_SET(temp_pte, 6);	// don't store in cache, coherent
    } else {
	PTE_WIMG_SET(temp_pte, 2);	// store in cache, coherent
    }
    PTE_PP_SET(temp_pte, AccessMode::RWPerms(perms));

    /*
     * see if entry already exists - can happen when page tables
     * are shared by several processors
     */
    uval tempVsidWord = temp_pte.vsidWord &
	(PTE_VSID_MASK|PTE_API_MASK|PTE_V_MASK);
    uval curVsidWord;
    uval msr, nmsr;
    for (pteg_index=0; pteg_index<NUM_PTES_IN_PTEG; pteg_index++) {
	if (_BootInfo->onHV) {
	    ptex = (hash_index << LOG_NUM_PTES_IN_PTEG) | pteg_index;
	    ret = hcall_read(retval, 0, ptex);
	    curVsidWord = retval[0];
	} else {
	    pteToSet = &(hashTable[hash_index].entry[pteg_index]);
	    //curVsidWord = pteToSet->vsidWord;
	    asm volatile (
		"mfmsr %[msr]\n"
		"and %[nmsr], %[msr], %[dr]\n"
		"mtmsrd %[nmsr]\n"
		"isync\n"
		"ldx %[vsid], 0, %[pte]\n"
		"mtmsrd %[msr]\n"
		"isync\n"
		: [msr] "=&r" (msr), [nmsr] "=&r" (nmsr),
		[vsid] "=r" (curVsidWord)
		: [dr] "r" (~(PSL_DR|PSL_EE)),
		[pte] "r" (memory->physFromVirt(uval(pteToSet)))
		);
	}
	// compare to vsid/api/valid value - ignore H and Software bits
	uval testCurVsidWord;
	testCurVsidWord = curVsidWord & (PTE_VSID_MASK|PTE_API_MASK|PTE_V_MASK);
	if (testCurVsidWord == tempVsidWord) {
	    if (_BootInfo->onHV) {
		// on HV, entry can exist because boot created it in HV htab
		ret = hcall_remove(retval, 0, ptex, 0);
		break;
	    } else {
		passertMsg(curVsidWord & PTE_BOLTED_MASK,
			   "entry already exists NOT bolted\n");
	    }
	    return;
	}
    }

    passertMsg(!isShared(),
	       "trying to bolt page %lx in a shared page table\n",
	       vaddr);
    /*
     * Find a free entry.  Start at the end of the PTEG so that normal
     * searches won't have to skip over the bolted entry all the time.
     */
    for (pteg_index = NUM_PTES_IN_PTEG-1; pteg_index >= 0; pteg_index--) {
	if (_BootInfo->onHV) {
	    ptex = (hash_index << LOG_NUM_PTES_IN_PTEG) | pteg_index;
	    ret = hcall_read(retval, 0, ptex);
	    if (!(retval[0] & PTE_V_MASK)) break;
	} else {
	    pteToSet = &(hashTable[hash_index].entry[pteg_index]);
	    asm volatile (
		"mfmsr %[msr]\n"
		"and %[nmsr], %[msr], %[dr]\n"
		"mtmsrd %[nmsr]\n"
		"isync\n"
		"ldx %[vsid], 0, %[pte]\n"
		"mtmsrd %[msr]\n"
		"isync\n"
		: [msr] "=&r" (msr), [nmsr] "=&r" (nmsr),
		[vsid] "=r" (curVsidWord)
		: [dr] "r" (~(PSL_DR|PSL_EE)),
		[pte] "r" (memory->physFromVirt(uval(pteToSet)))
		);
	    if ((PTE_V_MASK & curVsidWord) == 0) break;
	}
    }

    passertMsg(pteg_index >= 0, "No free PTE\n");

    if (_BootInfo->onHV) {

	ret = hcall_enter(retval, H_EXACT, ptex,
			  temp_pte.vsidWord, temp_pte.rpnWord);

	passert(ret == H_Success,
		err_printf("hcall_enter failed err=%lx\n", ret));
    } else {
	uval msr, nmsr;
	asm volatile (
	    "mfmsr %[msr]\n"
	    "and %[nmsr], %[msr], %[dr]\n"
	    "mtmsrd %[nmsr]\n"
	    "isync\n"
	    "std %[bolt], 0(%[pte])\n"
            "eieio\n"
	    "std %[rpn], 8(%[pte])\n"
	    "eieio\n"
	    "std %[vsid], 0(%[pte])\n"
	    "mtmsrd %[msr]\n"
	    "isync\n"
	    : [msr] "=&r" (msr), [nmsr] "=&r" (nmsr)
	    : [dr] "r" (~(PSL_DR|PSL_EE)),
	    [pte] "b" (memory->physFromVirt(uval(pteToSet))),
	    [vsid] "r" (temp_pte.vsidWord), [rpn] "r" (temp_pte.rpnWord),
            [bolt] "r" (PTE_BOLTED_MASK)
	    );
    }
}


/*
 * If we have a shared page table, we need locking.  We use the
 * hashTableLock to synchonize IPT (InvertedPageTable Object)
 * accesses to the page table.  We use lolitaLock to independently
 * synchronize lolita (real mode interupt handler) access.
 * Interactions between a running IPT update and a lolita update
 * continue to use the PTE entry busy bit strategy.  This is
 * necessary since IPT code can suffer a mapping type fault that
 * must be serviced by lolita.  The hashtable and lolita lock
 * words reside in the first cpu's IPT object in its exception
 * local.  IPT's must use the VMapsR, rather than the exception
 * local, address of these lock words.  If slockaddr and
 * spinlockaddr are passed in, they are both VMapsR addresses, NOT
 * addresses in (processor specific) exception local.
 */
/*
 * prepare to share must be called on the processor being managed
 * by the instance of IPT that is called - namely the one in
 * exception local.
 */
InvertedPageTable*
InvertedPageTable::prepareToShare()
{
    if (hashTableLock == 0) {
	// first call - convert to sharing
	slock.init();
	spinLock = 0;
	// VmapsR address of hashTableLock - useable by any cpu
	hashTableLock = (SpinLock *) (uval(&exceptionLocal.pageTable.slock) -
				   exceptionLocal.kernelPSRDelta +
				   exceptionLocal.vMapsRDelta);
	// Real address of lolita spin lock
	lolitaLock = (uval*) (uval(&exceptionLocal.pageTable.spinLock) -
			      exceptionLocal.kernelPSRDelta);
    }
    // return V maps R address of this IPT - useable from anywhere
    return (InvertedPageTable*) (uval(this) -
				 exceptionLocal.kernelPSRDelta +
				 exceptionLocal.vMapsRDelta);
}
/*
 * initShared is called from any processor, but usually the new one
 * setting up its page tables.  The active IPT to share is passed
 * prepareToShare must have been called on the old IPT
 * before making this call.
 */
void
InvertedPageTable::initShared(InvertedPageTable* sharedIPT)
{
    passertMsg(sharedIPT->isShared(),
	       "initShared called before prepareToShare\n");
    *this = *sharedIPT;
    initStats();
}

void
InvertedPageTable::init(uval lgNumPTEs, uval vaddr, uval paddr)
{
    hashTableLock = 0;
    lolitaLock = 0;
    logNumPTEs = lgNumPTEs;
    hashBits = logNumPTEs-LOG_PTEG_SIZE+LOG_PTE_SIZE;
    hashTable = (PTEG *)vaddr;
    mask = (1ul<<hashBits) - 1;
    pte = paddr;

  if (vaddr != 0) {
    // zero page all PTEs
    //memset((void *) vaddr, 0, (1<<(logNumPTEs+LOG_PTE_SIZE)));
    // do this using real mode data accesses since page table is not
    // addressable
    uval i = 1<<(logNumPTEs+LOG_PTE_SIZE-3);// count of 8 byte words to zero
    uval msr, nmsr;
    asm volatile (
	"mtctr %[i]\n"
	"mfmsr %[msr]\n"
	"and %[nmsr], %[msr], %[dr]\n"
	"mtmsrd %[nmsr]\n"
	"isync\n"
	"0: stdu %[z], 8(%[p])\n"
	"bdnz 0b\n"
        "mtmsrd %[msr]\n"
        "isync\n"
	: [p] "+&b" (paddr-8), [msr] "=&r" (msr), [nmsr] "=&r" (nmsr)
        : [i] "r" (i) , [z] "r" (0), [dr] "r" (~(PSL_DR|PSL_EE))
	: "ctr");
  }

    segLoad = HARDWARE;
    largePage.numSizes = 0;			// no large pages
    uval hid;
    /*
     * If largePage.numSizes > 0, we use large pages for selected ranges of
     * mapping faults.  Code in exception/arch/powerpc/lolita.S assumes there
     * is only one large page size of 16M bytes (GP GQ GR ...) or, for machines
     * with multiple large page sizes, HID reg setup causes PTE_LP = SLB_LS = 0
     * to select 16MB pages, corresponding to largePage.logSizes[0] = 24.
     */
    switch (_BootInfo->cpu_version) {
    case VER_BE_PU:
	segLoad = SOFTWARE;
	largePage.numSizes = 2;
	largePage.logSizes[0] = 24;	// 16Meg
	largePage.logSizes[1] = 16;	// 64k
      if (!_BootInfo->onHV) {
	// set large page selector in HID6 to 16meg/64k mode
	__asm __volatile("mfspr %0, 1017" : "=r"(hid));
	hid = (hid & 0xffff0fffffffffff) | 0x200000000000;
	__asm __volatile("sync");
	__asm __volatile("mtspr 1017, %0" : : "r"(hid));
	__asm __volatile("isync");
      }
	break;
    case VER_970:
    case VER_970FX:
    case VER_GP:
    case VER_GQ:
	segLoad = SOFTWARE;
	largePage.logSizes[0] = 24;	// 16Meg
	if (!_BootInfo->onHV) {
	    largePage.numSizes = 1;
	    // set large page available in HID4 on
	    __asm __volatile("mfspr %0, 1012" : "=r"(hid));
	    hid &= ~4;
	    __asm __volatile("sync");
	    __asm __volatile("mtspr 1012, %0" : : "r"(hid));
	    __asm __volatile("isync");
	} else {
	    largePage.numSizes = 0;
	}
	break;
    default:;
    };

    initStats();
}

void
InvertedPageTable::initStats()
{
    num_dsi = 0;
    num_dsgi = 0;
    num_isi = 0;
    num_isgi = 0;
    num_segmiss = 0;
    num_map_fault = 0;
    num_map_evict =0;
#ifdef PTE_STATS
    memset((void *) by8, 0,  sizeof by8);
    memset((void *) by16, 0,  sizeof by16);
    num_seg_evict = 0;
    num_maps = 0;
    num_evict = 0;
    num_invalidate = 0;
    num_invalidateNotFound = 0;
    num_skipped = 0;
#endif
}

/*
 * For debugging only.  Lets us look at ExceptionLocal on other processors.
 * See initKernelMappings
 */
extern ExceptionLocal *ELocalDbg[];

void
InvertedPageTable::printStats()
{
    // could be static function but don't want to change caller yet
    uval i;
    for (i=0; i<Scheduler::VPLimit; i++) {
	if (ELocalDbg[i]) {
	    err_printf("\nPage Tables for cpu %ld\n", i);
	    ELocalDbg[i]->pageTable.printStatsInternal();
	}
    }
}

void
InvertedPageTable::printStatsInternal()
{
#ifdef PTE_STATS
    err_printf("%ld dsi, %ld dsgi, %ld isi, %ld isgi\n"
	       "%ld segmiss, %ld map_fault, %ld map_evict\n"
	       "%ld segment evicted\n",
	       num_dsi, num_dsgi, num_isi, num_isgi,
	       num_segmiss, num_map_fault, num_map_evict,
	       num_seg_evict);
    num_dsi = num_dsgi = num_isi = num_isgi = 0;
    num_segmiss = num_map_fault = num_map_evict = 0;
    num_seg_evict = 0;

    err_printf("%ld mapped, %ld evicted, %ld skipped, "
	       "%ld invalidate, %ld not found\n",
	       num_maps, num_evict, num_skipped,
	       num_invalidate, num_invalidateNotFound);
    num_maps = num_evict = num_skipped =
	num_invalidate = num_invalidateNotFound = 0;
    uval i, j, k, prim, sec;
    memset((void *) by8, 0,  sizeof by8);
    memset((void *) by16, 0,  sizeof by16);
    for (i=0,j = (1ul<<(logNumPTEs-3))-1;
	i<(1ul<<(logNumPTEs-3-1));i++,j--) {
	prim = sec = 0;
	for (k = 0; k < 8; k ++) {
	    if (PTE_V_GET(hashTable[i].entry[k])) prim++;
	    if (PTE_V_GET(hashTable[j].entry[k])) sec++;
	}
	by8[prim]++;
	by8[sec]++;
	by16[prim+sec]++;
    }
    err_printf("by8  ");
    for (i=0;i<9;i++) err_printf("%6ld ", by8[i]);
    err_printf("\nby16 ");
    for (i=0;i<8;i++) err_printf("%6ld ", by16[i]);
    err_printf("\nby16 ");
    for (i=8;i<17;i++) err_printf("%6ld ", by16[i]);
    err_printf("\n");
#endif
}

#include "mem/PageCopy.H"
void
InvertedPageTable::clear()
{
    PageCopy::Memset0((void *) hashTable, (uval(1) << logNumPTEs) * 16);
}
