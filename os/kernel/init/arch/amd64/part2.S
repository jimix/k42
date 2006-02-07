/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *      
 * $Id: part2.S,v 1.6 2001/10/25 17:40:52 peterson Exp $
 *      
 *****************************************************************************/


#include <sys/kinclude.H>
#include <misc/asm.h>
#include "init/kernel.H"
#include <sys/arch/amd64/asmConstants.H>
#include <arch/amd64/segnum.h>              /* KERNEL_CS, ... */


/* This file is assembled and executed in real/protected mode, so
   it's addresses are all "real" physical addresses, not virtual
   addresses.  Memory layout is static and manual.  We assume
   everything is loaded starting at 0x100000 (__KERNEL_START).
   There are various page tables that need to be defined and we put
   those at fixed locations using the .org assembler command.  These
   are all relative to __KERNEL_START.  The memory layout is defined
   by the following symbols.
*/

/* This code gets one page (4K bytes) to work.  After we are
   done we will branch off to the rest of the kernel, which
   runs in high-address virtual space.  We will use this page
   as an initial stack for that code. */

#define __KERNEL_START    0x100000

#define __GDT32           0x0fc0     /* need 34 bytes */
#define __LJUMP           0x0ff8     /* need  6 bytes */
#define __LEVEL4_PGT      0x1000     /* each page table is 4K bytes */
#define __LEVEL3_IPT      0x2000
#define __LEVEL3_KPT      0x3000
#define __LEVEL2_IPT      0x4000
#define __LEVEL2_KPT      0x5000

/* offset from real to virtual addresses
   where kernel starts in virtual */
#define __VIRTUAL_OFFSET  0x700000000000  


/* the stack starts at the bottom of the code area, just
   before the level 4 page table, but in virtual space */
#define __KERNEL_STACK    (__KERNEL_START+__LEVEL4_PGT+__VIRTUAL_OFFSET)

	
/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */
/* KERNEL ENTRY POINT */
	.text
CODE_ENTRY(start)

/* part of the job of a boot loader would be to establish a
   valid memory state for the kernel code.  Specifically, we
   think that the Multiboot standard does a reasonable job
   of this.  It says that when we are entered, the following
   should be true:

> 
> CS must be a 32-bit read/execute code segment with an offset
> of 0 and a limit of 0xffffffff.
> 
> DS, ES, FS, GS, and SS must be a 32-bit read/write data
> segments with an offset of 0 and a limit of 0xffffffff.
> 
> The address 20 line must be usable for standard linear 32-bit
> addressing of memory (in standard PC hardware, it is wired to 0
> at bootup, forcing addresses in the 1-2 MB range to be mapped to
> the 0-1 MB range, 3-4 is mapped to 2-3, etc.).
> 
> Paging must be turned off. 
> 
> The processor interrupt flag must be turned off. 
> 
> EAX must contain the magic value 0x2BADB002; the presence of this
> value indicates to the OS that it was loaded by a
> MultiBoot-compliant boot loader (e.g. as opposed to another type
> of boot loader that the OS can also be loaded from).
> 
> EBX must contain the 32-bit physical address of the
> multiboot_info structure provided by the boot loader (see below).
> 
> All other processor registers and flag bits are undefined. This
> includes, in particular:
> 
> ESP: the 32-bit OS must create its own stack as soon as it needs one. 
> 
> GDTR: Even though the segment registers are set up as described
> above, the GDTR may be invalid, so the OS must not load any
> segment registers (even just reloading the same values!) until it
> sets up its own GDT.
> 
> IDTR: The OS must leave interrupts disabled until it sets up its
> own IDT.
>

  If your way of loading the kernel does not do this, it must be
  done before we get to start32 (below).
*/


#ifndef BOOTLOADER
#endif /* #ifndef BOOTLOADER */

	
/* ************************************************************ */
/* main entry point -- assumes the above state -- 32-bit protected
                       mode.  */
/* ************************************************************ */	

