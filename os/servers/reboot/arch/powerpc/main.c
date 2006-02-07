/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.c,v 1.13 2005/04/28 06:53:49 jk Exp $
 *****************************************************************************/

/*****************************************************************************
 *
 *  Boot K42 on 64-bit PowerPC hardware
 *
 *****************************************************************************/

#include <sys/types.H>

#include "mmu.h"

/* reach into the kernel to get the BootInfo structure */
#include "../../../../kernel/bilge/arch/powerpc/BootInfo.H"
/* reach into the kernel to get the memoryMap defines */
#include "../../../../kernel/sys/memoryMap.H"

extern void writeCom2(char);

#define LOG_PGSIZE 12
#define PGSIZE (1<<LOG_PGSIZE)

#define BOOT_STACK_SIZE (20 * PGSIZE)

#define MSR_SF_BIT (1ULL << 63)
#define MSR_ISF_BIT (1ULL << 61)
#define MSR_HV_BIT (1ULL << 60)
#define MSR_ME_BIT (1ULL << 12)
#define MSR_IR_BIT (1ULL << 5)
#define MSR_DR_BIT (1ULL << 4)

unsigned long long logNumPTEs;	/* log base 2 of page table size */
unsigned long long pageTableSize;
unsigned long long pageTableAddr;
unsigned long long segmentTableAddr;
unsigned long long segmentTableSize;

struct KA {
    uval64 asr;
    uval64 sdr1;
    uval64 msr;
    uval64 iar;
    uval64 toc;
    uval64 stack;
    uval64 bootInfo;
    uval64 vsid0;
    uval64 esid0;
    uval64 vsid1;
    uval64 esid1;
};

void copyDWords(unsigned int, unsigned int, unsigned int);
void syncICache(unsigned int, unsigned int, unsigned int);
void launch(struct KA *);


struct SegInfo {
    uval64 pStart, vStart, size, filePtr;
};

struct ParseInfo {
    struct SegInfo text;
    struct SegInfo data;
    struct SegInfo bss;
    uval64 *entryFunc;
};

static int ParseElf(uval64 kernStartAddr, struct ParseInfo *pi);

void mapRange(uval64 vaddr, uval64 paddr, uval64 size);

void dumpSegmentTable (uval64 ptr);
void dumpPageTable    (uval64 ptr, int size);

void of_exit()
{
    for (;;);
}

#define printf(args...) do {} while (0)

/*----------------------------------------------------------------------------*/
int main(struct BootInfo *bootInfo);
void
__start(struct BootInfo* bootInfo)
{
    main(bootInfo);
}

