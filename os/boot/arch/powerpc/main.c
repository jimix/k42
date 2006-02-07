/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.c,v 1.107 2005/06/06 19:08:19 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 *
 *  Boot K42 on 64-bit PowerPC hardware
 *
 *****************************************************************************/

#include <sys/types.H>

/* reach into the kernel to get the BootInfo structure */
#include "../../../kernel/bilge/arch/powerpc/BootInfo.H"

#include "ofw_pci.h"
#include "of_softros.h"
#include "mmu.h"
#include "lmb.h"
#include "zlib.h"
/* reach into the kernel to get the memoryMap defines */
#include "../../../kernel/sys/memoryMap.H"
#include "../../../kernel/sys/hcall.h"


#define LOG_PGSIZE 12
#define PGSIZE (1<<LOG_PGSIZE)

#define BOOT_STACK_SIZE (20 * PGSIZE)

#define MSR_SF_BIT (1ULL << 63)
#define MSR_ISF_BIT (1ULL << 61)
#define MSR_HV_BIT (1ULL << 60)
#define MSR_ME_BIT (1ULL << 12)
#define MSR_IR_BIT (1ULL << 5)
#define MSR_DR_BIT (1ULL << 4)

/*
 * Space reserved for saving a copy of the boot image to facilitate a
 * fast reboot.
 */
#define REBOOT_SPACE (32 * (1024*1024))

/*
 * Space reserved for saving a copy of the OpenFirmware device tree.
 */
#define DEV_TREE_SPACE (1024 * 1024)

/*
 * The physical address at which the kernel is to be loaded.
 */
#define LOAD_ADDRESS 0x2000000

/*
 * Space reserved for the kernel image (text, data, bss) plus extra that the
 * kernel can use for early dynamic allocation.
 */
#define KERNEL_SPACE (48 * (1024*1024))


char bootData[BOOT_DATA_MAX] __attribute__ ((section("k42_boot_data")));

int main(void *vpd, int res, void *OFentry, char *arg, int argl);
extern char *sccc;
void _start(void *vpd, int res, void *OFentry, char *arg, int argl)
{
    // This function fixes of the boot-program, as it myab be loaded
    // at a location different from where we linked at and bss is not
    // cleared yet.  Then we call main().


    // We need the return address , because we know it differs from
    // __start_link_register by our load offset.
    // Seem comment in first.S
    extern int __start_link_register[];

    uval32 reloc = (uval32)__builtin_return_address(0)
	- (uval32)__start_link_register;

    // This forces a got reference, thus forcing .got2 + 32768 into r30
    uval32* got = (uval32*)&sccc;

    // We now pick off r30 to identify where to do relocations.
    // Using "got" as an input prevent reordering with above stmt.
    __asm__(/*".long 0x7C0007CE\n\t"/* uncomment to breakpoint on mambo*/
	    "mr	%0,30\n\t"
	    :"=r"(got):"0"(got));
    got -= 32768/sizeof(uval32);

    // Walk through the got and update all the entries
    // FIXME:  Best way so far I've found to detect end of got is to look
    //         for 0 entries.
    while (*got) {
	*got += reloc;
	++got;
    }

    // clear bss
    {
	extern int _end[];
	extern int __bss_start[];
	int *x = &__bss_start[0];
	while (x<_end) {
	    *x=0;
	    ++x;
	};
    }


    main(vpd, res, OFentry, arg, argl);


}




int stdout_ihandle;

unsigned long long logNumPTEs;	/* log base 2 of page table size */
unsigned long long pageTableSize;
unsigned long long pageTableAddr;
unsigned long long segmentTableAddr;
unsigned long long segmentTableSize;

struct KA {
    uval64 asr;			// offset 0
    uval64 sdr1;		// offset 8
    uval64 msr;			// offset 16
    uval64 iar;			// offset 24
    uval64 toc;			// offset 32
    uval64 stack;		// offset 40
    uval64 bootInfo;		// offset 48
    uval64 vsid0;		// offset 56
    uval64 esid0;		// offset 64
};


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

void __mapPage(uval64 vaddr, uval64 paddr, uval64 wimg);

unsigned int find_of(unsigned int *);

unsigned int find_kernel();

uval32 instantiate_rtas(uval32 memBottom, struct BootInfo *);
void rtas_timebase_call(int, int);
void rtas_pci_call(int, int, int, int, int, int, int, int);
int event_scan_call(int rtas_base, int token,
		    int event_mask, int critical,
		    int buffer, int len);
int ibm_scan_log_dump_call(int, int, int, int);

void start_processors(struct BootInfo *, int);

void relocate(unsigned int, unsigned int, unsigned int);

void launch(struct KA *);
int resetReal(struct KA *, void (*ptr)(struct KA*),
	      unsigned long long *msr, void* serport);


void mapRangeSim(uval64 vaddr, uval64 paddr, uval64 size);
void mapRange(uval64 vaddr, uval64 paddr, uval64 size, const char* tag);

void dumpSegmentTable (uval64 ptr);
void dumpPageTable    (uval64 ptr, int size);

void linux_prom_init(unsigned long r3, unsigned long r4, unsigned long pp,
		     unsigned long r6, unsigned long r7);
void linux_naca_init(struct BootInfo *bootInfo);
void prom_initialize_naca();
unsigned int of_map_range(void *phys, void *virt, int size);
void gunzip(void *dst, int dstlen, unsigned char *src, int *lenp);





unsigned long linuxSaveOFData(unsigned long mem_start, unsigned long mem_end);
#define RTAS_BUFFER_SIZE 1024
char rtas_buffer[RTAS_BUFFER_SIZE];
/*----------------------------------------------------------------------------*/
int onSim; /* 0=none, 1=SimOS-PPC, 2=Mambo */
int onHV = 0;


uval _get_PVR() {
    uval pvr;
    asm volatile ("mfpvr %0" : "=&r" (pvr));
    return pvr;
}




extern unsigned int
__of_map_range(void *phys, void *virt, int size, int mode);

static inline void eieio(void)
{
	__asm__ __volatile__ ("eieio" : : : "memory");
}

static unsigned char scc_inittab[] = {
    13, 0,		/* set baud rate divisor */
    12, 0,
    14, 1,		/* baud rate gen enable, src=rtxc */
    11, 0x50,		/* clocks = br gen */
    5,  0xea,		/* tx 8 bits, assert DTR & RTS */
    4,  0x46,		/* x16 clock, 1 stop */
    3,  0xc1,		/* rx enable, 8 bits */
};

#define	SCC_TXRDY	4
#define SCC_RXRDY	1

char *sccc;
char *sccd;

void writeChar(char c)
{
    unsigned int i;
    *sccc = 9; eieio();
    *sccc = 0xc0;
    for (i = 0; i < sizeof(scc_inittab); ++i) {
	eieio();
	*sccc = scc_inittab[i];
    }
    eieio();
    while ((*sccc & SCC_TXRDY) == 0)
	eieio();
    *sccd = c;
    eieio();

}

void writeChar3(char c, char* addr)
{
    do {
	eieio();
    } while ((*addr & SCC_TXRDY) == 0);
    eieio();
    *(addr+0x10) = c;
    eieio();
}

void writeChar2(char c)
{
    while ((*sccc & SCC_TXRDY) == 0)
	eieio();
    *sccd = c;
    eieio();
}