/* The following code is lifted from linux/arch/x86_64/kernel/head.S */


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */
CODE_ENTRY(start32)

     .code32
	/*
	 * We are being loaded at 0x100000.
	 *
	 * At this point the CPU is running in 32-bit protected
	 * mode (CS.D = 1) with paging disabled. The point of
	 * this code is to switch to 64-bit long mode with an
	 * identity mapping for kernel (V=R) and to jump into the
	 * kernel virtual addresses.  There is no stack until we
	 * set one up.  
	 */

	/* First, check if extended functions are implemented */
        movl	$0x80000000, %eax     /* ask about largest extended-function */
	cpuid
	cmpl	$0x80000000, %eax
	jbe	no_long_mode
	/* Check if long mode is implemented */
	movl	$0x80000001, %eax
	cpuid
	btl	$29, %edx		/* test bit 29 for long mode */
	jc	long_mode

	/* This isn't an x86-64 CPU, so hang */
no_long_mode:
	jmp	no_long_mode

	/*
	 * Prepare for entering 64-bits mode
	 */
long_mode:
	/* Enable PAE mode and PGE */
	xorl	%eax, %eax
	btsl	$5, %eax          /* bit 5 is PAE */
	btsl	$7, %eax          /* bit 7 is PGE (page global enable) */
	movl	%eax, %cr4

	/* Setup early boot stage 4 level pagetables */
	/* stage 4 page table is at 1Meg plus 1 page.  We started
 	   this code segment at 1Meg.  So we need to finish this
 	   code within 1 page */
	movl	$__KERNEL_START+__LEVEL4_PGT, %eax  
	movl	%eax, %cr3

	/* Setup EFER (Extended Feature Enable Register) */
	movl	$0xc0000080, %ecx
	rdmsr
	/* Fool rdmsr and reset %eax to avoid dependences */
	xorl	%eax, %eax
	/* Enable Long Mode */
	btsl	$8, %eax
	/* Enable System Call */
	/* btsl	$0, %eax  -- leave this out for now */
	/* Make changes effective */
	wrmsr

	xorl	%eax, %eax
	/* Enable paging and in turn activate Long Mode */
	btsl	$31, %eax
	/* Enable protected mode */
	btsl	$0, %eax

        /* In k42 we save and restore floating point as volatile
         * very frequently and clearly this feature should be
         * used to save/restore only when really needed.  Will
         * set up an individual stack for this
         * device-not-available (#NM) interrupt and save in the
         * handler.  Because we do not expect use of floating
         * point initially we set MP so that WAIT/FWAIT will
         * trigger an interrupt and remind us of work to be done
         * XXX pdb */
		
	/* Enable MP */
	btsl	$1, %eax   /* MP set, enables interrupt on FWAIT */
	/* Enable ET */
	btsl	$4, %eax   /* already hard coded to 1 (hw) */
	/* Enable NE */
	btsl	$5, %eax   /* enables FPU error interrupt */
	/* Enable WP */
	btsl	$16, %eax  /* prevent kernel write in userland (COW ?) */
	/* Enable AM */
	btsl	$18, %eax  /* enables interrupt on mis-alignment at user level CPL3 */
	/* Make changes effective */
	movl	%eax, %cr0
	jmp	reach_compatibility_mode
reach_compatibility_mode:
	
	/*
	 * At this point we're in long mode but in 32-bit
	 * compatibility mode with EFER.LME = 1, CS.L = 0, CS.D =
	 * 1 (and in turn EFER.LMA = 1).  Now we want to enter
	 * 64-bit mode, to do that we load the new gdt/idt that
	 * has KERNEL_CS with CS.L = 1.  */

	/* Load new GDT with the 64-bit segment using 32-bit descriptor */
	/* To avoid 32-bit relocations we use fixed addresses here */
	movl	$pGDT32, %eax
	lgdt	(%eax)
	movl    $ljumpvector, %eax
	/* Finally jump to 64-bit mode */
	ljmp	*(%eax)


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

	.code64