int
main(struct BootInfo *bootInfo)
{
    uval32 kernbase;
    struct KA kernelArgs;
    struct xcoffhdr *xcoffp;
    struct scnhdr *scnp;
    uval64 scnNum;
    uval64 kernEnd;
    uval64 stkEnd;
    uval64 vMapsRDelta, numPages;
    uval64 *entryFunc;
    uval64 *addr;
    struct ParseInfo pi;
    uval32 memBottom;

//    writeZilog('\r');  writeZilog('A');  writeZilog('B');  writeZilog('C');

    printf("\n\n\rK42 CHRP Booter\n\r");
    printf("Built on %s at %s\n\r", __DATE__, __TIME__);

    kernbase = bootInfo->rebootImage;
    printf("Kernel located at: %08lx\n", kernbase);

    if (!ParseElf(kernbase, &pi)) {
	printf("error: image is not a 64-bit ELF.");
	while (1) {};
    }

    vMapsRDelta = pi.text.vStart - bootInfo->kernelImage;

    pi.text.pStart = pi.text.vStart - vMapsRDelta;
    pi.data.pStart = pi.data.vStart - vMapsRDelta;
    pi.bss.pStart = pi.bss.vStart - vMapsRDelta;

    printf("Text: vaddr 0x%016llX   paddr 0x%016llX\n\r"
	   "       size 0x%016llX  offset 0x%016llX\n\r",
	   pi.text.vStart, pi.text.pStart, pi.text.size, pi.text.filePtr);

    printf("Data: vaddr 0x%016llX   paddr 0x%016llX\n\r"
	   "       size 0x%016llX  offset 0x%016llX\n\r",
	   pi.data.vStart, pi.data.pStart, pi.data.size, pi.data.filePtr);

    printf("BSS:  vaddr 0x%016llX   paddr 0x%016llX \n\r"
	   "       size 0x%016llX\n\r",
	   pi.bss.vStart, pi.bss.pStart, pi.bss.size);

    if ((pi.data.vStart + pi.data.size) != pi.bss.vStart) {
	printf("Data and BSS segments are not contiguous.\n\r");
	printf("Exiting...\n\r");
	of_exit();
    }

    if ((pi.text.vStart + pi.text.size) > pi.data.vStart) {
	printf("Text and data segments overlap.\n\r");
	printf("Exiting...\n\r");
	of_exit();
    }

    if (pi.bss.pStart + pi.bss.size >
	pi.text.pStart + bootInfo->kernelImageSize) {
	printf("Kernel too large\n\r");
	printf("Exiting...\n\r");
	of_exit();
    }
    /*
     * Copy text and data (data first because ranges may overlap).
     */
    printf("Copying data... %llX-%llX -> %llX-%llX  [%llX]...",
	   pi.data.filePtr, pi.data.filePtr + pi.data.size,
	   pi.data.pStart, pi.data.pStart + pi.data.size,
	   (uval64)pi.data.size);
    copyDWords(pi.data.pStart, pi.data.filePtr, pi.data.size / 8);
    printf("done.\n\r");

    printf("Copying text... %llX-%llX -> %llX-%llX  [%llX]...",
	   pi.text.filePtr, pi.text.filePtr + pi.text.size,
	   pi.text.pStart, pi.text.pStart + pi.text.size,
	   (uval64)pi.text.size);
    copyDWords(pi.text.pStart, pi.text.filePtr, pi.text.size / 8);
    printf("done.\n\r");

    /*
     * Synchronize data and instruction caches for text area.
     */
    printf("Synchronizing data and instruction caches...");
    syncICache(pi.text.pStart, pi.text.pStart + pi.text.size,
	       bootInfo->dCacheL1LineSize);
    printf("done.\n\r");

    /* initial stack precedes RTAS */
    if (bootInfo->rtas.base) {
	stkEnd = bootInfo->rtas.base;
    } else {
	stkEnd = (uval64) bootInfo;
    }
    stkEnd = (stkEnd - BOOT_STACK_SIZE) & ~(PGSIZE - 1);

    /*
     * We need a page table large enough to map in memory from 0 to
     * the end of BSS, and we need one page for the hardware Segment Table.
     */
    kernEnd = pi.text.pStart + bootInfo->kernelImageSize;
    numPages = (kernEnd + (PGSIZE-1)) / PGSIZE;
    logNumPTEs = LOG_NUM_PTES_MIN;
    while ((1ULL << logNumPTEs) < numPages) {
	logNumPTEs++;
    }

    pageTableSize = 1ULL << (logNumPTEs + LOG_PTE_SIZE);
    segmentTableSize = PGSIZE;

    /* align up to pageTableSize boundary */
    pageTableAddr = (kernEnd + (pageTableSize - 1)) & ~(pageTableSize - 1);

    printf("Placing initial page table at %016llX\n\r", pageTableAddr);

    printf("Clearing page table.\n\r");
    for (addr = (uval64 *) pageTableAddr;
	 addr < ((uval64 *) (pageTableAddr + pageTableSize));
	 addr++)
    {
	*addr = 0;
    }

    /*
     * Put the segment table in the page following the page table
     */
    segmentTableAddr = pageTableAddr + pageTableSize;
    printf ("Placing initial segment table at %016llX\n\r", segmentTableAddr);

    /* zero the page containing the nascent segment table */
    for (addr = (uval64 *) segmentTableAddr;
	 addr < ((uval64 *) (segmentTableAddr + PGSIZE));
	 addr++)
    {
	*addr = 0;
    }

    printf("Mapping kernel image (text, data, bss).\n\r");
    mapRange(pi.text.vStart, pi.text.pStart, bootInfo->kernelImageSize);
    printf("Mapping exception vector area.\n\r");
    mapRange(0 + vMapsRDelta, 0, 3*PGSIZE);
    printf("Mapping bootInfo.\n\r");
    mapRange(((uval64) bootInfo) + vMapsRDelta, ( (uval64)bootInfo),
	     pi.text.pStart - (uval64)bootInfo);
    printf("Mapping boot stack.\n\r");
    mapRange(stkEnd + vMapsRDelta, stkEnd, BOOT_STACK_SIZE);

    kernelArgs.asr  = segmentTableAddr | 1; /* low bit of ASR is its "V" bit */
    kernelArgs.sdr1 = pageTableAddr | (logNumPTEs - LOG_NUM_PTES_MIN);

    kernelArgs.msr = MSR_ISF_BIT| MSR_SF_BIT | MSR_HV_BIT | MSR_ME_BIT |
		     MSR_IR_BIT | MSR_DR_BIT;

    kernelArgs.iar = pi.entryFunc[0];
    kernelArgs.toc = pi.entryFunc[1];

    /*
     * Start preliminary kernel stack immediately below bootInfo.
     * Create fake parent frame for kinit.C:start() to "save" LR and CR
     */
    kernelArgs.stack = ((stkEnd + BOOT_STACK_SIZE) - 0x70) + vMapsRDelta;

    kernelArgs.bootInfo = ((uval64) bootInfo) + vMapsRDelta;

    printf("BSS will be cleared after closing OpenFirmware.\n\r");

    dumpSegmentTable (segmentTableAddr);
#if 0
    dumpPageTable (pageTableAddr, pageTableSize);
#endif

    printf("\nAbout to jump to K42...\n\r");
    printf("    asr:              %016llX\n\r", kernelArgs.asr);
    printf("    sdr1:             %016llX\n\r", kernelArgs.sdr1);
    printf("    msr:              %016llX\n\r", kernelArgs.msr);
    printf("    iar:              %016llX\n\r", kernelArgs.iar);
    printf("    TOC address:      %016llX\n\r", kernelArgs.toc);
    printf("    stack pointer:    %016llX\n\r", kernelArgs.stack);
    printf("    bootInfo address: %016llX\n\r", kernelArgs.bootInfo);
    printf("    rtas base:        %016llX\n\r", bootInfo->rtas.base);
    printf("    rtas entry:       %016llX\n\r", bootInfo->rtas.entry);
    printf("    availCPUs:        %016llX\n\r", bootInfo->availCPUs);
    printf("    masterCPU:        %016llX\n\r", bootInfo->masterCPU);

    /*
     * Clear BSS to zeros
     */
    for (addr = (uval64 *) pi.bss.pStart;
	 addr < ((uval64 *) (pi.bss.pStart + pi.bss.size));
	 addr++)
    {
	*addr = 0;
    }

    kernelArgs.vsid0 = 0;
    if (bootInfo->cpu_version == VER_GP
	|| bootInfo->cpu_version == VER_GQ
	|| bootInfo->cpu_version == VER_970
	|| bootInfo->cpu_version == VER_970FX
	|| bootInfo->cpu_version == VER_BE_PU) {

	/* Compute initial SLB entries in kernelArgs for launch init, at the
	 * end of boot.  Segment that contains kernel text will go into the
	 * 0th (hardware bolted) entry.  Init should verify that
	 * ExceptionLocal_CriticalPage is contained in this segment.  The
	 * processor-specific base segment will go into the 1st entry.  Init
	 * should verify that exceptionLocal is contained in this segment.
	 * Software needs to preserve (reload) this segment after each
	 * execution of slbia.
	 */

	/* vsid = 0x0001.esid, Ks=0 Kp=1 NLC=0 */
	kernelArgs.vsid0 = ((pi.text.vStart >> 16) & 0xFFFFFFFFF000ULL)
			   | 0x0001000000000400ULL;
	/* esid = 36 bits of ea, V=1 index=0 */
	kernelArgs.esid0 = (pi.text.vStart & 0xFFFFFFFFF0000000ULL) | 0x8000000;
	printf("SLB new 0th vsid %016llX\n\r", kernelArgs.vsid0);
	printf("SLB new 0th esid %016llX\n\r", kernelArgs.esid0);

	/* vsid = 0x0001.esid, Ks=0 Kp=1 NLC=0 */
	kernelArgs.vsid1 = (KERNEL_PSPECIFIC_BASE >> 16)
			   | 0x0001000000000400ULL;
	/* esid = 36 bits of ea, V=1 index=1 */
	kernelArgs.esid1 = KERNEL_PSPECIFIC_BASE | 0x8000001;
	printf("SLB new 1st vsid %016llX\n\r", kernelArgs.vsid1);
	printf("SLB new 1st esid %016llX\n\r", kernelArgs.esid1);
    }

    launch(&kernelArgs);

    of_exit();
}

