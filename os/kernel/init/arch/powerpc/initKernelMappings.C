/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: initKernelMappings.C,v 1.183 2005/08/22 14:54:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#define TEST_DBL_HASH 0

#include "kernIncs.H"
#include <sys/KernelInfo.H>
#include <mmu.H>
#include <sys/syscalls.H>
#include "sys/thinwire.H"
#include <mem/SegmentTable.H>
#include <init/memoryMapKern.H>
#include <init/MemoryMgrPrimitiveKern.H>
#include <init/BootPrintf.H>
#include <init/kernel.H>	// for _OnSim
#include <sys/ppccore.H>
#include <misc/hardware.H>
#include <misc/arch/powerpc/trap.h>
#include <bilge/arch/powerpc/simos.H>
#include <bilge/arch/powerpc/BootInfo.H>
#include <init/BootAssert.H>

#include <sys/hcall.h>
#include "bilge/MIP.H"

/*
 * For debugging only.  Lets us look at ExceptionLocal on other processors.
 */
ExceptionLocal *ELocalDbg[Scheduler::VPLimit];
extern void tick(char c);

//#define pingMsg(x) *(volatile uval*)0xc0000000000001a8 = x;

// references to low level assembler code dispatchers
extern code lolita_trap_exc;
extern code lolita_trap_exc_end;
extern code lolita_mchk_exc;
extern code lolita_mchk_exc_end;
extern code lolita_mchk_code;
extern code lolita_mchk_code_end;
extern code lolita_involuntary_exc;
extern code lolita_involuntary_exc_end;
extern code lolita_involuntary_exc_ldhdlr;
extern code lolita_exc_sc;
extern code lolita_exc_sc_end;
extern code exc_pmac_smp_code_start;
extern code exc_pmac_smp_code_end;

// references to low level assembler code exception handlers
extern code exc_null_handler;
extern code exc_dec_null_handler;
extern code exc_dsi_handler;
extern code exc_dsgi_handler;
extern code exc_isi_handler;
extern code exc_isgi_handler;
extern code exc_exi_handler;
extern code exc_dec_handler;
extern code exc_pgm_handler;
extern code exc_perf_handler;
extern code exc_trap_handler;
extern code exc_lolita_svc_handler;
extern code exc_dbg_handler;

// instruction to be patched after choosing a pagetable size
extern code lolita_insert_pteg_idx;
extern code lolita_insert_pteg_HV_patch;
extern code locore_sdr1_HV_patch;

static void
copyExceptionCode(MemoryMgrPrimitiveKern *memory, uval exc, codeAddress src,
		  codeAddress srcEnd)
{
    uval dst = memory->virtFromPhys(exc);
    memcpy((void *) dst, (void *) src, srcEnd - src);
    // Synchronize the data and instruction caches.
    asm volatile ("sync; dcbst 0,%0; icbi 0,%0; isync"
		    : : "r" (dst));
}

static void
setExceptionHandler(VPNum vp, MemoryMgrPrimitiveKern *memory,
		    ExceptionLocal *elocal, uval exc, codeAddress hndlr)
{
    elocal->handlers[EXC_IDX(exc)] =
	(codeAddress) memory->physFromVirt((uval) hndlr);
    if (vp==0) {
	copyExceptionCode(memory, exc, &lolita_involuntary_exc,
			  &lolita_involuntary_exc_end);
	/*
	 * Patch the offset field in the load-handler instruction in
	 * the copied code to the correct offset into the handler table.
	 */
	uval ldaddr = (memory->virtFromPhys(exc) +
			(lolita_involuntary_exc_ldhdlr -
			 lolita_involuntary_exc));
	*((uval32 *) ldaddr) += (EXC_IDX(exc) * sizeof(uval));
    }
}

