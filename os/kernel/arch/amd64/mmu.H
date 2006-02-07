#ifndef __MMU_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __MMU_H_ */

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mmu.H,v 1.11 2001/11/12 21:45:47 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: mmu and other parapharnelia of the amd64
 * **************************************************************************/

/* In what follows legacy mode means the traditional ia32 mode, while
 * in long mode there are 2 submodes, compatibility (sub)mode
 * (application level only) or 64 bit (sub)mode for both applications
 * and the kernel (the only mode for the kernel... unless legacy).
 *
 * In long mode which is turned on/off by a separate mechanism the
 * segment descriptors can be divided in 3 classes.
 *	- vanilla segment descriptors, to be loaded in the code, data or stack
 * segments registers. Data and stack segment registers will be loaded normally
 * but their content ignored, only the code segment descriptor register is
 * used, and only its DPL, and the L bit and D fields are significant.
 *
 * The L bit (previously a reserved bit) set to 1 indicates 64 bit mode, and
 * the D bit must then be zero (by default the data size is 32 bit).
 *
 * If the L bit is zero this indicates compatibility mode and the D bit specify
 * the data size like in legacy mode.
 * Therefore those segment descriptord remain essentially unchanged, in particular
 * remain 32 bit long.
 *
 *	- system segment registers, i.e. call gate, trap gate (not used in our design)
 * ldt (not used in our design), interrupt gate and tss segment descriptors registers
 * are all expanded to 64 bits.
 * Task gate disappear in long mode and are not used here.
 * (Pseudo) system descriptors loaded into the GDTR, IDTR and TR's are expanded in
 * 64 bit mode (that is the only time they may be used, at least loaded). The tss
 * in long mode is common for 32 compatibility and 64 bit.
 *
 *	- FS and GS segment descriptors are treated specially. Their content is not
 * ignored in long mode and being expanded to 64 bits they can be used to access
 * quickly thread, task control block or per processr private data.
 *
 * When switching from legacy mode to long mode the GDTR, IDTR and TR must be reloaded
 * with the 64 bit versions, avoiding exceptions or interrupts until at least
 * reloading the IDTR.
 *
 * THIS DOES NOT ADDRESS COMPATIBILITY MODE XXX
 */

// VVV

/*******************************************************************************
  Paging Data Structures - 64 bit
  */

/* the PTE, PDE, PDP and PML4 have similar formats, but enough differences
 * in the reserved fields that they cannot be nested (too bad).
 * valid PDE, PDP, PML4 entries  will have US_bit set to 1, G_bit, PS_bit
 * D_bit set to 0.
 * This uses only 4KB pages.
 * But note that the kernel and bss are mapped w/ 2MB pages.
 * Frame mask corresponds to 40 bit real.
 */

#define L_PTE(level)							\
    class L ## level ## _PTE {						\
public:									\
    uval P:1;				/* valid */			\
    uval RW:1;				/* write if 1 */		\
    uval US:1;				/* user  if 1 */		\
    uval PWT:1;				/* write through if 1 */	\
									\
    uval PCD:1;				/* cache disabled if 1 */	\
    uval A:1;				/* accessed */			\
    uval D:1;				/* dirty (i.e.modified) */	\
    uval PS:1;				/* reserved, set to 0 */	\
									\
    uval G:1;				/* global if 1 */		\
    uval SFTW_1:3;			/* available to software */	\
    uval Frame:28;			/* real address of page */	\
    uval ReservedZero:12;		/* reserved, must be 0 */	\
									\
    uval SFTW_2:12;			/* available to software */	\
									\
/* also define bits needed for efficient code in many places */		\
									\
    enum PTEBits {							\
	P_bit = 0x1,							\
	RW_bit = 0x2,							\
	US_bit = 0x4,							\
	PWT_bit = 0x8,							\
	PCD_bit = 0x10,							\
	A_bit = 0x20,							\
	D_bit = 0x40,							\
	PS_bit = 0x80,							\
	G_bit = 0x100,							\
	Frame_mask = 0xfffffff000UL,					\
	EntriesPerPage = 512,						\
	LogEntriesPerPage = 9,						\
    };									\
    enum {								\
	userFault = 4,							\
	writeFault = 2,							\
	protectFault = 1,						\
    };									\
} __attribute__((packed))

/* instantiate the generic classes
 */
L_PTE(1);
L_PTE(2);
L_PTE(3);
L_PTE(4);

#define PAGE_ADDRESS_SHIFT  12

/* equivalence with the x86-64 terminology
 */
#define PML4	L1_PTE
#define PDP	L2_PTE
#define PDE	L3_PTE
#define PTE	L4_PTE


/* these macros provides the virtual addresses of the various level ptes
 * for a given virtual address, gieven the address of the top level page.
 * Real frames have a fixed virtual address in the kernel address space. The physical
 * frame address found at each level entry  is converted to the virtual address of the next
 * level page and the offset to the entry is added.
 */
#define VADDR_TO_PML4_SHIFT		(LOG_SEGMENT_SIZE + PDP::LogEntriesPerPage + PDE::LogEntriesPerPage)
#define VADDR_TO_PDP_SHIFT		(LOG_SEGMENT_SIZE + PDE::LogEntriesPerPage)
#define VADDR_TO_PDE_SHIFT		(LOG_SEGMENT_SIZE)

#define DIRECTORY_INDEX_MASK		(PDE::EntriesPerPage - 1)	// common to all levels XXX
#define INVALID_PT_ADRESS		(0)

/* needs an early version
 */

#define EARLY_VADDR_TO_L1_PTE_P(topaddr, vaddr, memory)                                                      \
        ((L1_PTE *)(topaddr) +                                                                 \
        (((vaddr) >> VADDR_TO_PML4_SHIFT) & DIRECTORY_INDEX_MASK))

#define EARLY_VADDR_TO_L2_PTE_P(topaddr, vaddr, memory)                                                       \
        ((L2_PTE *) ((memory)->virtFromPhys(							\
        EARLY_VADDR_TO_L1_PTE_P((topaddr), (vaddr), memory)->Frame << PAGE_ADDRESS_SHIFT))                   \
        + (((vaddr) >> VADDR_TO_PDP_SHIFT) & DIRECTORY_INDEX_MASK))

#define EARLY_VADDR_TO_L3_PTE_P(topaddr, vaddr, memory)                                                       \
        ((L3_PTE *) ((memory)->virtFromPhys(							\
        EARLY_VADDR_TO_L2_PTE_P((topaddr), (vaddr), (memory))->Frame << PAGE_ADDRESS_SHIFT))                    \
        + (((vaddr) >> LOG_SEGMENT_SIZE) & DIRECTORY_INDEX_MASK))


#define EARLY_VADDR_TO_L4_PTE_P(topaddr, vaddr, memory)                                                               \
        ((L4_PTE *) ((memory)->virtFromPhys(							\
        EARLY_VADDR_TO_L3_PTE_P((topaddr), (vaddr), (memory))->Frame << PAGE_ADDRESS_SHIFT))                    \
        + ((vaddr >> PAGE_ADDRESS_SHIFT) & DIRECTORY_INDEX_MASK))

#define TOUCH(x)        	(*(volatile char *)(x))
#define TOUCH_WRITE(x)        	{uval touch = *(volatile char *)(x); *(volatile char *)(x) = touch;}

#define __flush_tlb_one(addr) __asm__ __volatile__("invlpg %0": :"m" (*(char *) addr))
