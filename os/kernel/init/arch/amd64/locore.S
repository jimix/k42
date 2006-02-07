/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: locore.S,v 1.1 2001/04/11 17:17:56 peterson Exp $
 *****************************************************************************/
#include <sys/kinclude.H>
#include <misc/hardware.H>

#include <misc/asm.h>
#include <misc/arch/amd64/asdef.h>
#include <sys/arch/amd64/asmConstants.H>

/************************************************************
 * Initial Translate On exception trap we have to get to
 ************************************************************/

TOC_C_TEXT_ENTRY(AsmInstallKernPageTablesPhys)

C_TEXT_ENTRY(AsmInstallKernPageTables)
	// arguments to this SC trap
	// R3:	 virtual address of physical 0
	// R4:	 value to assign to SDR1 (page table addr)
	// R5:	 msr value to be set
	// R6:	 sprg0 value to be set
	// R7:   value to assign to ASR (segment table addr)

	LOAD_C_TEXT_ADDR(r11,AsmInstallKernPageTablesPhys)
	sub	r11,r11,r3
	mtsrr0	r11
	li	r12,PSL_ME
	mtsrr1	r12
	rfid		// continue with address translate off
	tw	4,r0,r0
	// NOTREACHED

################################################################
#
#     VIRTUAL MODE      VIRTUAL MODE     VIRTUAL MODE
#
#  DEBUGGING:  This code will store 0 into the control reg for
#              the COM2 port, thus dropping the line.  This
#              (the dropping of the line) can be seen on the
#              service console via minicom, which shows that
#              it has gone "offline".  In the early stages of
#              boot on hardware, this is sometimes the only
#              debugging information to be had.
#
#hack_virtual_mode:
#	li	r1, 0xF8
#	extsb	r1, r1
#	sldi	r1, r1, 24
#	ori	r1, r1, 0x02FC
#	li	r0, 0
#	stb	r0, 0(r1)
#	eieio
#	sync
#################################################################

C_TEXT_END(AsmInstallKernPageTables)

C_TEXT_ENTRY(AsmInstallKernPageTablesPhys)
	mtsprg  sprg0,r6		// set physical elocal
	mtsdr1	r4			// set page table base
	mtasr	r7			// move segment table address (and V bit) to ASR
// FIXME-RICK-REVIEW  >>> tell jimi about instructions for gas
#                                       // tlbia not valid on RS/6000 model 260
#	.long	0x7C0002E4		// tlbia - discard anything that may be in TLB
	slbia				// discard anything that may be in SLB
	isync				// context synchronizing

	mflr	r0
	mtsrr0	r0
	mtsrr1	r5
	rfid                            // context-synchronizing

################################################################
#
#     REAL MODE     REAL MODE       REAL MODE       REAL MODE
#
#  DEBUGGING:  This code will store 0 into the control reg for
#              the COM2 port, thus dropping the line.  This
#              (the dropping of the line) can be seen on the
#              service console via minicom, which shows that
#              it has gone "offline".  In the early stages of
#              boot on hardware, this is sometimes the only
#              debugging information to be had.
#
#hack_real_mode:
#	li	r1, 0xF8
#	clrlsldi r1, r1, 56, 24
#	ori	r1, r1, 0x02FC
#	li	r0, 0
#	stb	r0, 0(r1)
#	dcbst	0,r1
#	eieio
#	sync
#################################################################

C_TEXT_END(AsmInstallKernPageTablesPhys)

CODE_LABEL(StartAdditionalCPU)
C_TEXT_LABEL(StartAdditionalCPU)
	/*
	 * We start here in real mode.
	 * Assumes r3 contains address of XferArea (see MPinit.H)
	 */

	/*
	 * The hardware segment table is in the XferArea,
	 * one 4KB page aligned on a page boundary and pinned.
	 * We make it our segment table by placing its address in the ASR.
	 */
	la	r7,XA_segTable(r3)	// pick up value for ASR
	clrrdi.	r7,r7,1			// clear the ASR[V] bit; test address for 0
	beq-	$+8			// skip next if segment table addr == 0
	  ori	  r7,r7,1		// else set the ASR[V] bit to 1
	isync			        // context-synchronizing instr per arch book
	mtasr	r7			// load value into the ASR
	isync                           // context-synchronizing

	ld	r7,XA_rSecondaryBootMem(r3) // real addr of secondary boot mem
	mtsdr1	r7			// initialize SDR1

	ld	r7,XA_iar(r3)
	mtsrr0	r7			// setup value for PC
	ld	r7,XA_msr(r3)		// bump r3 only have 5 bits for offset
	mtsrr1	r7			// setup value for MSR (turns on virt mode)
	ld	r1,XA_stackAddr(r3)	// set up stack value 40(r3)
	ld	r2,XA_toc(r3)		// set up toc value 48(r3)
	ld	r3,XA_secondaryBootMem(r3) // parameter value
	rfid
	tw	4,r0,r0
	// NOTREACHED
CODE_END(StartAdditionalCPU)

C_TEXT_ENTRY(marctest)
	li	r4,0
	mtdbatu 0,r4
	isync
	lis	r5, 0xF800		# Load upper half of address (FF60)
	rldicl	r5, r5, 0, 32		# clear upper part
	ori	r8, r5, 0x002A		# WIMG = 0101, PP=2 (r/w)
	mtdbatl	0, r8
	ori	r8, r5, 0x0002		# 128KB block length, Vs=1, Vp=0
	mtdbatu	0, r8
	mfmsr   r8
	ori     r8, r8, 0x10		# turn on data relocate
	mtmsrd  r8
	isync				# Ensure BATs loaded

	li	r4,0
	stb	r4, 0x3FC(r5)		# store value to LEDs
	eieio				# order store
	mtdbatu 0,r4
	isync
	blr
C_TEXT_END(marctest)