/*----------------------------------------------------------------------------*/
uval64
stegIndex (uval64 eaddr)
{
    return (eaddr >> 28) & 0x1F;	/* select 5 low-order bits from ESID part of EA */
}

uval64
ptegIndex(uval64 vaddr, uval64 vsid)
{
    uval64 hash;
    uval64 mask;

    hash = (vsid & VSID_HASH_MASK) ^ ((vaddr & EA_HASH_MASK) >> EA_HASH_SHIFT);
    mask = (1ULL << (logNumPTEs - LOG_NUM_PTES_IN_PTEG)) - 1;

    return (hash & mask);
}

void
mapPage(uval64 vaddr, uval64 paddr)
{
    uval64 vsid, esid;
    struct STEG *stegPtr;
    struct STE  *stePtr;
    struct PTEG *ptegPtr;
    struct PTE  *ptePtr;
    struct PTE   pte;
    uval i;
    int segment_mapped = 0;

    /*
     * Kernel effective addresses have the high-order bit on, thus a
     * kernel ESID will be something like 0x80000000C (36 bits).
     * The 52-bit VSID for a kernel address is formed by prepending
     * 0x7FFF to the top of the ESID, thus 0x7FFF80000000C.
     */
    esid = (vaddr >> 28) & 0xFFFFFFFFFULL;
    vsid = esid | 0x7FFF000000000ULL;

    /*
     * Make an entry in the segment table to map the ESID to VSID,
     * it it's not already present.
     */
    stegPtr = &(((struct STEG *) segmentTableAddr)[stegIndex (vaddr)]);
    for (i=0; i<NUM_STES_IN_STEG; i++) {
	stePtr = &(stegPtr->entry[i]);
	if (STE_PTR_V_GET (stePtr) &&	          /* entry valid, and */
	    STE_PTR_VSID_GET (stePtr) == vsid) {  /* vsid matches */
	    segment_mapped = 1;
	    break;
	}
	if (!STE_PTR_V_GET (stePtr))	/* entry not yet used */
	    break;
    }
    if (!segment_mapped) {
	printf ("Creating STE for 0x%016llX, esid 0x%016llX, vsid 0x%016llX\n\r", vaddr, esid, vsid);
	if (i >= NUM_STES_IN_STEG) {
	    printf ("Error:  Boot-time segment table overflowed (!)\n\r");
	    printf("Exiting...\n\r");
	    of_exit();
	}
	/*
	 * Create segment table entry in stegPtr->entry[i].
	 * We don't need to be careful about the order in which we do this,
	 * as address translation hasn't been turned on yet.
	 */
	STE_PTR_CLEAR (stePtr);
	STE_PTR_ESID_SET (stePtr, esid);
	STE_PTR_VSID_SET (stePtr, vsid);
	STE_PTR_V_SET (stePtr, 1);

    }

    /*
     * Make a page table entry
     */
    ptegPtr = &(((struct PTEG *)pageTableAddr)[ptegIndex(vaddr, vsid)]);

    for (i = 0; i < NUM_PTES_IN_PTEG; i++) {
	ptePtr = &(ptegPtr->entry[i]);
	if (PTE_PTR_V_GET(ptePtr) == 0) break;
    }
    if (i >= NUM_PTES_IN_PTEG) {
	printf ("Creating PTE for 0x%016llX, vsid 0x%016llX, ptegIndex 0x%016llX",
		vaddr, vsid, ptegIndex (vaddr, vsid));
	printf("\n\r");
	printf("Error:  Boot-time page table overflowed\n\r");
	printf("Exiting...\n\r");
	of_exit();
    }

    if (ptegIndex (vaddr, vsid) == 0x10C) {
	printf ("Creating PTE for 0x%016llX, vsid 0x%016llX, ptegIndex 0x%016llX, index %d\n\r",
		vaddr, vsid, ptegIndex (vaddr, vsid), i);
    }
    PTE_CLEAR(pte);
    PTE_V_SET(pte, 1);
    PTE_VSID_SET(pte, vsid);
    PTE_H_SET(pte, 0);

    PTE_API_SET(pte, VADDR_TO_API(vaddr));
    PTE_RPN_SET(pte, (paddr >> RPN_SHIFT));

    PTE_R_SET(pte, 0);
    PTE_C_SET(pte, 0);
    /* w=0 no write-through
     * i=0 allow caching
     * m=1 coherency enforced
     * g=0 no guarding
     */
    PTE_WIMG_SET(pte, 2);
    PTE_PP_SET(pte, 2 /* writeUserWriteSup */);
    PTE_SET(pte, ptePtr);
}

