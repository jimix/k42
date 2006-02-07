/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simos-boot.c,v 1.60 2005/02/16 00:05:54 mergen Exp $
 *****************************************************************************/

#include <sys/types.H>

#include "mmu.h"

/* reach into the kernel to get the BootInfo structure */
#include "../../../kernel/bilge/arch/powerpc/BootInfo.H"

#define REAL_MODE_SIMOS_CALL 0
#define VIRT_MODE_SIMOS_CALL 1

#define SIMOS_CONS_FUN		0x00	/* Console output		*/
#define SimGetMachAttrK 68      /* 68 */ /* get a machine attribute */
#define SimPhysMemCopyK    69   /* a fast phys-phys mem copy */
#define SimPhysMemSetK 71       /* 71 */ /* a fast set memory */

#define SimNumbPhysCpusK 0
#define SimMemorySizeK 1

#define LOG_PGSIZE 12
#define PGSIZE (1<<LOG_PGSIZE)

/*
 * The physical address at which the kernel is to be loaded.
 */
#define LOAD_ADDRESS 0x2000000

/*
 * Space reserved for the kernel image (text, data, bss) plus extra that the
 * kernel can use for early dynamic allocation.
 */
#define KERNEL_SPACE (48 * (1024*1024))

#define BOOT_STACK_SIZE (20 * PGSIZE)
#define RTAS_SIZE 0x28000

#define SIMOS_BREAKPOINT   asm(".long 0x7C0007CE")

#define MSR_SF_BIT (1ULL << 63)
#define MSR_ISF_BIT (1ULL << 61)
#define MSR_ME_BIT (1ULL << 12)
#define MSR_IR_BIT (1ULL << 5)
#define MSR_DR_BIT (1ULL << 4)

uval32 logNumPTEs;	/* log base 2 of page table size */
uval32 pageTableSize;
uval64 pageTableAddr;
uval64 segmentTableAddr;


struct SegInfo {
    uval64 pStart, vStart, size, filePtr;
};

struct ParseInfo {
    struct SegInfo text;
    struct SegInfo data;
    struct SegInfo bss;
    uval64 *entryFunc;
};

static int ParseXCoff(uval64 kernStartAddr, struct ParseInfo *pi);
static int ParseElf(uval64 kernStartAddr, struct ParseInfo *pi);

long
SimOSSupport(unsigned int foo, ...)
{
    long ret;
    asm(".long 0x7c0007cc; mr %0,3" : "=&r" (ret) : /* no input */ : "r3");
    return ret;
}


long strLen (char *s)
{
    long i = 0;
    while (*s++) i++;
    return i;
}

void strCat (char *s, char *t)
{
    long ls = strLen (s);
    char *r = s + ls;
    while ((*r++ = *t++));
}

void strCpy(char *s, char *t)
{
    while ((*s++ = *t++));
}

char _hex[] = "0123456789ABCDEF";
char *printHex (char *s, unsigned long long v)
{
    int i, j;

    for (i=0; i<16; i++) {
	j = (v >> (60 - (4 * i))) & 0xF;
	*s++ = _hex[j];
    }
    *s = '\0';
    return s;
}

void
bprintf(char *buf)
{
    int len;
    char lbuf[256];

    strCpy(lbuf,buf);
    strCat (lbuf, "\r\n");
    len = strLen(lbuf);
    SimOSSupport(SIMOS_CONS_FUN, lbuf, len, REAL_MODE_SIMOS_CALL);
}

void
b1printf(char *buf, uval64 val)
{
    char *bufp;
    char lbuf[256];

    strCpy(lbuf,buf);

    bufp = lbuf + strLen(lbuf);
    bufp = printHex (bufp, val);
    bprintf (lbuf);
}

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
    mask = (((uval64)1) << (logNumPTEs - LOG_NUM_PTES_IN_PTEG)) - 1;

    return (hash & mask);
}

