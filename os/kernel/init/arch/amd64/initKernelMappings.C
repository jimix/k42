/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: initKernelMappings.C,v 1.28 2004/03/09 19:32:40 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:

 * Called by machine dependent init to initialize page tables on
 * additional processors by creating page tables needed to map
 * the kernel and to map real memory V->R and backing memory for
 * ProcessorSpecific ranges.

 * On boot processors, VM translation is enabled.
 * (V->R) is a linear mapping of real at a fixed offset
 * **************************************************************************/

#include "kernIncs.H"
#include "sys/thinwire.H"
#include "exception/ExceptionLocal.H"
#include "init/arch/amd64/APIC.H"
#include <scheduler/Scheduler.H>
#include <mem/HardwareSpecificRegions.H>
#include <arch/amd64/segnum.h>              // NUM_GDT_ENTRIES, ...

#include <bilge/libksup.H>
#include <mmu.H>
#include <mem/PageAllocatorKernPinned.H>
#include <init/memoryMapKern.H>

extern "C" void early_printk (const char *fmt,...);

void mybreakpoint ()
{
  early_printk("\n");
}

/* Use canned protection values - in our design
 * all page protection occurs in page entries
 * 	        1,      //  present
 *		1,      // RW
 *		1,      // user
 *		0,      // page level write thru
 *
 *		0,      // page level cache disable
 *		1,      // accessed
 *		0,      // ignored, would have been dirty
 *		0,      // must be zero
 *
 *		0,      // ignored, would have been global XXX
 *		0,
 *		0,
 *		0,
 *
 *		0;
 * PDE pde = {1,1,1,0, 0,1,0,0, 0,0,0,0, 0};	an entry in the pde page
 */

const PTE initpte =
{
  0 /* not present */ ,
  1 /* RW */ ,
  0 /* not user */ ,
  0 /* not write thru */ ,

  0 /* not cache disabled */ ,
  0 /* not accessed */ ,
  0 /* not dirty */ ,
  0 /* must be zero */ ,
  /* only the PTE itself can have the G bit set on x86-64  */
  0 /* global */ ,

  0 /* avail to sw */ ,
  0,
  0,
  0

};

/*
 * Add user-mode accessibility to a previously-mapped range of
 * pages.  Note that we have mapped all levels V->R as kernel
 * only, if we want to provide user-mode accessibility to a page
 * it must be granted at all levels of the hierarchy.  This works
 * only for 4KB page.
 */