void
mapRange(uval64 vaddr, uval64 paddr, uval64 size)
{
    uval64 offset;

    printf ("  range: vaddr %016llX, paddr %016llX, size %016llX\n\r",
	    vaddr, paddr, size);
    for (offset = 0; offset < size; offset += PGSIZE) {
	mapPage(vaddr + offset, paddr + offset);
    }
}

/*
 * Print page table entries for debugging
 */
void
dumpPageTable (uval64 ptr, int size)
{
    uval64 *pte = (uval64 *) (unsigned long) ptr;
    int     row, col;
    int     rows = size / 128;

    printf ("Page table at %llX (REAL)\n\r", ptr);

    for (row=0; row<rows; row++) {
	int hdr_printed = 0;
	for (col=0; col<8; col++) {
	    if (pte[0] | pte[1]) {
		if (!hdr_printed) {
		    printf ("  row 0x%X\n\r", row);
		    hdr_printed = 1;
		}
		printf ("    %d  ", col);
		printf ("%016llX %016llX\n\r", pte[0], pte[1]);
		printf ("          vsid=%013llX api=%02llX ",
			pte[0] >> 12,
			(pte[0] >> 7) & 0x1FULL);
		printf ("h=%c v=%c rpn=%013llX ",
			pte[0] & 2 ? '1' : '0',
			pte[0] & 1 ? '1' : '0',
			pte[1] >> 12);
		printf ("r=%c c=%c wimg=%c%c%c%c pp=%d\n\r",
			pte[1] & 0x100 ? '1' : '0',
			pte[1] & 0x80 ? '1' : '0',
			pte[1] & 0x40 ? '1' : '0',
			pte[1] & 0x20 ? '1' : '0',
			pte[1] & 0x10 ? '1' : '0',
			pte[1] & 0x08 ? '1' : '0',
			pte[1] & 3);

	    }
	    pte += 2;
	}
    }
}