void
mapPage(uval64 vaddr, uval64 paddr)
{
    uval64       vsid, esid;
    struct STEG *stegPtr;
    struct STE  *stePtr;
    struct PTEG *ptegPtr;
    struct PTE  *ptePtr;
    struct PTE   pte;
    uval         i;
    int          segment_mapped = 0;

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
    stegPtr = &(((struct STEG *)segmentTableAddr)[stegIndex (vaddr)]);
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
	b1printf ("Creating STE for ", vaddr);
	b1printf ("esid ", esid);
	b1printf ("vsid ", vsid);
	if (i >= NUM_STES_IN_STEG) {
	    bprintf ("Error:  Boot-time segment table overflowed (!)\n");
	    SIMOS_BREAKPOINT;
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
    ptegPtr = &(((struct PTEG *) pageTableAddr)[ptegIndex(vaddr, vsid)]);

    for (i = 0; i < NUM_PTES_IN_PTEG; i++) {
	ptePtr = &(ptegPtr->entry[i]);
	if (PTE_PTR_V_GET(ptePtr) == 0) break;
    }
    if (i >= NUM_PTES_IN_PTEG) {
	bprintf("Error:  Boot-time page table overflowed\n");
	SIMOS_BREAKPOINT;
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
     * m=0 no coherency enforced
     * g=0 no guarding
     */
    PTE_WIMG_SET(pte, 0);
    PTE_PP_SET(pte, 2 /* writeUserWriteSup */);
    PTE_SET(pte, ptePtr);
}

void
mapRange(uval64 vaddr, uval64 paddr, uval64 size)
{
    uval64 offset;

    b1printf ("mapping range: vaddr ", vaddr);
    b1printf ("               paddr ", paddr);
    b1printf ("               size ", size);

    for (offset = 0; offset < size; offset += PGSIZE) {
	mapPage(vaddr + offset, paddr + offset);
    }
}

static void
parseLoadKernelAndRun(uval64 kernStartAddr)
{
    uval64 stkEnd, stkPtr, bootInfoV;
    uval64 memSize;
    uval64 vMapsRDelta, kernEnd, numPages;
    uval64 entry, toc;
    uval64 asr, sdr1;
    uval64 rtasPtr;
    struct BootInfo *bootInfo;
    uval64 msrval;
    struct ParseInfo pi;
    uval64 kernsize, *src, *dst;

    /*
     * Get memory size specified via set PARAM(MEMSYS.MemSize).
     */

    memSize = SimOSSupport(SimGetMachAttrK, SimMemorySizeK);

    if (!(ParseXCoff(kernStartAddr, &pi) ||
	  ParseElf(kernStartAddr, &pi))) {
	bprintf("error: image is not a 64-bit XCOFF or ELF.");
	SIMOS_BREAKPOINT;
    }

    vMapsRDelta = pi.text.vStart - LOAD_ADDRESS;

    if ((pi.data.filePtr+pi.data.size) > LOAD_ADDRESS) {
	bprintf("error: overlap between loaded image and kernel addr");
	SIMOS_BREAKPOINT;
    }

    b1printf("test start", pi.text.vStart);
    b1printf("vamps delta", vMapsRDelta);

    pi.text.pStart = pi.text.vStart - vMapsRDelta;
    pi.data.pStart = pi.data.vStart - vMapsRDelta;
    pi.bss.pStart = pi.bss.vStart - vMapsRDelta;

    if ((pi.data.vStart + pi.data.size) != pi.bss.vStart) {
	bprintf("Data and BSS segments are not contiguous.");
	SIMOS_BREAKPOINT;
    }

    if ((pi.text.vStart + pi.text.size) > pi.data.vStart) {
	bprintf("Text and data segments overlap.");
	SIMOS_BREAKPOINT;
    }

    /*
     * The entry point is specified by a function descriptor whose (data
     * segment) virtual address is in the aux header.  The function
     * descriptor contains:
     *       8-byte address of first instruction
     *       8-byte address of TOC
     */
    entry = pi.entryFunc[0];
    toc = pi.entryFunc[1];

    bprintf("\nClearing bss:");
    b1printf(" vaddr ", pi.bss.vStart);
    b1printf(" paddr ", pi.bss.pStart);
    b1printf(" size ", pi.bss.size);

    SimOSSupport(SimPhysMemSetK, pi.bss.pStart, ((void *) 0), pi.bss.size);

    bprintf("\nCopying data:");
    b1printf(" vaddr ", pi.data.vStart);
    b1printf(" paddr ", pi.data.pStart);
    b1printf(" size ", pi.data.size);
    b1printf(" file ptr ", pi.data.filePtr);

    SimOSSupport(SimPhysMemCopyK, pi.data.pStart,pi.data.filePtr, pi.data.size);

    bprintf("\nCopying text:");
    b1printf(" vaddr ", pi.text.vStart);
    b1printf(" paddr ", pi.text.pStart);
    b1printf(" size ", pi.text.size);
    b1printf(" file ptr ", pi.text.filePtr);

    SimOSSupport(SimPhysMemCopyK, pi.text.pStart,pi.text.filePtr, pi.text.size);
    /* Place bootInfo in the pages below kernel text. */
    bootInfo = (struct BootInfo *)(pi.text.pStart - sizeof(struct BootInfo));
    /* Align it to page boundary */
    bootInfo = (struct BootInfo *)(((uval64)bootInfo)  & ~(PGSIZE - 1));
    SimOSSupport(SimPhysMemSetK, (uval64 *) bootInfo,
		 (void *)(0), sizeof(struct BootInfo));

    bootInfo->onSim = 1ULL;	/* on simulator */
    bootInfo->kernelImage = LOAD_ADDRESS;
    bootInfo->kernelImageSize = KERNEL_SPACE;
    bootInfo->argString = 0ULL;	/* no argument string on simos */
    bootInfo->argLength = 0ULL;

    bootInfo->machine_type = 44;

    // pseudo-RTAS space precedes bootInfo
    rtasPtr = (((uval64) bootInfo) - RTAS_SIZE) & ~(PGSIZE - 1);
    bootInfo->rtas.base = rtasPtr;
    bootInfo->rtas.entry = 0ULL;

    bootInfo->controlFlags = 0;

    // On the simulator, tell the OS to use all available processors.
    bootInfo->processorCount = -1ULL;

    // initial stack precedes pseudo-RTAS
    stkEnd = (rtasPtr - BOOT_STACK_SIZE) & ~(PGSIZE - 1);

    /*
     * We need a page table large enough to map in memory from 0 to
     * the end of BSS, and we need one page for the hardware Segment Table.
     */
    kernEnd = pi.text.pStart + KERNEL_SPACE;
    numPages = (kernEnd + (PGSIZE-1)) / PGSIZE;
    logNumPTEs = LOG_NUM_PTES_MIN;
    while ((1ULL << logNumPTEs) < numPages) {
	logNumPTEs++;
    }

    pageTableSize = ((uval32)1) << (logNumPTEs + LOG_PTE_SIZE);

    /* align up to pageTableSize boundary */
    pageTableAddr = (kernEnd + (pageTableSize - 1)) &
	~(pageTableSize - 1);

    b1printf("Placing initial page    table at ", pageTableAddr);

    bprintf("Clearing page table.");
    SimOSSupport(SimPhysMemSetK, (uval64 *) pageTableAddr,
		 (void *)(0), pageTableSize);

    /*
     * Put the segment table in the page following the page table
     */
    segmentTableAddr = pageTableAddr + pageTableSize;
    b1printf ("Placing initial segment table at ", segmentTableAddr);

    bprintf("Clearing segment table.");
    SimOSSupport(SimPhysMemSetK, (uval64 *) segmentTableAddr,
		 (void *)(0), PGSIZE);

    if ((segmentTableAddr + PGSIZE) > memSize) {
	bprintf("Page and segment tables extend beyond the end of memory\n");
	SIMOS_BREAKPOINT;
    }

    bprintf("Mapping kernel image (text, data, bss).");
    mapRange(pi.text.vStart, pi.text.pStart, KERNEL_SPACE);
    bprintf("Mapping exception vector area.");
    mapRange(0 + vMapsRDelta, 0, 3*PGSIZE);
    bprintf("Mapping boot stack and bootInfo.");
    bprintf("Mapping bootInfo.");
    mapRange(((uval64) bootInfo) + vMapsRDelta, ((uval64) bootInfo),
	     pi.text.pStart - (uval64)bootInfo);
    bprintf("Mapping boot stack.");
    mapRange(stkEnd + vMapsRDelta, stkEnd, bootInfo->rtas.base - stkEnd);

    bootInfo->hwData = 0;
    bootInfo->hwDataSize = 0;


#if 0
    bootInfo->get_time_of_day = 0x4;
    bootInfo->set_time_of_day = 0x5;
    bootInfo->display_character = 0xc;
    bootInfo->set_indicator = 0xd;
    bootInfo->power_off = 0x13;
    bootInfo->system_reboot = 0x16;
#endif
    bootInfo->freeze_time_base = 0x1c;
    bootInfo->thaw_time_base = 0x1d;

    /*
     * Number of processors to simulate
     * set PARAM(CPU.Count)
     */
    bootInfo->availCPUs = (1ULL << SimOSSupport(SimGetMachAttrK,
						SimNumbPhysCpusK)) - 1;
    bootInfo->masterCPU = 0;

    /*
     * Memory is contiguous.  End of physical memory is equal to the size.
     */
    bootInfo->physEnd = memSize;
    bootInfo->realOffset = 0;

    /* Create LMB to match the mappings and reservations we've made above */
    bootInfo->lmb.memory.cnt = 1;
    bootInfo->lmb.memory.size = memSize;
    bootInfo->lmb.memory.region[0].size = memSize;

    bootInfo->lmb.reserved.cnt = 2;

    /* Reserve exception area*/
    bootInfo->lmb.reserved.region[0].size = 3*PGSIZE;
    bootInfo->lmb.reserved.region[0].base = 0;
    bootInfo->lmb.reserved.region[0].physbase = 0;
    bootInfo->lmb.reserved.size = bootInfo->lmb.reserved.region[0].size;

    /* Reserve from start of boot stack to end of kernel*/
    bootInfo->lmb.reserved.region[1].size =
	KERNEL_SPACE + (pi.text.pStart - stkEnd);
    bootInfo->lmb.reserved.region[1].base = stkEnd;
    bootInfo->lmb.reserved.region[1].physbase = stkEnd;
    bootInfo->lmb.reserved.size += bootInfo->lmb.reserved.region[1].size;


    bootInfo->rebootImage = memSize;	/* on simos no image is preserved */
    bootInfo->rebootImageSize = 0;
    bootInfo->dCacheL1Size = 0x10000;
    bootInfo->dCacheL1LineSize = 0x80;
    bootInfo->L2cachesize = 0x400000;
    bootInfo->L2linesize = 0x80;

    bootInfo->cpu_version = 0x32; /* Apache in Raven S/70 */
    /*
     * # Clock speed in MHz.
     * set PARAM(CPU.Clock)          200
     */
    bootInfo->clock_frequency = 200000000ULL;
    /* 100 MHz bus */
    bootInfo->bus_frequency = 100000000ULL;

    /* value for "simos -B" (?) on kitch0 (previously in KernelClock.H) */
    bootInfo->timebase_frequency = 20000000;

    /* some ISA device offsets */
    bootInfo->naca.serialPortAddr = 0xC0000000ULL + 0x3F8;

    /* thinwire channel offset */
    bootInfo->wireChanOffset = 0;

    /* simulated hard-disk number */
    bootInfo->simosDisk0Number = 0;
    bootInfo->simosDisk1Number = 1;

    // Start preliminary kernel stack immediately below bootInfo.
    // Create fake parent frame for kinit.C:start() to "save" LR and CR
    stkPtr = ((stkEnd + BOOT_STACK_SIZE) - 0x70) + vMapsRDelta;
    bootInfoV = ((uval64) bootInfo) + vMapsRDelta;

    msrval = MSR_SF_BIT | MSR_ME_BIT | MSR_IR_BIT | MSR_DR_BIT;

    asr = segmentTableAddr | 1;  /* low-order bit of ASR is its "V" bit */
    sdr1 = pageTableAddr | (logNumPTEs - LOG_NUM_PTES_MIN);

    bprintf("\nAbout to jump to K42 ...\n");
    b1printf("    asr:              ", asr);
    b1printf("    sdr1:             ", sdr1);
    b1printf("    msr:              ", msrval);
    b1printf("    entry point:      ", entry);
    b1printf("    stack pointer:    ", stkPtr);
    b1printf("    TOC address:      ", toc);
    b1printf("    bootInfo address: ", bootInfoV);

    asm volatile ("\n"
	"       # inlined assembly to switch to 64-bit mode virtual,\n"
	"       # setting up sdr1, segment registers, msr, PC, toc and stkptr.\n"
	"       mtasr  %0               # set asr (segment table addr)\n"
	"       mtsdr1 %1		# set sdr1 (page table addr and length)\n"

	"li     0,0x5A5A         # an invalid SID value (FIXME RICK we can do better than this...)\n"
	"mtsrd  0,0\n"
	"mtsrd  1,0\n"
	"mtsrd  2,0\n"
	"mtsrd  3,0\n"
	"mtsrd  4,0\n"
	"mtsrd  5,0\n"
	"mtsrd  6,0\n"
	"mtsrd  7,0\n"
	"mtsrd  8,0\n"
	"mtsrd  9,0\n"
	"mtsrd  10,0\n"
	"mtsrd  11,0\n"
	"mtsrd  12,0\n"
	"mtsrd  13,0\n"
	"mtsrd  14,0\n"
	"mtsrd  15,0\n"

	"       mtsrr0 %2		# set val for what PC will be after rfi\n"
	"mtsrr1 %3		# set val for what msr will be after rfi\n"
	"       mr     1,%4		# set pc with start of kernel\n"
	"       mr     2,%5		# move toc into r2 for kernel\n"
	"mr     3,%6		# parameter is bootInfoV\n"

	"#  .long  0x7C0007CE	# static breakpoint instr (commented out)\n"

	"rfid\n"
	:
	: "r" (asr), "r" (sdr1), "r" (entry), "r" (msrval),
	  "r" (stkPtr), "r" (toc), "r" (bootInfoV)
	: "r0", "r3", "memory");

    bprintf("error: should never get here\n");
}

static int
ParseXCoff(uval64 kernStartAddr, struct ParseInfo *pi)
{
    struct filehdr {
        unsigned short  f_magic;
        unsigned short  f_nscns;
        sval32          f_timdat;
        sval64       f_symptr;
        unsigned short  f_opthdr;
        unsigned short  f_flags;
        sval32          f_nsyms;
    };

    struct aouthdr {
        short magic;
        short vstamp;
        uval32 o_debugger;
        uval64 text_start;
        uval64 data_start;
        uval64 o_toc;
        short  o_snentry;
        short  o_sntext;
        short  o_sndata;
        short  o_sntoc;
        short  o_snloader;
        short  o_snbss;
        short  o_algntext;
        short  o_algndata;
        char   o_modtype[2];
        unsigned char o_cpuflag;
        unsigned char o_cputype;
        sval32 o_resv2[1];
        sval64 tsize;
        sval64 dsize;
        sval64 bsize;
        sval64 entry;
        uval64 o_maxstack;
        uval64 o_maxdata;
        sval32 o_resv3[4];
    };

    struct xcoffhdr {
        struct filehdr filehdr;
        struct aouthdr aouthdr;
    };
    struct scnhdr {
        char  s_name[8];
        uval64 s_paddr;
        uval64 s_vaddr;
        uval64 s_size;
        sval64 s_scnptr;
        sval64 s_relptr;
        sval64 s_lnnoptr;
        uval32 s_nreloc;
        uval32 s_nlnno;
        sval32 s_flags;
    };

    const int xcoff_magic = 0757;
    const int scnhdr_sz = sizeof (struct scnhdr);
    struct xcoffhdr *xcoffp;
    struct scnhdr *scnp;
    uval32 scnNum;

    xcoffp = (struct xcoffhdr *)(uval64)kernStartAddr;

    if (xcoff_magic != xcoffp->filehdr.f_magic) {
	return 0;
    }

    scnNum = xcoffp->aouthdr.o_sntext - 1;
    scnp = (struct scnhdr *)(((uval64) &(xcoffp[1])) + scnNum * scnhdr_sz);

    pi->text.vStart = scnp->s_vaddr;
    pi->text.size = scnp->s_size;
    pi->text.filePtr = (uval64) (kernStartAddr + scnp->s_scnptr);

    scnNum = xcoffp->aouthdr.o_sndata - 1;
    scnp = (struct scnhdr *) (((uval64) &(xcoffp[1])) + scnNum * scnhdr_sz);
    pi->data.vStart = scnp->s_vaddr;
    pi->data.size = scnp->s_size;
    pi->data.filePtr = (uval64) (kernStartAddr + scnp->s_scnptr);

    scnNum = xcoffp->aouthdr.o_snbss - 1;
    scnp = (struct scnhdr *) (((uval64) &(xcoffp[1])) + scnNum * scnhdr_sz);
    pi->bss.vStart = scnp->s_vaddr;
    pi->bss.size = scnp->s_size;

    pi->entryFunc = (uval64 *) (uval64)(pi->data.filePtr +
			    (xcoffp->aouthdr.entry - pi->data.vStart));
    return 1;
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
	PT_LOAD = 1
    };
    struct elf64_hdr *e64 = (struct elf64_hdr *)(uval64)kernStartAddr;
    struct elf64_phdr *phdr;
    int i;

    b1printf("i0",(uval64)e64->e_ident[0]);
    b1printf("i1",(uval64)e64->e_ident[1]);
    b1printf("i2",(uval64)e64->e_ident[2]);
    b1printf("i3",(uval64)e64->e_ident[3]);
    b1printf("mach",(uval64)e64->e_machine);

    if ((int)e64->e_ident[0] != 0x7f
	|| e64->e_ident[1] != 'E'
	|| e64->e_ident[2] != 'L'
	|| e64->e_ident[3] != 'F' ) {
	//	|| e64->e_machine != 21 /* EM_PPC64 */) {
	return (0);
    }
    phdr = (struct elf64_phdr *)(uval64)((uval)e64 + e64->e_phoff);

    // We know that the order is text, data that includes bss
    if (e64->e_phnum > 2) {
	b1printf("ERROR: to many ELF segments",
		 (uval64) e64->e_phnum);
	return 0;
    }

    /* FIXME: This is cheeesy */
    for (i = 0; i < e64->e_phnum; i++) {
	if (phdr[i].p_type == PT_LOAD) {
	    if (phdr[i].p_flags == (PF_R | PF_X) ||
		phdr[i].p_flags == PF_ALL) {
		pi->text.filePtr  = kernStartAddr + phdr[i].p_offset;
		pi->text.size = phdr[i].p_filesz;
		pi->text.vStart  = phdr[i].p_vaddr;
		if (phdr[i].p_memsz != pi->text.size) {
		    bprintf("ERROR: text memsz and text filesz are not equal");
		    return 0;
		}
	    } else if (phdr[i].p_flags == (PF_R | PF_W)) {
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
		} else {
		    bprintf("ERROR: entry descriptor is not in data segment");
		    return 0;
		}
	    }
	}
    }
    return 1;
}

void
_boot(long kernStartAddr)
{

#ifdef __GNU_AS__
    extern long _k42_boot_image[];
    kernStartAddr = (long)_k42_boot_image;

#endif

    b1printf("Kernel image location: ", kernStartAddr);
    parseLoadKernelAndRun(kernStartAddr);
}
