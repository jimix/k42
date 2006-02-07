#ifndef __KERNEL_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __KERNEL_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernel.H,v 1.4 2001/10/05 21:49:44 peterson Exp $
 *****************************************************************************/

#ifndef __ASSEMBLER__
#include __MINC(amd64_hw.H)

/*
 * Symbol that marks the location to which the kernel was relocated by
 * linker.  The kernel will run correctly when placed at this address in
 * virtual memory.
 */
extern code kernVirtStart;

/* descriptor tables */
extern SegDesc Gdt[];
extern SysGate Idt[], IdtEnd[];
extern DTParam Idtr;

/* other routines */
void InitIdt();
void set_debug_traps(void);
void gdb_putchar(sval);

#endif /* #ifndef __ASSEMBLER__ */