/*
 * Print page table entries for debugging
 */
void
dumpSegmentTable (uval64 ptr)
{
    uval64  seg_tab_real_addr = ptr & -2LL;  /* eliminate "V" bit from pointer */
    int     row, col;
    uval64 *ste;

    printf ("Segment table at %llX (REAL)\n\r", seg_tab_real_addr);
    ste = (uval64 *) (unsigned long) seg_tab_real_addr;

    for (row=0; row<32; row++) {
	int hdr_printed = 0;
	for (col=0; col<8; col++) {
	    if (ste[0] | ste[1]) {
		if (!hdr_printed) {
		    printf ("  hash .......%02X.......\n\r", row);
		    hdr_printed = 1;
		}
		printf ("    %d  ", col);
		printf ("%016llX %016llX  ", ste[0], ste[1]);
		printf ("esid=%09llX ", ste[0] >> 28);
		printf ("v=%c t=%c ks=%c kp=%c n=%c ",
			ste[0] & 0x80ULL ? '1' : '0',
			ste[0] & 0x40ULL ? '1' : '0',
			ste[0] & 0x20ULL ? '1' : '0',
			ste[0] & 0x10ULL ? '1' : '0',
			ste[0] & 0x08ULL ? '1' : '0');
		printf ("vsid=%013llX\n\r",
			ste[1] >> 12);
	    }
	    ste += 2;
	}
    }
}

