/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: locore.S,v 1.89 2005/02/16 00:06:24 mergen Exp $
 *****************************************************************************/
#include <sys/kinclude.H>
#include <misc/hardware.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

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
	// R8:	 if 1, we're a partition, don't do tlbie
	LOAD_C_TEXT_ADDR(r11,AsmInstallKernPageTablesPhys)
	sub	r11,r11,r3
	mtsrr0	r11

	// even if not on HV setting this bit should be ignored
	li	r12,((PSL_SF|PSL_HV)>>50)
	sldi	r12,r12,50
	ori	r12,r12,PSL_ME
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

	// These two are supposed to be NOPs if we are on HV
CODE_LABEL(locore_sdr1_HV_patch)
	mtsdr1	r4			// set page table base
	slbia				// discard anything that may be in SLB
	ld	r9,EL_segLoad(r6)       // hardware 0
	cmpldi	r9,0			// compare with zero
	beq	hdwSLB			// branch if hardware SLB reload
	la	r9, ST_SLBCache-8(r7)     // start of cache array
	ld      r10, ST_cacheMax(r7)    // number to load
	mtctr	r10
0:	
	ldu	r11, 8(r9)              // get vsid 
	ldu     r12, 8(r9)              // esid words - index field is correct
	slbmte	r11,r12			// load SLB entry
	bdnz	0b
	b	clearTLB	        // go do isync
hdwSLB:
	mtasr	r7			// move segment table address
					// (and V bit) to ASR
clearTLB:	
	cmpldi	r8, 0			// clear TLB only if not on HV
	bne	finish	
	// Clear the TLB.  Nothing but small pages to worry about.
	// This is necessary because the new page table installed here may
	// contain large page entries for the same addresses that are still in
	// the TLB as small page entries from the boot page table.  This has
	// caused machine check on Mac 970s with some page table sizes.
	sync
	isync
	li	r11,256
	mtctr	r11
	li	r11,0
TLBIA_Loop:
	tlbie	r11
	addi	r11,r11,0x1000
	bdnz	TLBIA_Loop
	sync
	isync
finish:	
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
	 * Assumes r3 contains address of kernelInitArgs in kernel.H
	 */
	
       /* FIXME MAA 10/15/04
	* I found this code halfway through on one path.
	* Don't know if its relevant but if it were be done ...
	* Go to 64 right away
	*/
	mfmsr	r11			// or-in the SF to force 64-bit mode
	lis	r12, 0x8000
	sldi	r12, r12, 32
	or	r11, r11, r12
	mtmsrd	r11
	sync
	
	ld	r12, KA_relocal(r3)	//real address of new exceptionLocal
	ld	r11, EL_vMapsRDelta(r12) // v to r offset
	/*
	 * The hardware segment table is in the XferArea,
	 * one 4KB page aligned on a page boundary and pinned.
	 * We make it our segment table by placing its address in the ASR.
	 */
	ld	r7,EL_currentSegmentTable(r12) // pick up value for ASR 

	ld	r8, EL_segLoad(r12)	// check if we need to init SLB entries
	cmpldi	r8, 0
	slbia
	bne	software_slb		// software reload
	ld	r7, ST_segDescTable(r7)	// get vaddr of hardware segment table
	sub	r7, r7, r11		// virtual to real
	ori     r7,r7,1			// set the ASR[V] bit to 1
	mtasr	r7			// load value into the ASR
	b	SLB_done

software_slb:	
	ld	r9, ST_cacheMax(r7)	// number to load
	la	r8, ST_SLBCache-8(r7)	// cache entries to load
	mtctr	r9
1:	ldu	r9, 8(r8)		// vsid word
	ldu	r10, 8(r8)		// esid word
	slbmte	r9, r10			// load SLB entry
	bdnz	1b

    SLB_done:	
	mtsprg	sprg0, r12		// exceptionlocal
	ld	r7,KA_sdr1(r3)
	mtsdr1	r7			// initialize SDR1
	
	ld	r7,KA_iar(r3)
	mtsrr0	r7			// setup value for PC
	ld	r7,KA_msr(r3)		// bump r3 only have 5 bits for offset
	mtsrr1	r7			// new MSR (turns on virt mode)
	ld	r1,KA_stackAddr(r3)	// set up stack value 40(r3)
	ld	r2,KA_toc(r3)		// set up toc value 48(r3)
	add	r3,r3,r11		// real to virtual
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