static void
AddUserPrivilege (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory,
		  uval virtaddr, uval size)
{
  uval v;

  for (v = virtaddr; v < (virtaddr + size); v += PAGE_SIZE) {
    if (EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

    if (EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

    if (EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->P == 0 ||
	EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->PS) {
      early_printk("ERROR<%s,%d>: No page table or 2MB page size\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

    if (EARLY_VADDR_TO_L4_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

  }
}
/* static */ void
ReadOnlyUserandKernel (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory,
		       uval virtaddr, uval size)
{
  uval v;

  for (v = virtaddr; v < (virtaddr + size); v += PAGE_SIZE) {
    if (EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

    if (EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

    if (EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else
      EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->US = 1;

    if (EARLY_VADDR_TO_L4_PTE_P(pageDirectory, virtaddr, memory)->P == 0) {
      early_printk("ERROR<%s,%d>: No page table\n",
		   __FILE__, __LINE__);
      BREAKPOINT;
    }
    else {
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, virtaddr, memory)->US = 1;
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, virtaddr, memory)->RW = 0;
    }

  }
}

static uval once = 0;

/* Create and map an intermediate level directory page. */
void
CreateAndMapPDPPage (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory, uval physaddr, uval virtaddr)
{
  uval tmp, i;
  PTE * pagetable;

  /* allocate a page for that level. We left mapped via a 2MB
   * mapping the first 2MB above the (aligned) bss so that we can
   * initialize the first 3 pages which will be the PDP, PDE and
   * PTE 4KB pages which underpin themselves.
   */

  memory->alloc(tmp, PAGE_SIZE, PAGE_SIZE);
#ifdef DEBUG_BOOT
  early_printk("allocating PDP page 0x%lx \n ",(uval) tmp);
#endif /* #ifdef DEBUG_BOOT */

  pagetable = (PTE *) tmp;
  for (i = 0; i < PAGE_SIZE / sizeof(PTE); i++) {
    pagetable[i] = initpte;
  }
  passertSilent(once == 1, BREAKPOINT);

  /* now map it into the previous level. */
  EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->Frame =
    memory->physFromVirt((uval) pagetable) >> PAGE_ADDRESS_SHIFT;
  EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->RW = 1;
  EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->US = 1;
  EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->A = 1;
  /*    EARLY_VADDR_TO_L1_PTE_P(pageDirectory,virtaddr, memory)->G = 1; */
  EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->P = 1;
}

void
CreateAndMapPDEPage (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory, uval physaddr, uval virtaddr)
{
  uval tmp, i;
  PTE * pagetable;

  /* allocate a page for that level. */
  memory->alloc(tmp, PAGE_SIZE, PAGE_SIZE);
#ifdef DEBUG_BOOT
  early_printk("allocating PDE page 0x%lx \n ",(uval) tmp);
#endif /* #ifdef DEBUG_BOOT */
  pagetable = (PTE *) tmp;
  for (i = 0; i < PAGE_SIZE / sizeof(PTE); i++) {
    pagetable[i] = initpte;
  }
  passertSilent(once == 1, BREAKPOINT);

  /* now map it into the previous level.  Unless we have not
   * initialized yet the first PTE page.
   */

  EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->Frame = memory->physFromVirt((uval) pagetable) >> PAGE_ADDRESS_SHIFT;
  EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->RW = 1;
  EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->US = 1;
  EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->A = 1;
  EARLY_VADDR_TO_L2_PTE_P(pageDirectory, virtaddr, memory)->P = 1;
}

void
CreateAndMapPTEPage (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory, uval physaddr, uval virtaddr)
{
  uval tmp, i;
  PTE * pagetable;

  /* allocate a page for that level. */
  memory->alloc(tmp, PAGE_SIZE, PAGE_SIZE);
#ifndef NDEBUG
  /*    early_printk("allocating PTE 0x%lx \n ", (uval) tmp); */
#endif /* #ifndef NDEBUG */
  pagetable = (PTE *) tmp;
  for (i = 0; i < PAGE_SIZE / sizeof(PTE); i++) {
    pagetable[i] = initpte;
  }

  /* now map it into the previous level defer if we still need to
   * retain addressability via the 2MB mapping to enter the first
   * page pte because this page underpins itself.
   */
  if (once) {
    EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->Frame = memory->physFromVirt((uval) pagetable) >> PAGE_ADDRESS_SHIFT;
    EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->RW = 1;
    EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->US = 1;
    EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->A = 1;
    EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->P = 1;
  }
}


/*
 * Map a contiguous range of physical pages.  No page of the
 * corresponding virtual range should currently be mapped.
 */
static void
MapPageRange (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory,
	      uval physaddr, uval virtaddr, sval size,
	      uval disableCaching = 0)
{
  uval sz;
  uval vaddr;
  uval paddr;
  uval end;

  while (size > 0) {
    /* Calculate how much of the range lies in one segment. */
    sz = SEGMENT_SIZE - (virtaddr & (SEGMENT_SIZE - 1));
    if (sz > (uval) size)
      sz = size;

    /* if an entry entry for that range does not exist in a
     * directory level allocate a page and map it for that
     * level.
     */
    if (EARLY_VADDR_TO_L1_PTE_P(pageDirectory, virtaddr, memory)->P == 0)
      CreateAndMapPDPPage(pageDirectory, memory, physaddr, virtaddr);

    if (EARLY_VADDR_TO_L2_PTE_P((uval) pageDirectory, virtaddr, memory)->P == 0)
      CreateAndMapPDEPage(pageDirectory, memory, physaddr, virtaddr);

    if (EARLY_VADDR_TO_L3_PTE_P((uval) pageDirectory, virtaddr, memory)->P == 0)
      CreateAndMapPTEPage(pageDirectory, memory, physaddr, virtaddr);
    else
      if (EARLY_VADDR_TO_L3_PTE_P((uval) pageDirectory, virtaddr, memory)->PS) {
	  /*
	   * This is the leftover 2MB mapping for which we want
	   * to substitute 4KB page mapping. We need a PTE page
	   * but it will not be entered in the PDE until it is
	   * itself addressable as a 4KB page i.e. before its
	   * pte entry has been entered ... into itself.
	   */
	  CreateAndMapPTEPage(pageDirectory, memory, physaddr, virtaddr);
	  passertSilent(once == 0, BREAKPOINT);
	}

    end = virtaddr + sz;

    for (vaddr = virtaddr, paddr = physaddr; vaddr < end; vaddr += PAGE_SIZE, paddr += PAGE_SIZE) {
      if (EARLY_VADDR_TO_L4_PTE_P(pageDirectory, vaddr, memory)->P) {
	early_printk("ERROR<%s, %d>: MapPageRange: "
		     "0x%lx already mapped\n",
		     __FILE__, __LINE__, vaddr);
	BREAKPOINT;
      }
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, vaddr, memory)->Frame = paddr >> PAGE_ADDRESS_SHIFT;
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, vaddr, memory)->RW = 1;
      EARLY_VADDR_TO_L1_PTE_P(pageDirectory, vaddr, memory)->A = 1;
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, vaddr, memory)->G = 1;
      EARLY_VADDR_TO_L4_PTE_P(pageDirectory, vaddr, memory)->P = 1;
      if (disableCaching)
	EARLY_VADDR_TO_L4_PTE_P(pageDirectory, vaddr, memory)->PCD = 1;

      /* if the pde emtry still refers to a 2MB mapping which
       * we just used to initialize the allocated pte page
       * we set up for 4KB mapping and invalidate the 2MB
       * mapping.
       */
      if (EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->PS) {
	passertSilent(once == 0, BREAKPOINT);
	passertSilent(vaddr == virtaddr, BREAKPOINT);

	passertSilent(memory->physFromVirt((uval) vaddr) == EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->Frame << PAGE_ADDRESS_SHIFT, BREAKPOINT);
	passertSilent(EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr + SEGMENT_SIZE, memory)->P == 0, BREAKPOINT);
	/*
	 * finally remove the 2MB mapping and replace it by
	 * a 4KB mapping with the newly allocated pte page
	 * entered in the pde page.
	 */
	EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->RW = 1;
	EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->PS = 0;
	/* remove 2MB mapping */
	EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->A = 1;
	EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->G = 1;
	EARLY_VADDR_TO_L3_PTE_P(pageDirectory, virtaddr, memory)->P = 1;
	__flush_tlb_one(virtaddr);
	once = 1;
      }
#ifndef NDEBUG
      /* early_printk("touching vaddr 0x%lx\n", vaddr); */
      TOUCH_WRITE(vaddr);
#endif /* #ifndef NDEBUG */
    }

    /* Move to the next segment. */
    /* early_printk("touching virtaddr 0x%lx\n", virtaddr); */
    TOUCH_WRITE(virtaddr);
    virtaddr += sz;
    physaddr += sz;
    size -= sz;
  }
}


static void
InitKernelIDTR_GDTR (MemoryMgrPrimitiveKern * memory)
{
  /*
   * We have been running with a large segment base which, when
   * added to a "virtual" address, produced the correct physical
   * address.  Now set the segment bases to zero.  Because of the
   * architected behavior of x86, this does not take effect until
   * the segment registers are reloaded, which happens in
   * bios_xlate_on().
   */

  /* **************************************************************
   * DEBUGGER MUST NOT BE USED FROM HERE
   ************************************************************* */

  /*    Gdt[KERNEL_CS >> 3].setbase(0); */
  /*    Gdt[KERNEL_SS >> 3].setbase(0); */
  DTParam gdtr;
  /*    gdtr.base  = (uval) &Gdt; */
  /*    gdtr.limit = (NUM_GDT_ENTRIES * sizeof(Gdt[0])) - 1; */

  /*
   * reload idt register with the virtual address instead o
   */
  /*    asm("  sidt %0" : "=m" (Idtr)); */
  /*    Idtr.base = memory->virtFromPhys(Idtr.base); */
#ifdef DEBUG_BOOT
  early_printk(" Idtr.base 0x%lx, Idtr.limit 0x%x \n", Idtr.base, Idtr.limit);
#endif /* #ifdef DEBUG_BOOT */
  asm("  lidt %0": : "m"(Idtr));

  /*
   * copy gdt to exception local gdt
   */
#ifdef DEBUG_BOOT
  early_printk(" Gdt 0x%lx,  \n", memory->virtFromPhys((uval) & Gdt[0]));
  early_printk(" sizeof(SegDesc)  0x%x,  \n", sizeof(SegDesc));
#endif /* #ifdef DEBUG_BOOT */
  for (int ix = 0; ix < 8; ix++)
#ifdef DEBUG_BOOT
    early_printk(" SEGDESC 0x%lx \n", *(uval *) memory->virtFromPhys(((uval) & Gdt[ix])));
#endif /* #ifdef DEBUG_BOOT */
  memcpy(&exceptionLocal.Gdt,(const void *) memory->virtFromPhys((uval) Gdt),
	 sizeof(exceptionLocal.Gdt));

  gdtr.limit = sizeof(exceptionLocal.Gdt) - 1;
  gdtr.base = (uval) & exceptionLocal.Gdt;
  asm("  lgdtl %0": : "m"(gdtr));

  /* ***************************************************************
   * DEBUGGER CAN BE USED AGAIN FROM HERE
   ************************************************************** */
}

/* should have been renamed extern uval level1_pgt */
extern uval level4_pgt;


static void
InitKernelPageDirectory (PTE * pageDirectory, MemoryMgrPrimitiveKern * memory)
{
  uval physaddr = memory->physFromVirt((uval) pageDirectory);
  early_printk("&pageDirectoryPhys = 0x%lx\n", physaddr);

  while (1)
    early_printk("Fixme for smp ");

  DTParam gdtr;
  gdtr.limit = sizeof(exceptionLocal.Gdt) - 1;
  gdtr.base = (uval) & Gdt;
  asm volatile("  lgdt %0": : "m"(gdtr));

  asm volatile("  movq %0, %%cr3": : "r"(physaddr));
  early_printk("Swapped in new pageDirectory\n");

  /* copy gdt inherited from boot program to exception local gdt */
  memcpy(&exceptionLocal.Gdt, &Gdt,
	 sizeof(exceptionLocal.Gdt));

  /* switch to gdt in exception local */
  gdtr.limit = sizeof(exceptionLocal.Gdt) - 1;
  gdtr.base = (uval) & exceptionLocal.Gdt;
  asm("  lgdt %0": : "m"(gdtr));
}

/* entered in 64 bit mode, interrupt disabled
 * NMI disabled	(needs to reenable somewhere)
 * XXX
 */
void
InitKernelMappings (VPNum vp, MemoryMgrPrimitiveKern * memory)
{
  PTE * pageDirectory;
  uval tmp;
  uval i;

  early_printk("vp = %ld\n", vp);

  /*
   * Allocate top-level and ALL intermediate level page directory
   * necessary to map all real memory above the kernel V->R
   */

  if (vp) {
    memory->alloc(tmp, PAGE_SIZE, PAGE_SIZE);
    pageDirectory = (PTE *) tmp;
    for (i = 0; i < (PAGE_SIZE / sizeof(PTE)); i++) {
      pageDirectory[i] = initpte;
    }
  }
  else {
    uval level1_pgt_virt = memory->virtFromPhys((uval) & level4_pgt);
    pageDirectory = (PTE *) level1_pgt_virt;
  }

  /*    early_printk("Initialized pageDirectory at raddr  0x%lx\n", pageDirectory); */

  /*
   * Map physical memory not already mapped (above 2MB-aligned end of bss).
   * Could pass a parameter instead XXX
   */
  uval raddr = memory->physStart();
  tmp = memory->virtFromPhys(raddr);
  while (EARLY_VADDR_TO_L3_PTE_P(pageDirectory, tmp, memory)->PS) {
    tmp += SEGMENT_SIZE;
    raddr += SEGMENT_SIZE;
  }

  /* we have left one extra 2MB mapping above 2MB aligned end of
   * bss ...  see kinit()
   */
  tmp -= SEGMENT_SIZE;
  raddr -= SEGMENT_SIZE;

  early_printk("Mapping real memory 0x%lx to 0x%lx\n", raddr, tmp);
  MapPageRange(pageDirectory, memory, raddr, tmp,
	       (memory->physEnd() - raddr));

  /* flush tlb for the extra 2MB mapping left above bss... see above */
  __flush_tlb_one(tmp);

#ifndef NDEBUG
  /* touch all real memory we just mapped. */
  for (uval raddr = raddr; raddr < memory->physEnd(); raddr += SEGMENT_SIZE) {
    /* early_printk("touching raddr 0x%lx \n", raddr); */
    TOUCH((uval) memory->virtFromPhys(raddr));
  }
#endif /* #ifndef NDEBUG */

  /*
   * For virtual processors other than 0, initialize mapping for
   * APIC using virtual address previously allocated.  This has to
   * be allocated early, since we handle an interrupt before
   * we are fully up, which has to be acked using APIC
   */
  if (vp != 0) {
    if (HardwareSpecificRegions::IsInitialized()) {
      MapPageRange(pageDirectory, memory, 0xFEE00000,
		   HardwareSpecificRegions::GetAPICVaddr(), PAGE_SIZE, 1);
    }
  }

#ifdef notdef
  /*
   * Map the first 1 meg virtual = real for now.  We need a better
   * strategy for this later.  First 1 meg is used for BIOS calls.
   */
  MapPageRange(pageDirectory, memory, 0, 0, 0x100000);
  /*  early_printk("Map first 1MB\n"); */
#endif /* #ifdef notdef */

  /*
   * Map in static processor-specific memory area.
   * includes in particular lTransTableLocal and exceptionLocal.
   */
  uval pSpecificSpace, physaddr;
  memory->alloc(pSpecificSpace, kernelPSpecificRegionSize, PAGE_SIZE);
  physaddr = memory->physFromVirt(pSpecificSpace);
  early_printk("Mapping processor-specific memory area \n");
  passertSilent(uval(&exceptionLocal) == kernelPSpecificRegionStart, BREAKPOINT);
  early_printk("kernelPSpecificRegionStart = 0x%lx pSpecificPhys = 0x%lx\n", kernelPSpecificRegionStart, physaddr);
  early_printk("kernelPSpecificRegionSize = 0x%lx \n", kernelPSpecificRegionSize);

  MapPageRange(pageDirectory, memory, physaddr,
	       kernelPSpecificRegionStart, kernelPSpecificRegionSize);
  early_printk("Mapped processor-specific memory area \n");
#ifndef NDEBUG
  for (uval vaddr1 = kernelPSpecificRegionStart; vaddr1 < (kernelPSpecificRegionStart + kernelPSpecificRegionSize);
       vaddr1 += PAGE_SIZE) {
    TOUCH(vaddr1);
  }
#endif /* #ifndef NDEBUG */

  /*
   * Allocate and map physical memory for the common
   * processor-specific memory area.  Contains (only)
   * kernelInfoLocal and extRegsLocal.  The global (per-processor)
   * kernelInfoLocal structure is mapped read-only in both user
   * and kernel modes.  Exception-level code uses the V->R
   * address stored in exceptionLocal.kernelInfoPtr when updating
   * the structure.
   */
  uval commonPSR;
  memory->alloc(commonPSR, commonPSpecificRegionSize, PAGE_SIZE);
  physaddr = memory->physFromVirt(commonPSR);
#ifdef DEBUG_BOOT
  early_printk("Mapping commonPSR \n");
#endif /* #ifdef DEBUG_BOOT */
  MapPageRange(pageDirectory, memory, physaddr,
	       commonPSpecificRegionStart, commonPSpecificRegionSize);
#ifndef NDEBUG
  TOUCH(commonPSpecificRegionStart);
#endif /* #ifndef NDEBUG */

  /*
   * Make the extRegsLocal subrange of the common processor-specific
   * area user-writable.  and the kernelInfoLocal read/only for
   * everybody.
   */
#ifdef DEBUG_BOOT
  early_printk("AddUserPrivilege \n");
#endif /* #ifdef DEBUG_BOOT */
#ifndef NDEBUG
  TOUCH(&extRegsLocal);
#endif /* #ifndef NDEBUG */
  AddUserPrivilege(pageDirectory, memory,
		   (uval)(&extRegsLocal), sizeof(extRegsLocal));
#ifdef DEBUG_BOOT
  early_printk("ReadOnlyUserandKernel \n");
#endif /* #ifdef DEBUG_BOOT */
#ifndef NDEBUG
  TOUCH(&kernelInfoLocal);
#endif /* #ifndef NDEBUG */
#ifdef DEBUG_BOOT
  early_printk("&kernelInfoLocal 0x%lx sizeof(kernelInfoLocal) 0x%lx \n",(uval)(&kernelInfoLocal), sizeof(kernelInfoLocal));
#endif /* #ifdef DEBUG_BOOT */
  ReadOnlyUserandKernel(pageDirectory, memory,
			(uval)(&kernelInfoLocal), sizeof(kernelInfoLocal));
#ifdef DEBUG_BOOT
  early_printk("ReadOnlyUserandKernel ... done \n");
#endif /* #ifdef DEBUG_BOOT */

  /*
   * Substitute local pageDirectory for non boot processors others.
   */
  if (vp == 0) {
    InitKernelIDTR_GDTR(memory);
  }
  else {
    InitKernelPageDirectory(pageDirectory, memory);
  }

  /*
   * Set the current and kernel segment table pointers.  Note
   * that on x86-like, machines a SegmentTable object is
   * structurally identical to a page directory.
   */
#ifdef DEBUG_BOOT
  early_printk("setting currentSegmentTable and exceptionLocal.kernelSegmentTable ");
#endif /* #ifdef DEBUG_BOOT */
  exceptionLocal.currentSegmentTable =
    exceptionLocal.kernelSegmentTable =
    (SegmentTable *) pageDirectory;

  /*
   * Record V->R address of KernelInfo for kernel write access.
   */
#ifdef DEBUG_BOOT
  early_printk("setting kernelInfoPtr ");
#endif /* #ifdef DEBUG_BOOT */
  exceptionLocal.kernelInfoPtr = (KernelInfo *)
    (commonPSR + (uval(&kernelInfoLocal) - commonPSpecificRegionStart));

  /*
   * The global (per-processor) kernelInfoLocal structure is mapped
   * read-only in both user and kernel modes.  Exception-level code uses
   * the V->R address stored in exceptionLocal.kernelInfoPtr when updating
   * the structure.  We initialize the onSim field as well as other fixed
   * values in kernelInfoLocal here once and for all.
   */
    exceptionLocal.kernelInfoPtr->init(
#ifdef CONFIG_SIMICS
	/* onSim */		1,
#else /* #ifdef CONFIG_SIMICS */
	/* onSim */		0,
#endif /* #ifdef CONFIG_SIMICS */
	/* numaNode */		0,
	/* procsInNumaNode */	1, // XXX -- for more processors 
	/* physProc */		vp,
	/* curPhysProcs */	1, // XXX -- get the truth
	/* maxPhysProcs */	1, // XXX -- get the truth
	/* sCacheLineSize */	32,  // XXX -- pick up from bootinfo
	/* pCacheLineSize */	32,  // XXX -- pick up from bootinfo
	/* controlFlags */       0);  // XXX -- pick up from bootinfo
    //FIXME: where to get numa values? For now use 0 and total procs, but
    //        need more info on numa topology.
    exceptionLocal.copyTraceInfo();

  /*
   * Set currentProcessAnnex to NULL to indicate we're still booting.
   */
  exceptionLocal.currentProcessAnnex = NULL;

  early_printk("exiting InitKernelMappings \n");
}

void MapTraceRegion(uval vaddr, uval space, uval size)
{
    // FIXME -- NYI
}