void
probeSCC(struct BootInfo *bootInfo)
{
    bootInfo->naca.serialPortAddr = 0x80013020;
    char* page = (char*)(int)(0xfffff000 & bootInfo->naca.serialPortAddr);
    int ret = __of_map_range(page, page, PGSIZE, 0x6a);
    char c = *sccc;
    sccc = (char*)(int)(0xffffffff & bootInfo->naca.serialPortAddr);
    sccd = sccc + 0x10;
}


/* Here is code to track how we're setting up memory usage */

struct mem_use{
    uval64 virtStart;
    uval32 physStart;
    uval32 size;
    const char* tag;
    struct mem_use *next;
};

struct mem_use used[20] = {{0,},};
struct mem_use * start = (struct mem_use*)0;
uval32 next = 0;
struct mem_use **last = &start;
void useMem(uval64 v, uval32 p, uval32 s, const char* t) {
    struct mem_use **curr = &start;
    s = s + (v & (PAGE_SIZE-1));
    v = (v + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    p = (p + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    s = (s + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);

    used[next].virtStart = v;
    used[next].physStart = p;
    used[next].size = s;
    used[next].tag = t;
    while (*curr) {
	if ((*curr)->physStart > p) {
	    break;
	}
	curr = &((*curr)->next);
    }
    if (*curr) {
	used[next].next = *curr;
    }
    *curr = &used[next];
    ++next;
}

/* Record memory usage here and in the LMB as well */
/* LMB is passed to kernel to describe memory usage */
void useMemLMB(uval64 v, uval64 p, uval64 s, const char* t) {
    s = s + (v & (PAGE_SIZE-1));
    v = (v + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    p = (p + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    s = (s + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    lmb_reserve(p, s);
    useMem(v, p, s, t);
}

void memUseDump() {
    struct mem_use **curr = &start;
    printf("     From       To           Mapping Description\n\r");
    while (*curr) {
	if ((*curr)->virtStart) {
	    printf("[%08lx %08lx] %016llx %s\n\r",
		   (*curr)->physStart, (*curr)->physStart + (*curr)->size,
		   (*curr)->virtStart, (*curr)->tag);
	} else {
	    printf("[%08lx %08lx]         unmapped %s\n\r",
		   (*curr)->physStart, (*curr)->physStart + (*curr)->size,
		   (*curr)->tag);
	}
	curr = &((*curr)->next);
    }
}

#define REAL_MODE_MAMBO_CALL 0
#define VIRT_MODE_MAMBO_CALL 1
#define SimNumbPhysCpusK     0
#define SimMemorySizeK       1
#define MAMBO_CONS_FUN	     0x00	/* Console output		*/
#define SimExitCode          31         /* 31 */
#define SimGetMachAttrK      68         /* 68 */ /* get a machine attribute */
#define SimPhysMemCopyK      69         /* a fast phys-phys mem copy */
#define SimPhysMemSetK       71         /* 71 */ /* a fast set memory */
#define RTAS_SIZE 0x28000
#define MSR_SIM		0x0000000020000000
#define MSR_MAMBO       0x0000000010000000

/* CAUTION: use this with only ALL 32 bit args for registers */
long
SimCutThrough(unsigned int foo, ...)
{
    long ret;
    if (onSim == SIM_MAMBO) {
	asm(".long 0x000eaeb0; mr %0,3" : "=&r" (ret) : /* no input */ : "r3");
    } else { /*SIMOSPPC*/
	asm(".long 0x7c0007cc; mr %0,3" : "=&r" (ret) : /* no input */ : "r3");
    }
    return ret;
}


/* CAUTION: use this with only ALL 64 bit args for max of 3 registers */
long
SimCutThrough64(unsigned int foo, ...)
{
    long ret;
    asm("mr 4,6; rldimi 4,5,32,0");
    asm("mr 5,8; rldimi 5,7,32,0");
    asm("mr 6,10; rldimi 6,9,32,0");
    if (onSim == SIM_MAMBO) {
	asm(".long 0x000eaeb0; mr %0,3" : "=&r" (ret) : /* no input */ : "r3");
    } else { /*SIMOSPPC*/
	asm(".long 0x7c0007cc; mr %0,3" : "=&r" (ret) : /* no input */ : "r3");
    }
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

void strCpy(char *s, const char *t)
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
mprintf(const char *buf)
{
    int len;
    char lbuf[256];

    strCpy(lbuf,buf);
    len = strLen(lbuf);
    SimCutThrough(MAMBO_CONS_FUN, lbuf, len, REAL_MODE_MAMBO_CALL);
}

void
bprintf(char *buf)
{
    char lbuf[256];
    int len;

    strCpy(lbuf,buf);
    len = strLen(lbuf);
    lbuf[len] = '\n';
    lbuf[len+1] = '\r';
    lbuf[len+2] = 0;

    printf(lbuf);
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

void
b2printf(char *buf, uval64 val, uval64 val2)
{
    char *bufp;
    char lbuf[256];

    strCpy(lbuf,buf);

    bufp = lbuf + strLen(lbuf);
    bufp = printHex (bufp, val);
    *(bufp ++) = ' ';
    bufp = printHex (bufp, val2);
    bprintf (lbuf);
}

void
bootExit()
{
    int code;

    if (onSim && !onHV) {
	code = 17;
	SimCutThrough(SimExitCode, code);
    } else {
	of_exit();
    }
}

int
of_onHV(void)
{
    int phandle;
    char buf[16];
    int rc;

    phandle = of_finddevice("/rtas");

    if (phandle==-1) return 0;

    /* all we want to know is if this property is present and if its
     * size is greater than 0 */
    rc = of_getprop(phandle, "ibm,hypertas-functions", buf, 16);

    return (rc > 0);
}



main(void *vpd, int res, void *OFentry, char *arg, int argl)
{
    void  *rtas_desc[3];
    uval32 kernbase, kernsize, ofStart, ofEnd, ofSize;
    struct KA kernelArgs;
    struct xcoffhdr *xcoffp;
    struct scnhdr *scnp;
    uval64 scnNum;
    uval64 kernEnd;
    uval32 stkEnd;
    uval64 vMapsRDelta, numPages;
    struct BootInfo *bootInfo;
    uval64 *addr;
    uval32 kernend, *src, *dst;
    uval64 memSize;
    uval32 assumedMemSize;
    uval32 rebootImage;
    uval32 hwDataAreaAddr;
    uval32 hwDataAreaSize;
    sval32 rc;
    uval32 index, offset, length, i;
    uval32 memBottom;
    struct ParseInfo pi;
    char pstr[256];
    uval64 msr,sdr1;
    uval32 msr_low, msr_high;
    uval32 sdr1_low, sdr1_high;


    unsigned long long refmsr;

    extern void getMSR(unsigned long long* msr);
    extern void setMSR(unsigned long long* msr);

    getMSR(&refmsr);
    if (refmsr & MSR_SIM) {
	onSim = (refmsr & MSR_MAMBO) ? SIM_MAMBO : SIM_SIMOSPPC;
    } else {
	onSim = SIM_NONE;
    }

    cl_in_handler = (int (*)())OFentry;
    of_entry = (int (*)(struct service_request*))OFentry;


    if (OFentry) {
	extern struct prom_t prom;

	linux_prom_init((uval32)vpd, (uval32)res, (uval32)OFentry,
			(uval32)arg, (uval32)argl);

	stdout_ihandle = prom.stdout;

	printf("K42 CHRP Booter\n");
	ofSize = find_of(&ofStart);
	ofEnd = ofStart + ofSize;
	printf("OF start: %lx\n", ofStart);
	printf("OF size: %lx\n", ofSize);

	onHV = of_onHV();
    }

    printf("Boot args: %lx %lx %lx %lx %lx\n\r",
	   vpd, res, OFentry, arg, argl);

    strcpy(pstr, "Built on ");
    strcat(pstr, __DATE__);
    strcat(pstr, " at ");
    strcat(pstr, __TIME__);
    bprintf(pstr);

    if (onSim && !onHV) {
	/* on a simulator */
	if (onSim == SIM_MAMBO) {
	    uval tmp = SimCutThrough(42); /* return sim type */
	    b1printf("K42 booting on Mambo version ",tmp>>8);
	} else {
	    bprintf("K42 booting on SimOS-PPC");
	}
	memSize = SimCutThrough(SimGetMachAttrK, SimMemorySizeK);
	b1printf("memsize reported by sim ", memSize);
    } else {
	if (onHV) {
	    b1printf("ibm,hypertas-functions ",onHV);
	} else {
	    bprintf("\n\n\rK42 booting on hardware");
	}
	bprintf("K42 CHRP Booter");
	memSize = 0x08000000;  /* 128MB, until memory discovery is done  */
    }

    {
	extern unsigned int _k42_boot_image[];
	extern unsigned int _k42_boot_image_end[];
	kernbase = (unsigned int)_k42_boot_image;
	kernsize = ((unsigned int) _k42_boot_image_end)-kernbase;
    }



    printf("Kernel image location start: %lx - %lx\n\r",
	   (unsigned int)kernbase, kernbase + kernsize);


    /* On Hardware
     * We haven't done memory discovery yet, but we need to copy the kernel
     * image and the OpenFirmware device tree somewhere safe to enable fast
     * reboot.  We simply assume the machine has at least 128MB of contiguous
     * physical memory, starting at 0, and we preserve the kernel image and
     * device tree at the end of it.
     * On simulator we already obtained the memory size use it
     */

    assumedMemSize = memSize;

    rebootImage = assumedMemSize - REBOOT_SPACE;
    hwDataAreaAddr = rebootImage - DEV_TREE_SPACE;

    if (!onSim || onHV) {
	//Create mappings, if we're in virt-mode
	of_map_range((void*)hwDataAreaAddr, (void*)hwDataAreaAddr,
		     DEV_TREE_SPACE);
	of_map_range((void*)rebootImage, (void*)rebootImage, REBOOT_SPACE);
    }


    kernend = kernbase + kernsize;

    // Test for compression, linux does it this way
    if (*(unsigned short*)kernbase == 0x1f8b) {
	unsigned int size = kernsize;
	gunzip((void*)rebootImage, REBOOT_SPACE, (void*)kernbase, &kernsize);
	printf("gunzip 0x%lx[%ld] -> 0x%lx[%ld]\n\r",
	       kernbase, size, rebootImage, kernsize);
	kernbase = rebootImage;
    } else if (!onSim && !onHV) {
	if (kernsize > REBOOT_SPACE) {
	    printf("Too little space reserved for reboot image.\n\r");
	    printf("Exiting...\n\r");
	    of_exit();
	}
	printf("Reboot image: %lx\n",rebootImage);

	b1printf("Copying kernel image for fast reboot ...", rebootImage);
	for (src = (uval32 *) kernbase, dst = (uval32 *) rebootImage;
	     src < (uval32 *) kernend;
	     src++, dst++) {
	    *dst = *src;
	}
	bprintf("done.");
	kernbase = rebootImage;
    }
    if (!onSim && !onHV) {
	bprintf("Clearing remainder of reboot area and HW data area...");
	memset((void*)rebootImage + kernsize, 0,
	       memSize - (rebootImage + kernsize));
	bprintf("done.");
    }

    if (!ParseElf(kernbase, &pi)) {
	bprintf("error: image is not a 64-bit ELF.");
	while (1) {};
    }

    vMapsRDelta = pi.text.vStart - LOAD_ADDRESS;

    pi.text.pStart = pi.text.vStart - vMapsRDelta;
    pi.data.pStart = pi.data.vStart - vMapsRDelta;
    pi.bss.pStart = pi.bss.vStart - vMapsRDelta;

    b1printf("Text: vaddr ", pi.text.vStart);
    b1printf("      paddr ", pi.text.pStart);
    b1printf("      size  ", pi.text.size);
    b1printf("      offset", pi.text.filePtr);

    b1printf("Data: vaddr ", pi.data.vStart);
    b1printf("      paddr ", pi.data.pStart);
    b1printf("      size  ", pi.data.size);
    b1printf("      offset", pi.text.filePtr);

    b1printf("BSS:  vaddr ", pi.bss.vStart);
    b1printf("      paddr ", pi.bss.pStart);
    b1printf("      size  ", pi.bss.size);

    if ((pi.data.vStart + pi.data.size) != pi.bss.vStart) {
	bprintf("Data and BSS segments are not contiguous.");
	bprintf("Exiting...");
	bootExit();
    }

    if ((pi.text.vStart + pi.text.size) > pi.data.vStart) {
	bprintf("Text and data segments overlap.");
	bprintf("Exiting...");
	bootExit();
    }

    if ((pi.bss.pStart + pi.bss.size) > ofStart && pi.text.pStart < ofStart) {
	bprintf("BSS segment overlaps OpenFirmware.");
	/*
	bprintf("Exiting...");
	bootExit();
	*/
	bprintf("Continuing with trepidation....");
    }


    kernend = pi.data.filePtr + pi.data.size;
    kernsize = ((kernend - kernbase) + (PGSIZE - 1)) &  ~(PGSIZE - 1);

    /*
     * Copy text and data (data first because ranges may overlap).
     */

    bprintf("\nCopying data:");
    b1printf(" vaddr ", pi.data.vStart);
    b1printf(" paddr ", pi.data.pStart);
    b1printf(" size ", pi.data.size);
    b1printf(" file ptr ", pi.data.filePtr);

    if (onSim && !onHV) {
	SimCutThrough64(SimPhysMemCopyK, pi.data.pStart,
			pi.data.filePtr, pi.data.size);
    } else {
	//Create mappings, if we're in virt-mode
	of_map_range((void*)(long)pi.data.pStart,
		     (void*)(long)pi.data.pStart,
		     pi.data.size + pi.bss.size);

	relocate(pi.data.pStart + pi.data.size, pi.data.filePtr + pi.data.size,
		 pi.data.size / 4);
    }
    bprintf("done.");

    bprintf("\nCopying text:");
    b1printf(" vaddr ", pi.text.vStart);
    b1printf(" paddr ", pi.text.pStart);
    b1printf(" size ", pi.text.size);
    b1printf(" file ptr ", pi.text.filePtr);

    if (onSim && !onHV) {
	SimCutThrough64(SimPhysMemCopyK, pi.text.pStart,
			  pi.text.filePtr, pi.text.size);
    } else {
	//Create mappings, if we're in virt-mode
	of_map_range((void*)(long)pi.text.pStart,
		     (void*)(long)pi.text.pStart,
		     pi.text.size);

	relocate(pi.text.pStart + pi.text.size, pi.text.filePtr + pi.text.size,
		 pi.text.size / 4);
    }
    bprintf("done.");


    memBottom = (uval32) pi.text.pStart;
    /* Place bootInfo in the pages below kernel text. */
    memBottom = (memBottom - sizeof(struct BootInfo)) & ~(PGSIZE - 1);
    bootInfo = (struct BootInfo *) memBottom;

    //Create mappings, if we're in virt-mode
    if (!onSim || onHV) {
	of_map_range((void*)(long)bootInfo, (void*)(long)bootInfo,
		     (int)((long)pi.text.pStart - (long)bootInfo));
    }

    memset(bootInfo, 0, sizeof(struct BootInfo));

    bprintf("Copying boot parameter data into bootInfo");
    /* Copy boot parameter data into bootInfo */
    for (index = 0; index < BOOT_DATA_MAX; index++) {
	bootInfo->boot_data[index] = bootData[index];
    }
    bprintf("Finished copying boot parameter data into bootInfo");

    bootInfo->onHV  = onHV;
    bootInfo->wireInit = 0;

    /* Get model name */
    {
	int phandle;
	phandle = of_finddevice("/");
	of_getprop(phandle, "model", bootInfo->modelName, 15);
    }

    linux_naca_init(bootInfo);
    if (onHV) bootInfo->platform = PLATFORM_PSERIES_LPAR;
    prom_initialize_naca();

    /* Remember chunks of memory we're using */
    if (!onSim && !onHV) {
	useMemLMB(0ULL, rebootImage, REBOOT_SPACE, "reboot space");
    }
    useMemLMB(0ULL, hwDataAreaAddr, DEV_TREE_SPACE, "OF dev tree");

    bootInfo->onSim = onSim;
    bootInfo->physEnd = lmb_end_of_DRAM();
    bootInfo->kernelImage = LOAD_ADDRESS;
    bootInfo->kernelImageSize = KERNEL_SPACE;
    bootInfo->realOffset = 0;

    //Get k42-specific rtas info
    memBottom = instantiate_rtas(memBottom, bootInfo);
    rtas_in_handler = (int (*)())(uval32)bootInfo->rtas.entry;

    if ((!onSim || onHV) &&
	(bootInfo->platform & PLATFORM_PSERIES) &&
	bootInfo->ibm_scan_log_dump != -1) {
	/* Save last RTAS error log entry. */
	printf("Saving RTAS dump scan log.\n\r");
	index = 0;
	*(int*)(rtas_buffer) = 0;
	for (;;) {
	    rc = ibm_scan_log_dump_call((int) bootInfo->rtas.base,
					bootInfo->ibm_scan_log_dump,
					(int) rtas_buffer,
					RTAS_BUFFER_SIZE);
	    length = *((uval32 *)(rtas_buffer + 4));
	    offset = *((uval32 *)(rtas_buffer + 8));
	    if (rc != 1) break;
	    if (index == 0) {
		printf("Last Checkstop: %s\n\r", rtas_buffer+offset+0x20);
	    }
	    if (index >= RTAS_INFO_BUF_MAX) {
		index += length;
		continue;
	    }
	    if ((index + length) > RTAS_INFO_BUF_MAX) {
		length = RTAS_INFO_BUF_MAX - index;
	    }
	    for (i = 0; i < length; i++) {
		bootInfo->rtas_info_buf[index+i] = rtas_buffer[offset+i];
	    }
	    index += length;
	    if (index >= RTAS_INFO_BUF_MAX) break;
	}
    }

    memBottom = (memBottom-BOOT_STACK_SIZE) & ~(PGSIZE-1);
    stkEnd = memBottom;

    printf("stack end: %lx\n\r", stkEnd);
    printf("bootInfo : %lx\n\r", bootInfo);
    printf("memBottom: %lx\n\r", memBottom);
    bootInfo->controlFlags = 0;

    /*
     * Tell the OS how many processors to use.  This value may be provided
     * on the command line, in which case we use it.  Otherwise use 1.
     * On the simulator, use a big number (-1ULL) so that the OS will use all
     * available processors.
     */
#ifndef OS_PROCESSOR_COUNT
#define OS_PROCESSOR_COUNT 1
#endif

    if (onHV) {
	bootInfo->processorCount = 1ULL;
    } else if (onSim) {
	bootInfo->processorCount = -1ULL;
    } else {
        bootInfo->processorCount = OS_PROCESSOR_COUNT;
    }

    #undef HACK_FOR_PROCESSOR_COUNT
    #ifdef HACK_FOR_PROCESSOR_COUNT
    /*
     * Sometimes we just want to force the system to start with
     * so many processors and the heck with the actual counts
     */
    bootInfo->processorCount = 1;
    #endif // HACK_FOR_PROCESSOR_COUNT

    /*
     * We need a page table large enough to map in memory from 0 to
     * the end of BSS, and we need one page for the hardware Segment Table.
     */
    kernEnd = pi.text.pStart + KERNEL_SPACE;
    numPages = (kernEnd + (PGSIZE-1)) / PGSIZE;

    if (onHV) {
	asm volatile ("mfsdr1  %0   \n"
		      "mr %1, %0   \n"
		      "srdi %0, %0, 32 \n" : "=&r"(sdr1_high),"=&r"(sdr1_low));
	sdr1 = ((uval64)sdr1_high << 32) | (uval64)sdr1_low;

	logNumPTEs = bootInfo->naca.pftSize - LOG_PTE_SIZE;
	b1printf("sdr1 ",sdr1);
	b1printf("logNumPTEs ",logNumPTEs);
    } else {
	logNumPTEs = LOG_NUM_PTES_MIN;
	while ((1ULL << logNumPTEs) < numPages) {
	    logNumPTEs++;
	}
    }

    pageTableSize = 1ULL << (logNumPTEs + LOG_PTE_SIZE);
    segmentTableSize = PGSIZE;

  if (!onHV) {
    /* align up to pageTableSize boundary */
    pageTableAddr = (kernEnd + (pageTableSize - 1)) & ~(pageTableSize - 1);

    b1printf("Placing initial page table at ", pageTableAddr);

    if (!onSim || onHV) {  // -JX!
	//Create mappings, if we're in virt-mode
	of_map_range((void*)(long)pageTableAddr, (void*)(long)pageTableAddr,
		     pageTableSize);
	useMemLMB(0ULL, pageTableAddr, pageTableSize, "page table");
    }

    if (!onHV) {  // -JX!
	bprintf("Clearing page table.");
	if (onSim) {
	    SimCutThrough64(SimPhysMemSetK, pageTableAddr,
			    (uval64)(0), pageTableSize);
	} else {
	    for (addr = (uval64 *) (uval32)pageTableAddr;
		 addr < ((uval64 *) (uval32)(pageTableAddr + pageTableSize));
		 addr++) {
		*addr = 0;
	    }
	}
    }

    /*
     * Put the segment table in the page following the page table
     */
    segmentTableAddr = pageTableAddr + pageTableSize;
    b1printf("Placing initial segment table at", segmentTableAddr);

    if (!onSim || onHV) {  //-JX
	of_map_range((void*)(long)segmentTableAddr,
		     (void*)(long)segmentTableAddr, PGSIZE);
	useMemLMB(0ULL, segmentTableAddr, PGSIZE, "segment table");
    }

    /* zero the page containing the nascent segment table */
    for (addr = (uval64 *) (uval32)segmentTableAddr;
	 addr < ((uval64 *) (uval32)(segmentTableAddr + PGSIZE));
	 addr++) {
	*addr = 0;
    }

    if (onSim || onHV) {
	if ((segmentTableAddr + PGSIZE) > memSize) {
	    bprintf("Page and segment tables extend beyond the end of memory\n");
	    b1printf("segmentTableAddr ", segmentTableAddr);
	    b1printf("memSize          ", memSize);
	    while (1) ;
	}
    } else {
	if (segmentTableAddr + PGSIZE > rebootImage) {
	    bprintf("*** WARNING ***");
	    bprintf("Page and segment tables extend into rebootImage area.");
	    bprintf("Fast reboot will almost certainly fail.");
	}
    }
  }

    /* Assume kernel fits within kernVirtStart ... kernVirtStart + kernSize*/
    if (pi.bss.pStart + pi.bss.size > pi.text.pStart + KERNEL_SPACE) {
	bprintf("Kernel image is too big. Change KERN_SIZE in BootInfo.H");
	/* We die now */
	bootExit();
    }


    bprintf("Mapping kernel image (text, data, bss).");
    mapRange(pi.text.vStart, pi.text.pStart, KERNEL_SPACE, "kernel image");

    bprintf("Mapping exception vector area.");
    mapRange(0 + vMapsRDelta, 0, 3*PGSIZE, "exception vector");

    bprintf("Mapping bootInfo.");
    mapRange((uval64)(((uval32)bootInfo) + (uval64)vMapsRDelta),
	     ((uval32) bootInfo),
	     (uval64)(pi.text.pStart - (uval32)bootInfo), "bootInfo");

    bprintf("Mapping boot stack");
    mapRange(stkEnd + vMapsRDelta, stkEnd, BOOT_STACK_SIZE, "boot stack");

    bprintf("Saving data to hwDataArea...");
    hwDataAreaSize = linuxSaveOFData(hwDataAreaAddr, hwDataAreaAddr
				     + DEV_TREE_SPACE) - hwDataAreaAddr;
    b1printf("done. bytes ", hwDataAreaSize);

    if (hwDataAreaSize > DEV_TREE_SPACE) {
	bprintf("Device tree is too big.");
	bprintf("Change DEV_TREE_SPACE in os/boot/arch/powerpc/main.c.");
	bprintf("Exiting...");
	bootExit();
    }

    b1printf("Placing hwDataArea at", hwDataAreaAddr);
    bootInfo->hwData = hwDataAreaAddr;
    bootInfo->hwDataSize = hwDataAreaSize;

    memcpy((void*)&bootInfo->lmb, (void*)&lmb, sizeof(struct lmb));

    if (!onSim && !onHV) {
	printf("Creating serial port mappings: %llx\n\r",
	       bootInfo->naca.serialPortAddr | 0xffff000000000000ULL);
	if (bootInfo->platform==PLATFORM_POWERMAC) {
	    probeSCC(bootInfo);
	} else {
	    __mapPage(bootInfo->naca.serialPortAddr | 0xffff000000000000ULL,
		      bootInfo->naca.serialPortAddr, 0xd);
	}
    }


    printf("Setting up kernel args\n\r");
    bootInfo->onHV  = onHV;
    bootInfo->onSim  = onSim;

    bootInfo->cpu_version = _get_PVR() >> 16;
    if (!bootInfo->cpu_version) {
	bootInfo->cpu_version = 0x32; /* Apache in Raven S/70 */
    }


    bootInfo->argString = (uval64) (uval32)arg;
    bootInfo->argLength = (uval64) (uval32)argl;
    bootInfo->rebootImage = rebootImage;
    bootInfo->rebootImageSize = REBOOT_SPACE;

    bootInfo->wireChanOffset = 0;
    bootInfo->simosDisk0Number = 0;
    bootInfo->simosDisk1Number = 1;

    start_processors(bootInfo, (int) bootInfo->rtas.base);


    /* Don't use lmb services or mapRange beyond here!!!!! */
    kernelArgs.asr  = segmentTableAddr | 1; /* low bit of ASR is its "V" bit */

    if (!onHV) {
	kernelArgs.sdr1 = pageTableAddr | (logNumPTEs - LOG_NUM_PTES_MIN);
    } else {
	kernelArgs.sdr1 = 0;
    }

    kernelArgs.msr = MSR_ISF_BIT | MSR_SF_BIT | MSR_ME_BIT |
		     MSR_IR_BIT | MSR_DR_BIT;

    kernelArgs.iar = pi.entryFunc[0];
    kernelArgs.toc = pi.entryFunc[1];

    kernelArgs.stack = ((stkEnd + BOOT_STACK_SIZE) - 0x100) + vMapsRDelta;

    kernelArgs.bootInfo = ((uval64) (uval32)bootInfo) + vMapsRDelta;

    bprintf("BSS will be cleared after closing OpenFirmware.");

    if (!onHV) dumpSegmentTable (segmentTableAddr);
    memUseDump();

    #ifdef HACK_FOR_PROCESSOR_COUNT
    bootInfo->availCPUs = 1;
    #endif // HACK_FOR_PROCESSOR_COUNT

    bprintf("\nAbout to jump to K42...");
    b1printf("    asr:              ", kernelArgs.asr);
    b1printf("    sdr1:             ", kernelArgs.sdr1);
    b1printf("    msr:              ", kernelArgs.msr);
    b1printf("    iar:              ", kernelArgs.iar);
    b1printf("    TOC address:      ", kernelArgs.toc);
    b1printf("    stack pointer:    ", kernelArgs.stack);
    b1printf("    bootInfo address: ", kernelArgs.bootInfo);
    b1printf("    rtas base:        ", bootInfo->rtas.base);
    b1printf("    rtas entry:       ", bootInfo->rtas.entry);
    b1printf("    availCPUs:        ", bootInfo->availCPUs);
    b1printf("    masterCPU:        ", bootInfo->masterCPU);
    b1printf("    serial port:      ", bootInfo->naca.serialPortAddr);
    b1printf("    CPU version:      ", bootInfo->cpu_version);
    b1printf("    onSim:            ", bootInfo->onSim);
    b1printf("    onHV:             ", bootInfo->onHV);
    b1printf("    platform:         ", bootInfo->platform);

    /*
     * Clear BSS to zeros
     */
    if (onSim && !onHV) {
	SimCutThrough64(SimPhysMemSetK, pi.bss.pStart,
			  (uval64)0, pi.bss.size);
    } else {
	for (addr = (uval64 *)(uval32) pi.bss.pStart;
	     addr < ((uval64 *)(uval32)(pi.bss.pStart + pi.bss.size));
	     addr++)
	{
	    *addr = 0;
	}
    }


//    of_close(stdout_ihandle);

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
	kernelArgs.esid0 = (pi.text.vStart & 0xFFFFFFFFF0000000ULL)
	    | 0x8000000;
	printf("SLB new 0th vsid %llx\n\r", kernelArgs.vsid0);
	printf("SLB new 0th esid %llx\n\r", kernelArgs.esid0);
    } else {
	kernelArgs.vsid0 = kernelArgs.esid0 = 0ULL;
    }

#if 0
    if (onSim && !onHV) {
	if (bootInfo->cpu_version != VER_GP
	    && bootInfo->cpu_version != VER_GQ
	    && bootInfo->cpu_version != VER_970
	    && bootInfo->cpu_version != VER_BE_PU) {
	    asm volatile ("\n"
			  "# an invalid SID value "
			  "(FIXME RICK we can do better than this...)\n"
			  "li     0,0x5A5A\n"
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
			  "mtsrd  15,0\n");
	}
    }
#endif

    if (refmsr & (MSR_DR_BIT | MSR_IR_BIT)) {
	printf("Jumping to real mode...\n\r");
	unsigned long long msr = MSR_HV_BIT|MSR_ME_BIT;
	extern unsigned int of_translate(void *virt);
	int x = resetReal(&kernelArgs, launch, &msr,
			  (void*)(int)bootInfo->naca.serialPortAddr);
	printf("Got x: %lx %llx\n",x, msr);
    } else {
	launch(&kernelArgs);
    }
    bprintf("error: should never get here\n");
    bootExit();
}

/*----------------------------------------------------------------------------*/
uval32
instantiate_rtas(uval32 memBottom, struct BootInfo *bootInfo)
{
    uval32 ihandle, phandle, rtas_base, rtas_entry, rtas_size;

    bootInfo->rtas.base = 0;
    bootInfo->rtas.size = 0;
    bootInfo->rtas.entry = 0;
    bootInfo->ibm_scan_log_dump = -1;

    phandle = of_finddevice("/rtas");
    if (phandle==~0) return memBottom;
    of_getprop(phandle, "rtas-size", &rtas_size, 4);

    /* base rtas on a page boundary below bootInfo */
    if (onHV) {
// FIXME MFM kludge till we detect RMO properly, use non-RMO appropriately
	rtas_base = (0x8000000 - rtas_size) & ~(PGSIZE - 1);
    } else {
	rtas_base = (memBottom - rtas_size) & ~(PGSIZE - 1);
    }
    memBottom = rtas_base;
    printf("BootInfo: %lx RTAS: %lx - %lx\n\r",
	   (int)bootInfo, rtas_base, rtas_base + rtas_size);

    useMemLMB(0ULL, rtas_base, rtas_size, "rtas");
    /* Commented out the following code to make the Model 270 boot,
     * seems to still work with the 260s and the S70, so leave this
     * change in: */
#if 0
    of_release(rtas_base, rtas_size);
    *rtas_base = of_claim(rtas_base, rtas_size, 0);
#endif /* #if 0 */

    /* base address input, entry point output */
    rtas_entry = rtas_base;
    ihandle = of_open("/rtas");
    of_call_method(1, 1, &rtas_entry, "instantiate-rtas", ihandle);

    of_close(ihandle);

    bootInfo->rtas.base = (uval64) rtas_base;
    bootInfo->rtas.entry = (uval64) rtas_entry;
    bootInfo->rtas.size = rtas_size;

#define GET_TOKEN(name, field) \
	if (of_getprop(phandle, name, &bootInfo->field, 4) != 4) { \
	    bootInfo->field = -1; \
	}

#if 0
    GET_TOKEN("get-time-of-day", get_time_of_day);
    GET_TOKEN("set-time-of-day", set_time_of_day);
    GET_TOKEN("display-character", display_character);
    GET_TOKEN("set-indicator", set_indicator);
    GET_TOKEN("power-off", power_off);
    GET_TOKEN("system-reboot", system_reboot);
    GET_TOKEN("read-pci-config", read_pci_config);
    GET_TOKEN("write-pci-config", write_pci_config);
    GET_TOKEN("event-scan", event_scan);
    GET_TOKEN("check-exception", check_exception);
    GET_TOKEN("rtas-last-error", rtas_last_error);
#endif
    GET_TOKEN("ibm,scan-log-dump", ibm_scan_log_dump);
    GET_TOKEN("freeze-time-base", freeze_time_base);
    GET_TOKEN("thaw-time-base", thaw_time_base);

#undef GET_TOKEN

    printf("RTAS: 0x%08x  0x%08x  0x%08x\n\r",
	   rtas_entry, rtas_base, rtas_size);
    return memBottom;
}

/*----------------------------------------------------------------------------*/
phandle of_find_node(phandle node, const char* prop,
		     const char* val, int val_size);

void
start_processors(struct BootInfo *bootInfo, int rtas_base)
{
    int phandle, child_phandle, master, cpunumber, spin_loop_size;
    char cpustatus[16];

    extern void spin_loop();

    extern char spin_loop_start, spin_loop_end;

    spin_loop_size = &spin_loop_end - &spin_loop_start;
    if (spin_loop_size > sizeof(bootInfo->spinLoopCode)) {
	printf("Error:  not enough space for spin loop in BootInfo\n\r");
	printf("Exiting...\n\r");
	of_exit();
    }
    if ((((int) (&bootInfo->spinLoopCode)) & 0x3) != 0) {
	printf("Error:  spin loop in BootInfo is not word-aligned\n\r");
	printf("Exiting...\n\r");
	of_exit();
    }

    relocate(((int) (&bootInfo->spinLoopCode)) + spin_loop_size,
	     ((int) (&spin_loop_start)) + spin_loop_size,
	     spin_loop_size / 4);

    static const char chosen[] = "chosen";
    phandle = of_find_node(0, "name", chosen, sizeof(chosen));
    of_getprop(phandle, "cpu", &master, 4);
    master = of_inst2pack(master);

    bootInfo->availCPUs = 0ULL;


    phandle = 0;
    while ((phandle = of_find_node(phandle, "device_type", "cpu", 4)) != 0) {
	of_getprop(phandle, "reg", &cpunumber, 4);
	of_getprop(phandle, "status", cpustatus, 16);

	printf("Processor #%02d%s status: %s\n\r", cpunumber,
	       (phandle == master ? " (master)" : ""), cpustatus);
	bootInfo->availCPUs |= 1ULL << cpunumber;
	bootInfo->startCPU[cpunumber].startIAR = 0ULL;
	/* Pass address of the bootInfo copy of the actual spin-loop code
	 * to spin_loop() in the second word of the startCPU structure.
	 */
	bootInfo->startCPU[cpunumber].startR3 =
				(uval64) (uval32)&bootInfo->spinLoopCode;
	sync();

	if (phandle != master) {
	    if (bootInfo->platform != PLATFORM_POWERMAC) {
		printf("Starting secondary cpu...\n");
		of_start_cpu(phandle, spin_loop,
			     (int) &bootInfo->startCPU[cpunumber]);
	    }
	} else {
	    bootInfo->masterCPU = cpunumber;
	    of_getprop(phandle, "cpu-version", &cpunumber, 4);
	    bootInfo->cpu_version = cpunumber >> 16;
	    of_getprop(phandle, "clock-frequency", &cpunumber, 4);
	    bootInfo->clock_frequency = cpunumber;
	    of_getprop(phandle, "bus-frequency", &cpunumber, 4);
	    bootInfo->bus_frequency = cpunumber;

	    of_getprop(phandle, "timebase-frequency", &cpunumber, 4);
	    bootInfo->timebase_frequency = cpunumber;

	    of_getprop(phandle, "d-cache-size", &cpunumber, 4);
	    bootInfo->dCacheL1Size = cpunumber;

	    if (bootInfo->platform == PLATFORM_POWERMAC) {
		of_getprop(phandle, "d-cache-block-size", &cpunumber, 4);
	    } else {
		of_getprop(phandle, "d-cache-line-size", &cpunumber, 4);
	    }
	    bootInfo->dCacheL1LineSize = cpunumber;

	    /* not all machines have an l2-cache property (like HV) */
	    child_phandle = -1;
	    of_getprop(phandle, "l2-cache", &child_phandle, 4);
	    if (child_phandle == -1) {
		if (bootInfo->platform==PLATFORM_POWERMAC) {
		    // G5's don't have an l2-cache node
		    bootInfo->L2cachesize = 512 * 1024;
		    bootInfo->L2linesize = 128;
		}
	    } else {
		of_getprop(child_phandle, "d-cache-size", &cpunumber, 4);
		bootInfo->L2cachesize = cpunumber;
		of_getprop(child_phandle, "d-cache-line-size", &cpunumber, 4);
		bootInfo->L2linesize = cpunumber;
	    }

	    printf("CPU Version: 0x%x  Clock: %d MHz  Bus: %d MHz  "
		   "Timebase: %d MHz\n\r",
		   (unsigned int) bootInfo->cpu_version,
		   ((unsigned int) bootInfo->clock_frequency) / 1000000,
		   ((unsigned int) bootInfo->bus_frequency) / 1000000,
		   ((unsigned int) bootInfo->timebase_frequency) / 1000000);
	    printf("L1 cache: %d KB (%d byte blocks)  "
		   "L2 cache: %d KB (%d byte blocks)\n\r",
		   ((unsigned int) bootInfo->dCacheL1Size) / 1024,
		   (unsigned int) bootInfo->dCacheL1LineSize,
		   ((unsigned int) bootInfo->L2cachesize) / 1024,
		   (unsigned int) bootInfo->L2linesize);
	}
    }
    printf("CPUs all started.\n\r");



    if (bootInfo->L2cachesize == 0) {
	bootInfo->L2cachesize = 0x400000;
	printf("Warning: OF shows no L2, using size of 0x%lx\n\r",
	       bootInfo->L2cachesize);
    }
    if (bootInfo->L2linesize == 0) {
	bootInfo->L2linesize = bootInfo->dCacheL1LineSize;
    }

    if (bootInfo->clock_frequency==0) {
	/*
	 * # Clock speed in MHz.
	 * set PARAM(CPU.Clock)          200
	 */
	bootInfo->clock_frequency = 200000000ULL;
    }

    if (bootInfo->bus_frequency==0) {
	/* 100 MHz bus */
	bootInfo->bus_frequency = 100000000ULL;
    }

    if (bootInfo->timebase_frequency==0) {
	/* value for "simos -B" (?) on kitch0 (previously in KernelClock.H) */
	if (onSim == SIM_MAMBO) {
	    if (onHV) {
		/* mpeter had 1500M */
		bootInfo->timebase_frequency = 10000000;
	    } else {
		bootInfo->timebase_frequency = 300000000ULL;
		printf("---------increased boot time frequency\n");
	    }
	} else {
	    bootInfo->timebase_frequency = 10000000;
	}
    }

    /* Here we will freeze the timebase for the entire machine, tell
     * all the slave CPUs to go ahead and zero their timebase, then
     * wake the machine up again.  Freezing the timebase before we
     * started the CPUs caused our S70 system to fail.  */

    // We don't do any of this on G5's or on HV
    if (rtas_base == 0) {
	for (cpunumber = 0; cpunumber < 32; cpunumber++) {
	    if ((bootInfo->availCPUs & (1ULL << cpunumber)) != 0) {
		// This just flags that CPU as one that needs to be started
		// (HWInterrupt.C)
		bootInfo->startCPU[cpunumber].startIAR = 1;
	    }
	}
#if 0
    } else if (onSim) {
	// Number of processors to simulate
	bootInfo->availCPUs = (1ULL << SimCutThrough(SimGetMachAttrK,
						     SimNumbPhysCpusK)) - 1;

	printf("Not known how to start secondary processors on mambo\n\r");
#endif
    } else if (!bootInfo->onHV) {
	printf("Freezing timebase, %llx",bootInfo->availCPUs);
	rtas_timebase_call(rtas_base, bootInfo->freeze_time_base);
	for (cpunumber = 0; cpunumber < 32; cpunumber++) {
	    if ((bootInfo->availCPUs & (1ULL << cpunumber)) != 0) {
		if (bootInfo->masterCPU != cpunumber) {
		    bootInfo->startCPU[cpunumber].startIAR = -1LL;
		    sync();
		    /* Wait for secondary CPU to confirm TB zeroed
		       (only check upper word in 32-bit mode) */
		    while (*(volatile unsigned long *)
			   &bootInfo->startCPU[cpunumber].startIAR)
			;
		} else {
		    zerotb();  /* Zero timebase on master CPU */
		}
	    }
	}
	printf("Thawing timebases.\n\r");
	rtas_timebase_call(rtas_base, bootInfo->thaw_time_base);
	printf("Timebases all zeroed.\n\r");
    } else {
	printf("Cannot adjust timebase, no rtas token\n\r");
    }
    return;
}

/*----------------------------------------------------------------------------*/
unsigned int
find_of(unsigned int *ofbase)
{
    int phandle;
    char buf[16];
    unsigned int size = 0;

    phandle = of_finddevice("/options");
    if (phandle!=-1) {
	of_getprop(phandle, "real-base", buf, 16);
	*ofbase = atoi(buf);
	of_getprop(phandle, "real-size", buf, 16);
	size = atoi(buf);
    }

    return size;
}
unsigned int
find_hypertas(void)
{
    int phandle;
    char buf[16];
    int rc;

    phandle = of_finddevice("/rtas");
     rc = of_getprop(phandle, "ibm,hypertas-functions", buf, 16);

    return rc != 0;
}

/*----------------------------------------------------------------------------*/
uval64
stegIndex (uval64 eaddr)
{
    /* select 5 low-order bits from ESID part of EA */
    return (eaddr >> 28) & 0x1F;
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
__mapPage(uval64 vaddr, uval64 paddr, uval64 wimg)
{
    uval64 vsid, esid;
    struct STEG *stegPtr;
    struct STE  *stePtr;
    struct PTEG *ptegPtr;
    struct PTE  *ptePtr;
    struct PTE   pte;
    uval i;
    int segment_mapped = 0;
    uval pteg;
    uval pteg_idx;
    uval64 hash;
    uval64 mask;


    /*
     * Kernel effective addresses have the high-order bit on, thus a
     * kernel ESID will be something like 0x80000000C (36 bits).
     * The 52-bit VSID for a kernel address is formed by prepending
     * 0x0001 to the top of the ESID, thus 0x000180000000C.
     */
    esid = (vaddr >> 28) & 0xFFFFFFFFFULL;
    vsid = esid | 0x0001000000000ULL;

  if (!onHV) {
    /*
     * Make an entry in the segment table to map the ESID to VSID,
     * it it's not already present.
     */
    stegPtr = &(((struct STEG *) (uval32)segmentTableAddr)[stegIndex (vaddr)]);
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
	b1printf("Creating STE for", vaddr);
	b1printf("  esid", esid);
	b1printf("  vsid", vsid);
	if (i >= NUM_STES_IN_STEG) {
	    bprintf ("Error:  Boot-time segment table overflowed (!)");
	    bprintf("Exiting...");
	    bootExit();
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
  }

    pteg = ptegIndex(vaddr, vsid);

    /*
     * Make a page table entry
     */

  if (!onHV) {
    ptegPtr= &(((struct PTEG *)(uval32)pageTableAddr)[ptegIndex(vaddr, vsid)]);

    for (i = 0; i < NUM_PTES_IN_PTEG; i++) {
	ptePtr = &(ptegPtr->entry[i]);
	if (PTE_PTR_V_GET(ptePtr) == 0) break;
    }
    pteg_idx =i;

    if (i >= NUM_PTES_IN_PTEG) {
	b1printf("Creating PTE for", vaddr);
	b1printf("  vsid", vsid);
	b1printf("  ptegIndex", ptegIndex (vaddr, vsid));
	bprintf("");
	bprintf("Error:  Boot-time page table overflowed");
	bprintf("Exiting...");
	bootExit();
    }
  }

    if (ptegIndex (vaddr, vsid) == 0x10C) {
	b1printf("Creating PTE for ", vaddr);
	b1printf("  vsid", vsid);
	b1printf("  ptegIndex", ptegIndex (vaddr, vsid));
	b1printf("  index", i);
    }
    PTE_CLEAR(pte);
    PTE_V_SET(pte, 1);
    PTE_VSID_SET(pte, vsid);
    PTE_H_SET(pte, 0);

    PTE_API_SET(pte, VADDR_TO_API(vaddr));
    PTE_RPN_SET(pte, (paddr >> RPN_SHIFT));
    PTE_R_SET(pte, 0);
    PTE_C_SET(pte, 0);

    PTE_WIMG_SET(pte, wimg);
    PTE_PP_SET(pte, 2 /* writeUserWriteSup */);

    if (!onHV) PTE_SET(pte, ptePtr);

    if (onHV) {
	uval64 args[4];
	uval64 ret1[1];
	sval retcode;

	hash = (vsid & VSID_HASH_MASK);
	hash ^= ((vaddr & EA_HASH_MASK) >> EA_HASH_SHIFT);
	mask = (1ULL << (logNumPTEs - LOG_NUM_PTES_IN_PTEG)) - 1;

	args[0] = 0;
	args[1] = (pteg << 3);
	args[2] = pte.vsidWord;
	args[3] = pte.rpnWord;

	retcode = hcall32_enter(ret1, args);

	if (retcode != 0 /* H_SUCCESS */) {
	    bprintf("hcall32_enter failed");
	    for (;;);
	}
    }
}

void
mapPage(uval64 vaddr, uval64 paddr)
{
    /* w 0 no write-through
     * i 0 allow caching
     * m 1 coherency enforced
     * g 0 no guarding
     */
    __mapPage(vaddr, paddr, 2);
}

void
mapRangeSim(uval64 vaddr, uval64 paddr, uval64 size)
{
    uval64 offset;

    b1printf ("mapping range: vaddr ", vaddr);
    b1printf ("               paddr ", paddr);
    b1printf ("               size  ", size);

    for (offset = 0; offset < size; offset += PGSIZE) {
	mapPage(vaddr + offset, paddr + offset);
    }
}

void
mapRange(uval64 vaddr, uval64 paddr, uval64 size, const char *tag)
{
    uval64 offset;
    useMemLMB(vaddr, (uval32)paddr, (uval32)size, tag);

    size  = size + (vaddr & (PAGE_SIZE-1));
    vaddr = (vaddr + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    paddr = (paddr + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
    size  = (size + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);

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
    };

    enum {
	PT_LOAD = 1,
	PT_GNU_STACK = 0x60000000 + 0x474e551
    };

    struct elf64_hdr *e64 = (struct elf64_hdr *)(uval32)kernStartAddr;
    struct elf64_phdr *phdr;
    int i;
    b1printf("Kern start", kernStartAddr);
    if ((int)e64->e_ident[0] != 0x7f
	|| e64->e_ident[1] != 'E'
	|| e64->e_ident[2] != 'L'
	|| e64->e_ident[3] != 'F'
	|| e64->e_machine != 21 /* EM_PPC64 */) {
	b1printf("Bad header: ", e64->e_ident[0]);
	return (0);
    }
    phdr = (struct elf64_phdr *)(uval32)((uval)e64 + e64->e_phoff);

    // We know that the order is text, data that includes bss
    // Increased to 3: new toolchains add a zero-size GNU_STACK section
    if (e64->e_phnum > 3) {
	b1printf("ERROR: too many ELF segments: ",(uval64) e64->e_phnum);
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
		    bprintf("ERROR: text memsz and text filesz are not equal");
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

		    pi->entryFunc = (uval64 *)(uval32)
			(pi->data.filePtr + e64->e_entry - phdr[i].p_vaddr);
		    b1printf("Entry desc: ", e64->e_entry);
		    b1printf("            ",(uval32)pi->entryFunc);
		    b1printf("            ",(uval64)pi->entryFunc[0]);
		    b1printf("            ",(uval64)pi->entryFunc[1]);
		} else {
		    bprintf("ERROR: entry descriptor is not in data segment");
		    return 0;
		}
	    }
	} else if (phdr[i].p_type == PT_GNU_STACK) {
	    /* We have a GNU_STACK segment - if we want to implement NX pages,
	     * turn on here */
	    bprintf("Found a GNU_STACK segment");

	}
    }
    return 1;
}


//
// everything below is from linux, arch/ppc64/boot/main.c
//
//

#define _ALIGN(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
static char scratch[128<<10];	/* 128kB of scratch space for gunzip */
char *avail_ram;
char *begin_avail, *end_avail;
char *avail_high;
unsigned int heap_use;
unsigned int heap_max;

struct memchunk {
	unsigned int size;
	unsigned int pad;
	struct memchunk *next;
};

static struct memchunk *freechunks;

void *zalloc(void *x, unsigned items, unsigned size)
{
	void *p;
	struct memchunk **mpp, *mp;

	size *= items;
	size = _ALIGN(size, sizeof(struct memchunk));
	heap_use += size;
	if (heap_use > heap_max)
		heap_max = heap_use;
	for (mpp = &freechunks; (mp = *mpp) != 0; mpp = &mp->next) {
		if (mp->size == size) {
			*mpp = mp->next;
			return mp;
		}
	}
	p = avail_ram;
	avail_ram += size;
	if (avail_ram > avail_high)
		avail_high = avail_ram;
	if (avail_ram > end_avail) {
		printf("oops... out of memory\n\r");
		of_exit();
	}
	return p;
}

void zfree(void *x, void *addr, unsigned nb)
{
	struct memchunk *mp = addr;

	nb = _ALIGN(nb, sizeof(struct memchunk));
	heap_use -= nb;
	if (avail_ram == addr + nb) {
		avail_ram = addr;
		return;
	}
	mp->size = nb;
	mp->next = freechunks;
	freechunks = mp;
}

#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

#define DEFLATED	8

void gunzip(void *dst, int dstlen, unsigned char *src, int *lenp)
{
	z_stream s;
	int r, i, flags;

	avail_ram = scratch;
	begin_avail = avail_high = avail_ram;
	end_avail = scratch + sizeof(scratch);

	/* skip header */
	i = 10;
	flags = src[3];
	if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
		printf("bad gzipped data\n\r");
		of_exit();
	}
	if ((flags & EXTRA_FIELD) != 0)
		i = 12 + src[10] + (src[11] << 8);
	if ((flags & ORIG_NAME) != 0)
		while (src[i++] != 0)
			;
	if ((flags & COMMENT) != 0)
		while (src[i++] != 0)
			;
	if ((flags & HEAD_CRC) != 0)
		i += 2;
	if (i >= *lenp) {
		printf("gunzip: ran out of data in header\n\r");
		of_exit();
	}

	s.zalloc = zalloc;
	s.zfree = zfree;
	r = inflateInit2(&s, -MAX_WBITS);
	if (r != Z_OK) {
		printf("inflateInit2 returned %d\n\r", r);
		of_exit();
	}
	s.next_in = src + i;
	s.avail_in = *lenp - i;
	s.next_out = dst;
	s.avail_out = dstlen;
	r = inflate(&s, Z_FINISH);
	if (r != Z_OK && r != Z_STREAM_END) {
		printf("inflate returned %d msg: %s\n\r", r, s.msg);
		of_exit();
	}
	*lenp = s.next_out - (unsigned char *) dst;
	inflateEnd(&s);
}