static void
initLOLITA(VPNum vp, ExceptionLocal *elocal, MemoryMgrPrimitiveKern *memory,
	   uval kernelPSR, uval commonPSR, uval ioRegionMap)
{
/*    SIMOS_BREAKPOINT; */
    /*
     * Set up mapping-fault descriptor for the V->R region.
     */
    elocal->vMapsRStart = memory->virtFromPhys(memory->physStart());
    elocal->vMapsREnd   = memory->virtFromPhys(memory->physEnd());
    elocal->vMapsREnd  += MIP::ExtendMemory(memory->physEnd());

    elocal->vMapsRDelta = memory->virtFromPhys(0) - 0;

    /*
     * Set up mapping-fault descriptor for kernel processor-specific region.
     */
    passertMsg(kernelPSpecificRegionEnd-kernelPSpecificRegionStart <=
	       SEGMENT_SIZE, "current design only supports on segment here\n");
    elocal->kernelPSRStart = kernelPSpecificRegionStart;
    elocal->kernelPSREnd   = kernelPSpecificRegionEnd;
    elocal->kernelPSRDelta = kernelPSpecificRegionStart -
					    memory->physFromVirt(kernelPSR);
    // kernelPSRVSID initialized later after pageTable is initialized

    /*
     * Set up mapping-fault descriptor for common processor-specific region.
     */
    passertMsg(commonPSpecificRegionEnd-commonPSpecificRegionStart <=
	       SEGMENT_SIZE, "current design only supports on segment here\n");
    elocal->commonPSRStart = commonPSpecificRegionStart;
    elocal->commonPSREnd   = commonPSpecificRegionEnd;
    elocal->commonPSRDelta = commonPSpecificRegionStart -
					    memory->physFromVirt(commonPSR);
    // commonPSRVSID initialize later
    elocal->commonPSRWritable = (uval) (&extRegsLocal);

    /*
     * The tracing region follows the common processor-specific region,
     * but space for it can't be allocated yet.  For now set up a
     * mapping-fault descriptor for a zero-length tracing region.
     */
    elocal->traceRgnStart = commonPSpecificRegionEnd;
    elocal->traceRgnEnd = elocal->traceRgnStart;
    elocal->traceRgnDelta = 0;

    /*
     * Set up mapping-fault descriptor for the I/O region.
     */
    elocal->ioRgnStart = IO_REGION_BASE;
    elocal->ioRgnEnd   = IO_REGION_BASE + (NUM_IOCHUNKS * IOCHUNK_SIZE);
    elocal->ioRgnMap   = memory->physFromVirt(ioRegionMap);

    /*
     * Set up lower bound on mapping fault addresses.  Page faults on
     * addresses below this boundary are assumed to not be mapping faults.
     * Faults on addresses at or above the boundary are mapping faults or
     * errors.
     */
    elocal->kernelRegionsEnd = KERNEL_REGIONS_END;
    elocal->trapHandler =
	(codeAddress) memory->physFromVirt(uval(&exc_trap_handler));
    elocal->lolitaSVCHandler =
	(codeAddress) memory->physFromVirt(uval(&exc_lolita_svc_handler));

    /*
     * Set up exception handlers.
     */
    if (vp == 0) {
	for (sval exc = EXC_RST; exc <= EXC_LAST; exc += 0x100) {
	    copyExceptionCode(memory, exc, &lolita_trap_exc,
			      &lolita_trap_exc_end);
	}
#if 0
	// testing code to point machine check and/or pgm check
	// at special serial output code - used if things are
	// so bad debugger won't work
	copyExceptionCode(memory, EXC_PGM, &lolita_mchk_exc,
			      &lolita_mchk_exc_end);
	copyExceptionCode(memory, EXC_MCHK, &lolita_mchk_exc,
			      &lolita_mchk_exc_end);
	// code to support above - assume second page is availabel
	copyExceptionCode(memory, 0x2000, &lolita_mchk_code,
			      &lolita_mchk_code_end);
#endif
	copyExceptionCode(memory, EXC_RST, &exc_pmac_smp_code_start,
			  &exc_pmac_smp_code_end);
    }

    setExceptionHandler(vp, memory, elocal, EXC_DSI, &exc_dsi_handler);
    setExceptionHandler(vp, memory, elocal, EXC_DSGI, &exc_dsgi_handler);
    setExceptionHandler(vp, memory, elocal, EXC_ISI, &exc_isi_handler);
    setExceptionHandler(vp, memory, elocal, EXC_ISGI, &exc_isgi_handler);
    setExceptionHandler(vp, memory, elocal, EXC_EXI, &exc_null_handler);
    setExceptionHandler(vp, memory, elocal, EXC_DEC, &exc_dec_null_handler);
    setExceptionHandler(vp, memory, elocal, EXC_PERF, &exc_null_handler);

    asm("std r2,%0" : "=m" (elocal->toc) : );
    elocal->msr = PSL_KERNELSET;
    elocal->msrUserChange = PSL_USERCHANGE;
    elocal->elocalVirt = uval64(&exceptionLocal);

    elocal->excPgfltExceptionUser   = &ExceptionLocal_PgfltExceptionUser;
    elocal->excPgfltExceptionKernel = &ExceptionLocal_PgfltExceptionKernel;
    elocal->excIOInterrupt          = &ExceptionLocal_IOInterrupt;
    elocal->excDecInterrupt         = &ExceptionLocal_DecInterrupt;
    elocal->excPerfInterrupt        = &ExceptionLocal_PerfInterrupt;
    elocal->excTrapExceptionUser    = &ExceptionLocal_TrapExceptionUser;
    elocal->excTrapExceptionKernel  = &ExceptionLocal_TrapExceptionKernel;

    /*
     * Setup the system call vector.
     */
    if (vp==0) {
	copyExceptionCode(memory, EXC_SC, &lolita_exc_sc, &lolita_exc_sc_end);
    }

    /*
     * Populate the system call vector.
     */
    for (int i = LAST_SYSCALL+1; i < SYSCALL_LIMIT; i++) {
	elocal->svc[i] = &ExceptionLocal_InvalidSyscall;
    }

    /*
     * Clear counters.
     */
    elocal->num_null = 0;
    elocal->num_exi = 0;
    elocal->num_dec = 0;
    elocal->num_trap = 0;
    elocal->num_nonnative_svc = 0;

}

