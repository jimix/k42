/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *      
 *****************************************************************************/

/* VVVVVVVVVVVVVVVVVVV from x86 VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV FIXME pdb  */

#include <sys/kinclude.H>
#include "init/kernel.H"
#include <misc/asm.h>
#include <sys/arch/amd64/asmConstants.H>

#define ADDR_procnum     (procnum    - C_TEXT(secondaryStartVector))
#define ADDR_cr3         (cr3        - C_TEXT(secondaryStartVector))
#define ADDR_stackvaddr  (stackvaddr - C_TEXT(secondaryStartVector))
#define ADDR_startFlg    (startFlg   - C_TEXT(secondaryStartVector))
#define ADDR_gdtr        (limit      - C_TEXT(secondaryStartVector))
			
C_TEXT_ENTRY(secondaryStartVector)
	.code16
	cli
	
	// We know that cs points to the address of the 4k page
	// on which this code has been aligned via the copy
	// by the BSP.  Set segment registers to use the same
	// segment as cs.
	movw    %cs, %ax
	movw    %ax, %ds
	movw	%ax, %es	
	// Set our selfs up with a temporary real mode stack.
	// FIXME: Turned out not to needed.  Unless we want want to
	//        make function calls in real mode
	//movw	%ax, %ss
	//movw	$(boot_stack-_secondaryStartVector), %sp

	// set these to null
	movw	$0, %ax
	movw	%ax, %fs
	movw	%ax, %gs
	// FIXME: May want to move the data on to stack
	//        page that we are passing in.

	lgdtl ADDR_gdtr
	
	// page table pointer
	movl  ADDR_cr3, %eax
	movl  %eax, %cr3	
	
	//  enable protected and transltation
	movl  %cr0, %eax
	orl   $(CR0_PE_bit|CR0_WP_bit|CR0_PG_bit), %eax
	movl  %eax, %cr0

	
	// Set cs via a long jump
	// FIXME: 0x20 really should be ???
	ljmp	$0x20, $1f

1:		
	.code32
	// moving stackpointer before changing data segment
	movl    ADDR_stackvaddr, %esp

	// set start flag to a magic value to ack done with struct
	movw $0xbe03, %ax	
	movw %ax, ADDR_startFlg
	
	//Set data segments appropriately
	//FIXME:  0x28 really should be ??
	movw    $0x28, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	
	call	C_TEXT(secondaryStart)
	hlt

//	.space 0x100   /* space for temporary stack */
//boot_stack:

	//  FIXME:    USE GENCONSTANTS!!!!!! For all
	//            of this.
	//  WARNING:  The structure of this data
	//            has been constructed by hand
	//            the MPinit code relies on this
	//            layout:
	//    struct secondaryStartVectorData {
	//	  uval32   procnum PACKED; 
	//        uval32   cr3 PACKED;
	//	  uval  stackvaddr PACKED;
	//        volatile uval16  startFlg PACKED;
	//        struct {
	//    	     uval16 limit PACKED;
	//    	     uval32 base PACKED;
	//        } gdtr PACKED;
	//    };
	
C_TEXT_LABEL(secondaryStartVectorData)
procnum:
	.long  0x0
cr3:
	.long  0x0
stackvaddr:
	.long  0x0
startFlg:
	.word  0x0
limit:	
	.word  0x0
base:	
	.long  0x0	
vectorEnd:		
	
C_TEXT_LABEL(secondaryStartVectorSize)
	.long  vectorEnd - C_TEXT(secondaryStartVector)