reach_long64:
	/*
	 * Where we're running at 0x0000000000100000, and yes, finally
	 * in 64-bit mode.
	 */

	/* Setup the first kernel stack */
	movq    $__KERNEL_STACK,%rax
	movq	%rax, %rsp

	/* zero EFLAGS after setting rsp */
	pushq $0
	popfq

	/*
	 * We must switch to a new descriptor in kernel space for
	 * the GDT because soon the kernel won't have access
	 * anymore to the user space addresses where we're
	 * currently running on. We have to do that here because
	 * in 32-bit we couldn't load a 64-bit linear address.
	 */
	lgdt	pGDT64

	/* esi is a pointer to real mode structure with
	   interesting info.  pass it to C */
	/* note that edi has a physical address and we will need
	   the identity page mapping to be able to get it, or we
	   need to offset it into kernel virtual by adding
	   __VIRTUAL_OFFSET */
	movl	%esi, %edi
	movq	$__VIRTUAL_OFFSET,%rax
	add     %rax,%rdi
	
	
	/* Finally jump to run C code and to be on real kernel
	 * address Since we are running on identity-mapped space
	 * we have to jump to the full 64-bit address , this is
	 * only possible as indirect jump 
	 */
	movq	initial_code(%rip),%rax
	jmp	*%rax

initial_code:
	.extern	x86_64_start_kernel
	.quad	x86_64_start_kernel




/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

	.org __GDT32
gdt32_table:
	.quad	0x0000000000000000	/* 0 -This one is magic */
	.quad	0x0000000000000000	/* 1 - unused */
	.quad	0x00af9a000000ffff	/* KERNEL_CS, base = 0,
					   limit = 0xfffff,
					   G(ranularity=4K),
					   L(ong 64-bit), P(resent),
					   DPL=0 */
gdt32_end:	



pGDT32:
	.word	gdt32_end-gdt32_table
	.quad	gdt32_table


/* ************************************************************ */
	
	.org __LJUMP
ljumpvector:
	.long	reach_long64
	.word	KERNEL_CS


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */
	/*
	 * This default setting generates an ident mapping at
	 * address 0x100000 and a mapping for the kernel that
	 * precisely maps virtual address 0x0000700000000000 to
	 * physical address 0x000000. (always using 2Mbyte large
	 * pages provided by PAE mode).  This puts the physical
	 * memory which is loaded at the virtual address that it
	 * is relocated to. */

	/* 0x0000700000000000 breaks down to
	   0x0000 for sign extend
	   0x70+0 bit (for level 4 page index) -> 0xe0 = 224 */
	   

#define CBITS 0x7   /* P(resent), R/W(rite), U/S(upervisor) */

	
	.org __LEVEL4_PGT
	.globl  level4_pgt
level4_pgt:
	.quad	__KERNEL_START+__LEVEL3_IPT+CBITS   /* 0 */
	.fill	223,8,0					/* 1 to 223 */
	.quad	__KERNEL_START+__LEVEL3_KPT+CBITS  /* 224 */
	.fill	 31,8,0					/* 225 to 255 */
	.fill	256,8,0					/* 256 to 511 */


	.org __LEVEL3_IPT
level3_ident_pgt:
	.quad	__KERNEL_START+__LEVEL2_IPT+CBITS
	.fill	511,8,0

	.org __LEVEL3_KPT
level3_kernel_pgt:
	.quad	__KERNEL_START+__LEVEL2_KPT+CBITS
	.fill	511,8,0


	.org __LEVEL2_IPT
level2_ident_pgt:
	/* the kernel identity mapping allows the kernel to
		pick things up from low memory with a virtual
		address. */
	/* here we map only the first 32*2Meg of low memory */
	/* each entry has low order bits of 0083 which
		means:	1=P(resent), 2=R/W(rite), and
		80=2Meg page size.  G is off, so that
		these pages are flushed from the TLB when
		we switch to user state */
	.quad	0x0000000000000083
	.quad	0x0000000000200083
	.quad	0x0000000000400083
	.quad	0x0000000000600083
	.quad	0x0000000000800083
	.quad	0x0000000000A00083
	.quad	0x0000000000C00083
	.quad	0x0000000000E00083
	.quad	0x0000000001000083
	.quad	0x0000000001200083
	.quad	0x0000000001400083
	.quad	0x0000000001600083
	.quad	0x0000000001800083
	.quad	0x0000000001A00083
	.quad	0x0000000001C00083
	.quad	0x0000000001E00083
	.quad	0x0000000002000083
	.quad	0x0000000002200083
	.quad	0x0000000002400083
	.quad	0x0000000002600083
	.quad	0x0000000002800083
	.quad	0x0000000002A00083
	.quad	0x0000000002C00083
	.quad	0x0000000002E00083
	.quad	0x0000000003000083
	.quad	0x0000000003200083
	.quad	0x0000000003400083
	.quad	0x0000000003600083
	.quad	0x0000000003800083
	.quad	0x0000000003A00083
	.quad	0x0000000003C00083
	.quad	0x0000000003E00083
	.fill	480,8,0
	
	.org __LEVEL2_KPT