static int
ParseElf(uval64 kernStartAddr, struct ParseInfo *pi)
{
    struct elf64_hdr {
	uval8  e_ident[16];
	sval16 e_type;
	uval16 e_machine;
	sval32 e_version;
	uval64 e_entry;
	uval64 e_phoff;
	uval64 e_shoff;
	sval32 e_flags;
	sval16 e_ehsize;
	sval16 e_phentsize;
	sval16 e_phnum;
	sval16 e_shentsize;
	sval16 e_shnum;
	sval16 e_shstrndx;
    };
    struct elf64_phdr {
	sval32 p_type;
	sval32 p_flags;
	uval64 p_offset;
	uval64 p_vaddr;
	uval64 p_paddr;
	uval64 p_filesz;
	uval64 p_memsz;
	uval64 p_align;
    };

    enum {
	PF_X = (1 << 0),
	PF_W = (1 << 1),
	PF_R = (1 << 2),
	PF_ALL = (PF_X | PF_W | PF_R),
	PT_LOAD = 1,
	PT_GNU_STACK = 0x60000000 + 0x474e551
    };
    struct elf64_hdr *e64 = (struct elf64_hdr *)kernStartAddr;
    struct elf64_phdr *phdr;
    int i;
    printf("Kern start %lx\n", kernStartAddr);
    if ((int)e64->e_ident[0] != 0x7f
	|| e64->e_ident[1] != 'E'
	|| e64->e_ident[2] != 'L'
	|| e64->e_ident[3] != 'F'
	|| e64->e_machine != 21 /* EM_PPC64 */) {
	printf("Bad header: %lx\n", e64->e_ident[0]);
	return (0);
    }
    phdr = (struct elf64_phdr *)(uval64)((uval)e64 + e64->e_phoff);

    // We know that the order is text, data that includes bss
    if (e64->e_phnum > 3) {
	printf("ERROR: too many ELF segments %llx\n",(uval64) e64->e_phnum);
	return 0;
    }

    /* FIXME: This is cheeesy */
    for (i = 0; i < e64->e_phnum; i++) {
	if (phdr[i].p_type == PT_LOAD) {
	    if (phdr[i].p_flags == (PF_R | PF_X)) {
		pi->text.filePtr  = kernStartAddr + phdr[i].p_offset;
		pi->text.size = phdr[i].p_filesz;
		pi->text.vStart  = phdr[i].p_vaddr;
		if (phdr[i].p_memsz != pi->text.size) {
		    printf("ERROR: text memsz and text filesz not equal\n");
		    return 0;
		}
	    } else if (phdr[i].p_flags & PF_W) {
		pi->data.filePtr = kernStartAddr + phdr[i].p_offset;
		pi->data.size = phdr[i].p_filesz;
		pi->data.vStart  = phdr[i].p_vaddr;

		// bss
		pi->bss.filePtr = 0;
		pi->bss.size = phdr[i].p_memsz - phdr[i].p_filesz;
		pi->bss.vStart  = phdr[i].p_vaddr + phdr[i].p_filesz;

		// entry is a function descriptor
		if (e64->e_entry >= phdr[i].p_vaddr &&
		    e64->e_entry <= phdr[i].p_vaddr +
		    phdr[i].p_memsz) {

		    pi->entryFunc = (uval64 *)(uval64)
			(pi->data.filePtr + e64->e_entry - phdr[i].p_vaddr);
		    printf("Entry desc: %lx\n", e64->e_entry);
		    printf("            %lx\n",(uval64)pi->entryFunc);
		    printf("            %lx\n",(uval64)pi->entryFunc[0]);
		    printf("            %lx\n",(uval64)pi->entryFunc[1]);
		} else {
		    printf("ERROR: entry descriptor is not in data segment\n");
		    return 0;
		}
	    }
	} else if (phdr[i].p_type == PT_GNU_STACK) {
	    /* We have a GNU_STACK segment - if we want to implement NX pages,
	     * turn on here */
	    printf("Found a GNU_STACK segment");
	}
    }
    return 1;
}