#if TEST_DBL_HASH
static uval dblhash[2048/sizeof(uval)];
#endif


extern "C" void AsmInstallKernPageTables(uval64, uval64, uval64,
					 uval64, uval64, uval64);

void
installKernPageTables(ExceptionLocal *elocal, MemoryMgrPrimitiveKern *memory)
{
    uval64 num_ptes;

    uval logNumPTEs = elocal->pageTable.getLogNumPTEs();

    uval msr_set  = PSL_KERNELSET;	// keep interrupts disabled
    uval sdr1 = elocal->pageTable.getSDR1();
    // real addr of hardware asr with V bit set (0 for SLB machines)
    uval asr   = elocal->currentSegmentTable->getASR();
    num_ptes = 1 << logNumPTEs;

    /*
     * Do hardware init stuff in assembler
     */

    AsmInstallKernPageTables(memory->virtFromPhys(0), sdr1, msr_set,
			     memory->physFromVirt((uval) elocal), asr,
			     _BootInfo->onHV);

}

static uval ioRegionMap = (uval)NULL;
static uval ioVAddr = IO_REGION_BASE;

uval
ioReserve(uval len)
{
    uval nextva, vaddr;
    len = (len + (IOCHUNK_SIZE-1)) & ~(IOCHUNK_SIZE-1);
    if (len == 0) return (uval)NULL;
    nextva = ioVAddr + len;
    if ((nextva < exceptionLocal.ioRgnStart)
        || (nextva > exceptionLocal.ioRgnEnd)) return (uval)NULL;
    vaddr = ioVAddr;
    ioVAddr = nextva;
    return vaddr;
}

