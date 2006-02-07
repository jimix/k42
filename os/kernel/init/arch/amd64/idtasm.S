/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *      
 *****************************************************************************/

// VVVVVVVVVVVVVVVVVVV from x86 VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV FIXME pdb 
// VVVVVVVVVVVVVVVVVVV from x86 VVVVVVVVVVVVVVVVVVVVVVV FIXME pdb  needs idt for 64bit mode and startup 32 bit.

#include <sys/kinclude.H>
#include <misc/asm.h>
#include "init/kernel.H"
#include <sys/arch/amd64/asmConstants.H>	
#include <arch/amd64/segnum.h>              // KERNEL_CS, ...
	
	.text
	/* this code pushes the int number.  for ints which do not
	push an error code, it pushes a zero before the int number so
	that frames have always the same layout, except for
		- the timer interrupt	104
		- I/O interrupts 	105 to 119
		- IPI interruptd	120
		- I/O interrupts	121 to 255
	which do not push error code nor NULL (but do push the interrupt
	number, the irq).

	Note that for each interrupt number the corresponding code start at
	an entry in an array of 255 entries (i.e. there is padding to
	compensate for the different sizes of code due to the extra pushq $0
	when needed).
	*/

i	=	0
j	=	32			// chose j by experiment, or just make it a bit large

C_TEXT_LABEL(int0)
1:	pushq	$0
	pushq	$i
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90
i	=	i + 1

	/* 1-7 to not push an error code */
	.globl	C_TEXT(int1)
C_TEXT_LABEL(int1)
	.rept	(8 - 1)
1:
	pushq	$0
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90
	.endr

	/* 8 does */
1:	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90

	/* 9 does not push an error code */
1:
	pushq	$0
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90

	/* 10-14 push error codes */
	.rept	(15 - 10)
1:
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90
	.endr

	/* 15-16 do not push an error code */
	.rept	(17 - 15)
1:
	pushq	$0
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90
	.endr

	/* 17 does */
1:	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90


	/* 18-103 do not push an error code */
	.rept	(104 - 18)
1:
	pushq	$0
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90
	.endr

	/* 104 does not push an error code; used for timer ints */
1:
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TimerInterrupt)
	.org	1b+j, 0x90

	/* 105-119 do not push an error code, are I/O interrupts */
	.rept	(120 - 105)
1:
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_IOInterrupt)
	.org	1b+j, 0x90
	.endr

	/* 120 does not push an error code; used for interprocessor ints */
1:
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_IPInterrupt)
	.org	1b+j, 0x90

	/* 121-255 do not push an error code */
	.rept	(256 - 121)
1:
	pushq	$0
	pushq	$i
i	=	i+1
	jmp	C_TEXT(ExceptionLocal_TrapException)
	.org	1b+j, 0x90
	.endr


	/* Interrupt Descriptor Table  data */

	.balign 	8
C_DATA_LABEL(Idtr)
	.word	C_DATA(IdtEnd) - C_DATA(Idt) - 1
	.quad	C_DATA(Idt)		// virtual address of Idt


IntGate0	=	KERNEL_CS*0x10000
IntGate1	=	0x00000e00	// Int Gate not present
IntGate1a	=	0x00008e00	// Int Gate present, DPL=0
IntGate1b	=	0x0000ee00	// Int Gate present, DPL=3
IntGate2	=	0x0		// offset 63:32
IntGate3	=	0x0		// reserved, ignored

	.balign 	16
C_DATA_ENTRY(Idt)
idt_table:	
	.long	IntGate0, IntGate1,  IntGate2, IntGate3	/*  0 - Divide Error */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  1 - Debug Exception */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  2 - NMI Interrupt */
	.long	IntGate0, IntGate1b, IntGate2, IntGate3	/*  3 - Breakpoint DPL=3*/
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  4 - INTO-detected Overflow */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  5 - BOUND Range Exceeded */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  6 - Inval Opcode */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  7 - Device Not Available */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  8 - Double Fault */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/*  9 - CoProcessor Segment Overrun */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 10 - Invalid Task State Segment */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 11 - Segment Not Present */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 12 - Stack Fault */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 13 - General Protection */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 14 - Page Fault */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 15 - reserved */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 16 - Floating-Point Error */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 17 - Alignment Check */
	.long	IntGate0, IntGate1a, IntGate2, IntGate3	/* 18 - Machine Check */
	.rept	237
	.long	IntGate0, IntGate1b, IntGate2, IntGate3  	/* additional int. gates, DPL=3 */
	.endr
C_DATA_LABEL(IdtEnd)
