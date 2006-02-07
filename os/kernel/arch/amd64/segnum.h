/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: segnum.h,v 1.6 2001/11/27 19:44:51 pdb Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: segment descriptors for addressing of the amd64
 * **************************************************************************/

/* There are places where we still need to refer to the code
   segment (CS), data segment (DS), and/or stack segments (SS)
   used for semented addressing on the x86 architecture.  These
   may be in assembly code, C code, or C++ code, so we use a
   simple #define structure.  */


/*	Conventional GDT indexes.

	N.B. The first three indexes are owned by firmware (that does not seem 
        to be the case anymore ?)  and must be valid whenever firmware is called. 
	In 64 bit mode segment descriptors remain 8 bytes except the TSS to  
	accomodate the larger addresses.
	THESE DEFINES  MUST MATCH THE DEFINITION OF Gdt IN START.S
*/

#define MAGIC_NULL	((0 << 3) + 0)	// forbidden
#define FIRMWARE_CS	((1 << 3) + 0)	// ring 0

#define KERNEL_CS	((2 << 3) + 0)	// ring 0
#define KERNEL_SS	((3 << 3) + 0)	// ring 0, must follow KERNEL_CS
#define USER32_CS	((4 << 3) + 3)	// ring 3, __USER32_CS
#define USER_SS		((5 << 3) + 3)	// ring 3, (must follow USER_CS), __USER_DS, __USER32_DS
#define USER_CS		((6 << 3) + 3)	// ring 3, __USER_CS

#define KERNEL_TR	((7 << 3) + 0)	// ring 0, attention for SMP  this one is 16 bytes long 

/* the number of 8 bytes entries in the gdt is 7 augmented with  2*number of TR's (one per cpu).
 */
#define NUM_GDT_ENTRIES 9


/*
 * The layout of the GDT under Linux for x86-64 for information:
 * it is not quite up to date even w/ the latest Linux ... for info
 *	- cache alignment
 *	- smp TRs
 *	- APM BIOS etc...
 *
 *   0 - null
 *   1 - not used
 *   2 - kernel code segment
 *   3 - kernel data segment
 *   4 - user code segment                  <-- new cacheline
 *   5 - user data segment
 *   6 - not used
 *   7 - not used
 *   8 - APM BIOS support                   <-- new cacheline
 *   9 - APM BIOS support
 *  10 - APM BIOS support
 *  11 - APM BIOS support
 *
 * The TSS+LDT descriptors are spread out a bit so that every CPU
 * has an exclusive cacheline for the per-CPU TSS and LDT:
 *
 *  12 - CPU#0 TSS                          <-- new cacheline
 *  13 - CPU#0 LDT
 *  14 - not used
 *  15 - not used
 *  16 - CPU#1 TSS                          <-- new cacheline
 *  17 - CPU#1 LDT
 *  18 - not used
 *  19 - not used
 *  ... NR_CPUS per-CPU TSS+LDT's if on SMP
 *
 * Entry into gdt where to find first TSS.

#define __FIRST_TSS_ENTRY 12
#define __FIRST_LDT_ENTRY (__FIRST_TSS_ENTRY+1)

#define __TSS(n) (((n)<<2) + __FIRST_TSS_ENTRY)
#define __LDT(n) (((n)<<2) + __FIRST_LDT_ENTRY)


 */