uval
ioMap(uval vaddr, uval paddr, uval len)
{
    uval n, i;
    if ((vaddr < exceptionLocal.ioRgnStart)
        || (vaddr > (ioVAddr - IOCHUNK_SIZE))) return (uval)NULL;
    len = (len + (IOCHUNK_SIZE-1)) & ~(IOCHUNK_SIZE-1);
    if (len == 0) return (uval)NULL;
    if (((vaddr + len) < exceptionLocal.ioRgnStart)
	|| ((vaddr + len) > ioVAddr)) return (uval)NULL;
    paddr = paddr & ~(PAGE_SIZE-1);
    n = (vaddr >> LOG_IOCHUNK_SIZE) & (NUM_IOCHUNKS-1);
    for (i = n; i < (n + (len >> LOG_IOCHUNK_SIZE)); i++) {
//	if (((uval*)ioRegionMap)[i] != (uval)NULL) return (uval)NULL;
	((uval*)ioRegionMap)[i] = paddr;
	paddr += IOCHUNK_SIZE;
    }
    return vaddr;
}

uval
ioReserveAndMap(uval paddr, uval len)
{
    uval vaddr;
    vaddr = ioReserve(len);
    if (vaddr == (uval)NULL) return (uval)NULL;
    return ioMap(vaddr, paddr, len) | (paddr & (PAGE_SIZE-1));
}

extern code kernVirtStart;
extern "C" int _end[];

