/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kinit.C,v 1.25 2004/04/18 15:33:19 mostrows Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include <sys/thinwire.H>
#include "exception/ExceptionLocal.H"
#include <scheduler/Scheduler.H>

#include <bilge/libksup.H>
#include <mmu.H>
#include <mem/PageAllocatorKernPinned.H>
#include <init/memoryMapKern.H>

#include <misc/macros.H>
#include <trace/traceBase.H>

/* 64 MB contiguous real at least is assumed on SIMICS and real hardware.
 */
#define BOOT_MINIMUM_REAL_MEMORY	(0x10000000)	// 256MB
// #define BOOT_MINIMUM_REAL_MEMORY	(0x8000000)	// 128MB
// #define BOOT_MINIMUM_REAL_MEMORY	(0x4000000)	// 64MB

extern "C" void early_printk(const char *fmt, ...);
extern void init_PIC(unsigned int );
extern uval level4_pgt;						// should really be level1_pgt

/* this is called from assembler and passed control in long mode 64 bit
 * interrupts disabled.
 * At this stage the first 32MB have been mapped with 2MB pages.
 */
extern "C" void
kinit()
{
    extern char __bss_end[];
    struct KernelInitArgs kernelInitArgs;
    MemoryMgrPrimitiveKern *memory = &kernelInitArgs.memory;
    uval vp = 0;	/* master processor */
    uval vaddr;

    /* on this machine like all x86 machines nowaydays the boot
     * image is loaded at 1MB.  This is hard coded here.  */
    extern code start_real;
    codeAddress kernPhysStartAddress = &start_real;
    extern code kernVirtStart;

    early_printk("kernPhysStartAddress 0x%lx \n",
		 (unsigned long)kernPhysStartAddress);


    /* We ignore memory below the 1meg boundary.  PhysSize is the
     * size of memory above the boundary.
     */
    uval physSize = BOOT_MINIMUM_REAL_MEMORY -0x100000;
    uval physStart = 0x0;
    uval physEnd = physStart + 0x100000 + physSize;

    early_printk("BOOT_MINIMUM_REAL_MEMORY 0x%lx, physStart 0x%lx,"
		 " physEnd 0x%lx, physSize 0x%lx \n",
		 BOOT_MINIMUM_REAL_MEMORY,  physStart, physEnd, physSize);

    /*
     * We want to map all of physical memory into a V->R region.  We choose a
     * base for the V->R region (virtBase) that makes the kernel land correctly
     * at its link origin, &kernVirtStart.  This link origin must wind up
     * mapped to the physical location at which the kernel was loaded
     * (kernPhysStartAddress).
     */
    uval virtBase = (uval) (&kernVirtStart - kernPhysStartAddress);
    early_printk("&kernVirtStart 0x%lx virtBase 0x%lx \n",
		 (unsigned long long)&kernVirtStart,
		 (unsigned long long)virtBase);

    /*
     * Memory from __end_bss
     * to the end of physical memory is available for allocation.
     * Correct first for the 2MB page mapping the kernel.
     */
    early_printk("__bss_end is 0x%lx physEnd is 0x%lx \n", __bss_end , physEnd);
    uval allocStart = ALIGN_UP(__bss_end, SEGMENT_SIZE);
    uval allocEnd = virtBase + physEnd;

    early_printk("allocStart is 0x%lx allocEnd is 0x%lx \n",
		 allocStart, allocEnd);
    memory->init(physStart, physEnd, virtBase, allocStart, allocEnd);

    /*
     * Remove mappings between allocStart and
     * BOOT_MINIMUM_REAL_MEMORY to allow 4KB page mapping for
     * that range.  No need to tlb invalidate, unless they are
     * touched (debugging).  Actually we need to keep the first
     * 2MB mapping above allocStart so that we can initialize the
     * first 2 (or 3 if we need a PDP page as well) 4KB pages
     * which are PDE and PTE pages for the V->R mapping before
     * they are themselves mapped as 4KB pages.
     */
    early_printk("top page real address is 0x%lx \n", (uval)&level4_pgt);
    uval level1_pgt_virt = memory->virtFromPhys((uval)&level4_pgt);
    early_printk("top page real address is 0x%lx \n", (uval)level4_pgt & ~0xfff);
    early_printk("top page virtual  address is 0x%lx \n", (uval )level1_pgt_virt);

    for (vaddr = allocStart + SEGMENT_SIZE; vaddr < allocEnd; vaddr += SEGMENT_SIZE)	{

#ifndef NDEBUG
      //     early_printk("removing pde, pml4 at virtual address 0x%lx \n", EARLY_VADDR_TO_L1_PTE_P(level1_pgt_virt, vaddr, memory));
      TOUCH(EARLY_VADDR_TO_L1_PTE_P(level1_pgt_virt, vaddr, memory));

      //     early_printk("removing pde, pdp at virtual address 0x%lx \n", EARLY_VADDR_TO_L2_PTE_P(level1_pgt_virt, vaddr, memory));
      TOUCH(EARLY_VADDR_TO_L2_PTE_P(level1_pgt_virt, vaddr, memory));

      //     early_printk("removing pde at virtual address 0x%lx \n", EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt, vaddr, memory));
      TOUCH(EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt, vaddr, memory));
#endif /* #ifndef NDEBUG */


      EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt, vaddr, memory)->P = 0;
      EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt, vaddr, memory)->PS = 0;
      EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt, vaddr, memory)->G = 0;
      EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt, vaddr, memory)->Frame = 0;
      __flush_tlb_one(vaddr);
    }

    /*
     * Because of the 2MB page mapping for the kernel no
     * unused space can be recuperated at a 4KB page granularity.
     * We may want to map the fringe bss with 4KB page(s)
     * or alternatively make free for (pinned only) 4KB allocation
     * the unused 4KB pages unused in the 2MB pages at this point. XXX dangerous
     */

    early_printk("Calling InitKernelMappings\n");
    InitKernelMappings(0, memory);

    // kernelInitArgs.onSim = onSim; not there anymore but where is it set XXX

    kernelInitArgs.vp = 0;
    kernelInitArgs.barrierP = 0;

#define LOOP_NUMBER 	0x000fffff	// iteration counter for delay
    init_PIC(LOOP_NUMBER);

    early_printk("Calling InitIdt\n");
    InitIdt();			// initialize int handlers

    early_printk("Calling enableHardwareInterrupts\n");
    enableHardwareInterrupts();

    early_printk("Calling thinwireInit\n");
    thinwireInit(memory);

    /* no thinwire console XXX taken from mips64 but check  */
    early_printk("Calling LocalConsole and switching to tty \n");
    LocalConsole::Init(vp, memory, CONSOLE_CHANNEL, 1, 0 );

    err_printf("Calling KernelInit.C\n");

    /* Remove the V=R initial mapping only used for jumping to
     * the final mapping, i.e the first 2MB. XXX todo should not
     * do it until VGABASE has been relocated currently mapped
     * V==R XXX cannot use early_printk() from now on.  */
    L3_PTE *p;
    p = EARLY_VADDR_TO_L3_PTE_P(level1_pgt_virt,(uval)0x100000,memory);
    p->P = 0;
    p->PS = 0;
    p->G = 0;
    p->Frame = 0;
    __flush_tlb_one(0x100000);

    KernelInit(kernelInitArgs);
    /* NOTREACHED */
}