level2_kernel_pgt:

	/* the kernel virtual mapping allows the kernel to
	   address its own code and date.  It starts at
	   0x0000700000000000. We map only the first 64*2Meg of
	   kernel virtual memory */
	
	/* each entry has low order bits of 0183 which means:
	   1=P(resent), 2=R/W(rite), and 80=2Meg page size, and
	   100=G(lobal Page), these pages need not be flushed
	   from the TLB when we switch to user state */

	.quad	0x0000000000000183
	.quad	0x0000000000200183
	.quad	0x0000000000400183
	.quad	0x0000000000600183
	.quad	0x0000000000800183
	.quad	0x0000000000A00183
	.quad	0x0000000000C00183
	.quad	0x0000000000E00183
	.quad	0x0000000001000183
	.quad	0x0000000001200183
	.quad	0x0000000001400183
	.quad	0x0000000001600183
	.quad	0x0000000001800183
	.quad	0x0000000001A00183
	.quad	0x0000000001C00183
	.quad	0x0000000001E00183
	.quad	0x0000000002000183
	.quad	0x0000000002200183
	.quad	0x0000000002400183
	.quad	0x0000000002600183
	.quad	0x0000000002800183
	.quad	0x0000000002A00183
	.quad	0x0000000002C00183
	.quad	0x0000000002E00183
	.quad	0x0000000003000183
	.quad	0x0000000003200183
	.quad	0x0000000003400183
	.quad	0x0000000003600183
	.quad	0x0000000003800183
	.quad	0x0000000003A00183
	.quad	0x0000000003C00183
	.quad	0x0000000003E00183
	.quad	0x0000000004000183
	.quad	0x0000000004200183
	.quad	0x0000000004400183
	.quad	0x0000000004600183
	.quad	0x0000000004800183
	.quad	0x0000000004A00183
	.quad	0x0000000004C00183
	.quad	0x0000000004E00183
	.quad	0x0000000005000183
	.quad	0x0000000005200183
	.quad	0x0000000005400183
	.quad	0x0000000005600183
	.quad	0x0000000005800183
	.quad	0x0000000005A00183
	.quad	0x0000000005C00183
	.quad	0x0000000005E00183
	.quad	0x0000000006000183
	.quad	0x0000000006200183
	.quad	0x0000000006400183
	.quad	0x0000000006600183
	.quad	0x0000000006800183
	.quad	0x0000000006A00183
	.quad	0x0000000006C00183
	.quad	0x0000000006E00183
	.quad	0x0000000007000183
	.quad	0x0000000007200183
	.quad	0x0000000007400183
	.quad	0x0000000007600183
	.quad	0x0000000007800183
	.quad	0x0000000007A00183
	.quad	0x0000000007C00183
	.quad	0x0000000007E00183
	.fill	448,8,0

	.balign  4096  /* page aligned */



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

	.data

	.balign 16
pGDT64:
	.word	GdtEnd-Gdt    /* length */
	.quad	Gdt	         /* table address */
	

/* We need valid kernel segments for data and code in long mode too
 * IRET will check the segment types  kkeil 2000/10/28
 * Also sysret mandates a special GDT layout 
 */
		 		
	.balign 64 /* cacheline aligned, keep this synchronized with asm/desc.h */
C_DATA_ENTRY(Gdt)
	.quad	0x0000000000000000	/* This one is magic */
	.quad	0x0000000000000000	/* unused */
	.quad	0x00af9a000000ffff	/* KERNEL_CS */
	.quad	0x00cf92000000ffff	/* __KERNEL_DS */
	.quad	0x00cffe000000ffff	/* __USER32_CS */
	.quad	0x00cff2000000ffff	/* __USER_DS, __USER32_DS  */		
	.quad	0x00affa000000ffff	/* __USER_CS */
	.quad	0, 0			/* __TSS */
C_DATA_LABEL(GdtEnd)

	.balign  64