void
InitKernelMappings(KernelInitArgs& kernelInitArgs)
{
    uval vp = kernelInitArgs.vp;
    MemoryMgrPrimitiveKern *memory = &(kernelInitArgs.memory);
    uval numPTEs;
    /*
     * Allocate physical memory for the kernel processor-specific memory area.
     * We can't get to it through its normal address yet, but we can access it
     * via a V->R address.
     */

/*    SIMOS_BREAKPOINT;   */ /* FIXME RICK */
    uval kernelPSR;
    memory->alloc(kernelPSR, kernelPSpecificRegionSize, PAGE_SIZE);

    /*
     * Allocate physical memory for the common processor-specific memory area.
     */
    uval commonPSR;
    memory->alloc(commonPSR, commonPSpecificRegionSize, PAGE_SIZE);

    /*
     * Allocate physical memory and initialize the I/O region mapping array,
     * if this is the first processor to initialize.
     */
    uval i;
    if (ioRegionMap == (uval)NULL) {
	memory->alloc(ioRegionMap, NUM_IOCHUNKS * 8, PAGE_SIZE);
	for (i = 0; i < NUM_IOCHUNKS; i++) {
	    ((uval*)ioRegionMap)[i] = (uval)NULL;
	}
    }

    /*
     * Construct a V->R address for exceptionLocal.
     */
    ExceptionLocal *elocal = (ExceptionLocal *)
	(kernelPSR + (uval(&exceptionLocal) - kernelPSpecificRegionStart));
    ELocalDbg[vp] = elocal;
    kernelInitArgs.elocal = elocal;	// for secondary start use
    kernelInitArgs.relocal = memory->physFromVirt((uval) elocal);

    initLOLITA(vp, elocal, memory, kernelPSR, commonPSR, ioRegionMap);

    uval maxPhys, curPhys;
    maxPhys = HWInterrupt::TotalPhys();
    tassertBootMsg(vp < maxPhys, "vp number %lx is too big\n", vp);

    curPhys = MIN(maxPhys, MAX(1, _BootInfo->processorCount));

    /*
     * The global (per-processor) kernelInfoLocal structure is mapped
     * read-only in both user and kernel modes.  Exception-level code uses
     * the V->R address stored in exceptionLocal.kernelInfoPtr when updating
     * the structure.  We initialize the onSim field as well as other fixed
     * values in kernelInfoLocal here once and for all.
     */
    elocal->kernelInfoPtr = (KernelInfo *)
	(commonPSR + (uval(&kernelInfoLocal) - commonPSpecificRegionStart));
    elocal->kernelInfoPtr->init(
	/* onHV */              _OnHV,
	/* onSim */		_OnSim,
	/* numaNode */		0,
	/* procsInNumaNode */	curPhys,
	/* physProc */		vp,
	/* curPhysProcs */	curPhys,
	/* maxPhysProcs */	maxPhys,
	/* sCacheLineSize */	_BootInfo->L2linesize,
	/* pCacheLineSize */	_BootInfo->dCacheL1LineSize,
	/* controlFlags */      _BootInfo->controlFlags);
    elocal->copyTraceInfo();

    /*
     * allocate bolted memory used by state restore code.
     */
    uval boltedPages;
    uval boltedPagesSize = _BootInfo->dCacheL1LineSize * maxPhys;
    if (vp == 0) {
	memory->alloc(boltedPages, boltedPagesSize, PAGE_SIZE);
	elocal->boltedRfiStatePage = (RfiState*)boltedPages;
    } else {
	// when doing other vp's we are running on the starting
	// processor and can address its exception local in the normal way
	elocal->boltedRfiStatePage = exceptionLocal.boltedRfiStatePage;
	boltedPages = uval(exceptionLocal.boltedRfiStatePage);
    }
    elocal->boltedRfiState =
	(RfiState*)(boltedPages + vp*_BootInfo->dCacheL1LineSize);
    elocal->boltedRfiStatePageSize = boltedPagesSize;

    //FIXME: where to get numa values? For now use 0 and total procs, but
    //        need more info on numa topology.

    uval logNumPTEs;
    uval htabSize;
    uval htab = 0;

    if (_BootInfo->onHV) {
	logNumPTEs = _BootInfo->naca.pftSize - LOG_PTE_SIZE;
    } else {
	/*
	 * Allocate physical memory for the hash table.  The hash
	 * table alignment must equal its size.  The primitive memory
	 * manager will keep track of the space lost to alignment for
	 * later recovery.

	 * FIXME: Research on pagetable sizing is needed.  The
	 * published guidelines assume a single pagetable shared
	 * across all processors.  We have a separate pagetable per
	 * processor, so the guidelines aren't applicable.  ...

	 * but will do for now, going to set the size of the page table
	 * to cover four times the amount memory we have (this is what
	 * the power pc manual recommends as the minimum and what Linux
	 * uses.  While we have not yet done research on what makes sense
	 * for an MP yet we will heuristically divide the amount of memory
	 * by the number of processors divided by two.

	 * FIXME: If you go for larger pagetables, you may have to
	 * increase secondaryBootMemSize in MPinit.C.
	 */
	/* old code
	   if (_BootInfo->physEnd <= 0x40000000) {
	       logNumPTEs = BASE_LOG_PTE;
	   } else {
	       logNumPTEs = BASE_LOG_PTE + 6;
	   }
	*/

	// as above, calculate to cover 4 times the number pages
	// curPhys is the number of actual processors we are going to start
	numPTEs = (((_BootInfo->physEnd/PAGE_SIZE)*4)/((curPhys+1)/2));
	logNumPTEs = 0;
	while (((uval)(1<<logNumPTEs)) < numPTEs) {
	    logNumPTEs++;
	}
	if (logNumPTEs < BASE_LOG_PTE) logNumPTEs = BASE_LOG_PTE;
    }

    if (vp == 0) {
	/*
	 * An instruction in the mapping fault handler inserts a PTEG
	 * index into the pagetable address to form a PTEG address.
	 * The number of bits to insert depends on the size of the
	 * pagetable, so we patch the instruction now that we know the
	 * size.  The instruction is of the form: rldimi rA,rS,SH,MB
	 * The "MB" field, which is encoded in the instruction in two
	 * pieces, must be patched to equal (60 - logNumPTEs).
	 */
	uval32 instr = (* (uval32 *) &lolita_insert_pteg_idx);
	instr =
	    (instr & ~((0x1f << 6) | (0x20 << 0))) | // clear existing MB field
	    (((60 - logNumPTEs) & 0x1f) << 6) |  // low-order 5 bits of MB
	    (((60 - logNumPTEs) & 0x20) << 0);   // high-order bit of MB
	(* (uval32 *) &lolita_insert_pteg_idx) = instr;

	// Synchronize the data and instruction caches.
	asm volatile ("sync; dcbst 0,%0; icbi 0,%0; isync"
		      : : "r" (&lolita_insert_pteg_idx));

    }

    if (_BootInfo->onHV) {
	// Patch this intruction to a NOP
	(* (uval32 *) &lolita_insert_pteg_HV_patch) = 0x60000000;

	// Synchronize the data and instruction caches.
	asm volatile ("sync; dcbst 0,%0; icbi 0,%0; isync"
		      : : "r" (&lolita_insert_pteg_HV_patch));

	// Patch this intruction to a NOP
	(* (uval32 *) &locore_sdr1_HV_patch) = 0x60000000;

	// Synchronize the data and instruction caches.
	asm volatile ("sync; dcbst 0,%0; icbi 0,%0; isync"
		      : : "r" (&locore_sdr1_HV_patch));
    }

    htabSize = (1 << (logNumPTEs + LOG_PTE_SIZE));

    if (vp == 0) {
	SysStatus rc;
	/* FIXME this allocation needs to be aligned in physical, here
	   we're only guaranteeing virtual alignment and it's working
	   because of our current V->R delta */
	if (!_BootInfo->onHV) {
	    rc = memory->allocFromChunks(htab, htabSize, htabSize);
	    passertSilent((rc==0), breakpoint());
	}
	elocal->pageTable.init(logNumPTEs, htab,
			       htab ? memory->physFromVirt(htab) : 0);
	BootPrintf::Printf("Page table %lx[%lx]\n", htab, htabSize);
    } else if (kernelInitArgs.sharedIPT) {
	// share page table
	elocal->pageTable.initShared(kernelInitArgs.sharedIPT);
    } else {
	// private page table provided by MPinit
	htab = kernelInitArgs.pageTable;
	elocal->pageTable.init(logNumPTEs, htab,
			       htab ? memory->physFromVirt(htab) : 0);
    }

    SegmentTable *st;    // the SegmentTable object

    /*
     * Allocate and initialize a SegmentTable object
     * SegmentTable chooses object based on hardware type info
     * from page table.
     * Pinned page/segment logic is now in SegmentTable object.
     */

    elocal->kernelPSRVSID = 0;
    elocal->kernelPSRVSID.VSID = elocal->pageTable.allocVSID();
    elocal->commonPSRVSID = 0;
    elocal->commonPSRVSID.VSID = elocal->pageTable.allocVSID();
    SegmentTable::CreateKernel(st, memory, elocal);
    elocal->currentSegmentTable =
	elocal->kernelSegmentTable = st;

    elocal->currentProcessAnnex = NULL;
#if TEST_DBL_HASH
    for (uval j = 0; j < 2048/sizeof(uval); j++) dblhash[j] = 0;
#endif

    if (vp == 0) {
	installKernPageTables(elocal, memory);

	/*
	 * on hardware, we premap all of the kernel to prevent mapping
	 * faults during Linux hardware init.  This is a kludge, but
	 * its the best we can think of for now.
	 * for debugging on simulator - map all physical memory
	 */

	uval vs = memory->virtFromPhys(memory->physStart());
	uval ve = (uval) (&_end);		// end of code/data/bss
	for (;vs<ve;vs+=PAGE_SIZE) {
	    *(volatile uval*)ve;
	}
	memory->touchAllocated();
    }
//  ioReserveAndMap(0, NUM_IOCHUNKS * IOCHUNK_SIZE);  // FIXME for test only
}

void MapTraceRegion(uval vaddr, uval paddr, uval size)
{
    passertMsg(vaddr >= exceptionLocal.commonPSREnd,
	       "Trace region must follow common processor-specific region.\n");

    exceptionLocal.traceRgnStart = vaddr;
    exceptionLocal.traceRgnEnd = vaddr + size;
    exceptionLocal.traceRgnDelta = vaddr - paddr;
}
