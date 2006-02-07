/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lolita.S,v 1.4 2003/10/28 21:12:34 rosnbrg Exp $
 *****************************************************************************/
#include <sys/kinclude.H>
#include <sys/arch/amd64/asmConstants.H>
#include <sys/syscalls.H>
#include <misc/hardware.H>

#include <misc/asm.h>
#include <misc/arch/amd64/asdef.h>
#include <misc/arch/amd64/trap.h>

#define SREG_INVAL	0x0		// software invalid sreg value


/*
 * -------------------------------------------------------------
 *         LOw Level Interface To Assembler  (LOLITA)
 * -------------------------------------------------------------
 *
 *	General Register Usage
 *	sprg0:	 physical address of exceptionLocal
 *      sprg1:	 physical address of current low-level save area
 *	sprg2:	 scratch (usually used to preserve r13)
 *	sprg3:	 scratch (usually used to preserve r14)
 */

/*
 * This code fragment is copied to the system-call exception handler address
 * (see init/arch/amd6432/initKernelMappings.C).
 */
CODE_ENTRY(lolita_exc_sc)
	/*
	 * On entry, r2 contains the svc index and r12 is available.
	 * We corrupt these two registers and pass all others intact to the
	 * individual system call handler (which may or may not preserve
	 * them).  Note that we do not pass along either the original iar
	 * (currently in srr0) or the original msr (currently in srr1).
	 *
 	 * Entered via hardware interrupt with:
 	 *    MSR.EE=0   Exernal interrupts disabled
         *    MSR.IR=0   Instruction relocate off (I-fetch uses REAL addresses)
 	 *    MSR.DR=0   Data relocate off (loads & stores use REAL addresses)
	 */
	mfsprg	r12,sprg0		// r12 <- &exceptionLocal (real)
	rlwinm	r11,r0,ROT_RIGHT_32(16-3),SHIFT_L((SYSCALL_LIMIT-1),3)
					// svc offset = (idx & mask) << 3
	addi	r11,r11,EL_svc		// r11 = svc[idx] offset in excLocal
	ldx	r11,r12,r11		// r11 <- exceptionLocal.svc[idx]
	mtsrr0	r11			// iar for rfid
	ld	r2,EL_toc(r12)		// set up kernel TOC for rfid
	ld	r12,EL_msr(r12)		// r12 <- kernel-mode msr
	mtsrr1	r12			// msr for rfid
	rfid				// continue at exception level
CODE_END(lolita_exc_sc)
CODE_LABEL(lolita_exc_sc_end)

CODE_ENTRY(lolita_missing_exc)
	.long 0x7C0007CE
CODE_END(lolita_missing_exc)
CODE_LABEL(lolita_missing_exc_end)

/*
 * This code fragment is copied to each exception-handler address for which
 * the full machine state must be preserved.  The load from the EL_handlers
 * array is patched specifically in each copy to load the appropriate entry
 * from the array (see init/arch/amd6432/initKernelMappings.C).
 *
 * Exception handlers are entered via hardware interrupt with:
 *    MSR.EE=0   Exernal interrupts disabled
 *    MSR.IR=0   Instruction relocate off (I-fetch uses REAL addresses)
 *    MSR.DR=0   Data relocate off (loads & stores use REAL addresses)
 */
CODE_ENTRY(lolita_involuntary_exc)
        mtsprg  sprg2,r13		// preserve r13
        mfsprg  r13,sprg0		// r13 <- &exceptionLocal (real)
        mtsprg  sprg3,r14		// preserve r14
	std	r7,EL_r7(r13)		// preserve r7

CODE_LABEL(lolita_involuntary_exc_ldhdlr)
	ld	r14,EL_handlers(r13)	// PATCHED to load correct entry
	mfctr   r7			// preserve ctr
	mtctr	r14			// load handler into ctr for branch
	bctr				// branch to exception handler
CODE_END(lolita_involuntary_exc)

CODE_LABEL(lolita_involuntary_exc_end)

#define INC_COUNTER(offset,baseReg,scratchReg)\
	ld	scratchReg,(offset)(baseReg);\
	addi	scratchReg,scratchReg,1;\
	std	scratchReg,(offset)(baseReg)

/*
 * -------------------------------------------------------------
 *  Null Interrupt - for use during initialization
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_null_handler)
    /*
     * Entered with interrupts disabled, REAL addressing mode
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */

	mtctr	r7
	ld	r7,EL_r7(r13)
	mfsprg  r14,sprg3
	mfsprg  r13,sprg2
	rfid
CODE_END(exc_null_handler)

/*
 * -------------------------------------------------------------
 *  Instruction Storage Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_isi_handler)
    /*
     * Entered with interrupts disabled, REAL addressing mode
     *
     * SRR1 says why we're here:
     *
     *        0x0000000040000000  page table miss
     *        0x0000000010000000  fetch from direct-store
     *        0x0000000008000000  protection violation
     *        0x0000000000200000  segment table miss
     *        0x00000000201F0000  should all be 0
     *        0xFFFFFFFF87C0FFFF  copied from MSR
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_isi,r13,r14)

	std	r6,EL_r6(r13)		// preserve r6
	std	r5,EL_r5(r13)		// preserve r5
	std	r4,EL_r4(r13)		// preserve r4
	mfcr	r6			// preserve cr
        mfsrr1  r4			// get fault info from SRR1
	andis.  r5,r4,0x0020            // check for
	cmpdi   cr1,r5,0		//   segment table miss
        andis.  r14,r4,0x1800		// isi other than page translation?
	mfsrr0  r5			// get fault address from SRR0
	bne	cr0,pageFault		// unknown isi exception
	bne     cr1,segmentMiss         // branch if miss in segment table
	b	pgfltCommon             // branch if miss in page table
CODE_END(exc_isi_handler)

/*
 * -------------------------------------------------------------
 *  Data Storage Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_dsi_handler)
    /*
     * Entered with interrupts disabled, REAL addressing mode
     *
     * DSISR indicates why we're here:
     *        0x80000000  Load or store -> direct-store exception
     *        0x40000000  Page table miss
     *        0x08000000  Protection violation
     *        0x04000000  eciwx etc. to direct-store or write-thru
     *        0x02000000  1 => store, 0 => load
     *        0x00400000  DABR
     *        0x00200000  Segment table miss
     *        0x00100000  eciwx or ecowx with EAR.E == 0
     *        0x318FFFFF  should all be 0
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_dsi,r13,r14)

	std	r6,EL_r6(r13)		// preserve r6
	std	r5,EL_r5(r13)		// preserve r5
	std	r4,EL_r4(r13)		// preserve r4
	mfcr	r6			// preserve cr
        mfdsisr r4			// get fault info from DSISR
	andis.  r5,r4,0x0020            // check for
	cmpdi   cr1,r5,0		//    segment table miss
        andis.  r14,r4,0x8c50		// dsi other than page or segment miss?
	mfdar   r5			// get fault address from DAR
	bne-	cr0,pageFault		// unknown dsi exception
	beq+	cr1,pgfltCommon		// branch if miss in page table
					// fall thru if miss in segment table

segmentMiss:
    /*
     * We're here because address translation could not find a segment table entry
     * for an I-fetch or a data load or store.
     *
     * The effective address space above KERNEL_BOUNDARY (typically defined to be
     * 0x8000000000000000) belongs to the kernel.
     *
     * For addresses below this (in user space), we continue with normal page fault
     * processing to resolve the missing segment table entry.  Either the proper
     * segment table entry will be loaded into the table and the faulting program
     * restarted, or the reference will be determined to be to an invalid address.
     *
     * For addresses in kernel space, we manufacture the appropriate segment table
     * entry here and install it in the segment table.  We extract the Effective
     * Segment ID (ESID) from the faulting Effective Address (virtual address in
     * K42 parlance), and from it we compute the Virtual Segment ID (VSID).
     * These two values plus some flags and protection keys make up the segment
     * table entry.
     *
     * The ESID of a faulting address consists of the high-order 36 bits of the
     * 64-bit address.  For addresses in kernel space, the 52-bit VSID consists
     * of the ESID shifted right 16 bits with the vacated 16 bits filled with 0x7FFF.
     * (The high order bit of the VSID is used as a lock bit in page table entries.)
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */
	INC_COUNTER(EL_num_segmiss,r13,r14)

	/* .long 0x7C0007CE */

// FIXME-RICK-REVIEW  >>> re-arrange code
	lis	r14, KERNEL_BOUNDARY > 48 // top 16 bits of kernel/user boundary
	ori	r14, r14, (KERNEL_BOUNDARY > 32) & 0xFFFF // next-to-top 16 bits
	sldi	r14, r14, 32            // shift into top 32 bits
	cmpld	r5,r14			// compare address to kernel/user boundary
	blt	pageFault               // addr below kernel; handle as page fault
	INC_COUNTER(EL_num_ksegmiss,r13,r14)
	std	r8,EL_r8(r13)		// save r8

	mfasr	r4			// get real address of segment table
	clrrdi	r4,r4,1			// clear the ASR[V] bit (purify the address)
	extrdi	r14,r5,5,31		// extract 5-bit congruence class, shift
	sldi	r14,r14,7		//   to be offset of start of STEG
	add	r14,r4,r14		// compute address of first STE,
	subi	r14,r14,16		//   less 16 to allow for update-form load
	li	r4,8
	mtctr	r4
L2:
	ldu	r4,16(r14)		// load first 8 bytes of STE
	andi.	r4,r4,0x80		// test V (valid) bit
	beq	L3			// branch if available (bit off)
	bdnz	L2			// loop thru all 8 entries

	// End of loop, no available STE found.  Pick one at random to evict.
	// Grab 3 "random" bits from bits 50..52 of the real time clock (time base register),
	// insert them into bits 57..59 of the STE pointer, thus specifying one of the STEs.
	// The "rlwimi" instr works in our 64-bit context, because the source and destination
	// fields are within the lower 32 bits; the upper 32 bits are not affected.

	mftb	r4			// get clock value
	rlwimi	r14,r4,28,0x70		// insert 3 bits from clock into pointer

	// We now have a pointer to an STE, which may or may not be occupied.
	// Lock the entry.  Then, if it's occupied, we must purge it before reusing it.
L3:
	ldarx	r4,0,r14		// load-and-reserve hi-order part
	ori	r8,r4,1			// make copy, with lock bit set
	stdcx.	r8,0,r14		// conditionally store lock bit
	bne	L3			// spin if store didn't happen
	cmpd	r8,r4			// see if value loaded had lock bit set
	beq	L3			// spin if someone else holds lock

	// Now we own the lock on this STE; see if it's occupied.

	andi.	r4,r4,0x80              // test V bit
	beq	L6			// don't invalidate if V != 1

	// Invalidate the STE pointed to by r14

	li	r4,1			// clear all but the lock bit in the
	std	r4,0(r14)		//   first 8 bytes of the STE
	sync				// sync
	slbie	r8			// kill off lookaside buffer entry, if any
	sync				// sync

L6:
	// r14 now points to the STE where we'll create our kernel segment table entry.
	// Compute the STE contents from the effective address in r5.

        // Second 8 bytes of segment table entry in r4
	sradi	r4,r5,16		// VSID = 0x7FFF concat ESID
	clrrdi	r4,r4,12
	clrldi	r4,r4,1
	std	r4,8(r14)		// store second half of STE
	sync				// sync

        // First 8 bytes of segment table entry in r1
	clrrdi	r4,r5,28		// isolate ESID in top 36 bits
	ori	r4,r4,0x90		// set V=1, T=0, Ks=0, Kp=1
	std	r4,0(r14)		// store first half, with V=1 and lock reset

	// Resume execution

	mtcr	r6			// restore cr
	mtctr	r7			// restore ctr
	ld	r4,EL_r4(r13)		// restore r4
	ld	r5,EL_r5(r13)		// restore r5
	ld	r6,EL_r6(r13)		// restore r6
	ld	r7,EL_r7(r13)		// restore r7
	ld	r8,EL_r8(r13)		// restore r8
	mfsprg	r14,sprg3		// restore r14
	mfsprg  r13,sprg2		// restore r13
	rfid				// resume

pgfltCommon:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */
	/* .long 0x7C0007CE */
	ld	r14,EL_mapFaultBoundary(r13)// check fault address against
	cmpld	r5,r14			//         mapping-fault boundary
	bge	mappingFault		// anything above is a mapping fault
	extrdi	r14,r4,1,58		// move the bit indicating "segment miss"
	insrdi	r4,r14,1,63		//   from 0x20 position to bottom bit, where
	sldi	r14,r14,5		//   later code expects it
	xor	r4,r4,r14		// turn off the 0x20 bit, if on

pageFault:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */

	mfsprg	r14,sprg1		// r14 <- current low-level save area

	std	r3,VS_r3(r14)		// save r3
	mfsrr1	r3			// pick up original msr
	std	r2,VS_r2(r14)		// save r2
	mfsrr0	r2			// pick up original iar
	std	r0,VS_r0(r14)		// save r0
	andi.	r0,r3,PSL_PR		// test for kernel-mode page fault
	std	r1,VS_r1(r14)		// save r1

	beq	pageFaultKernel		// branch if kernel-mode page fault

	// user-mode page fault
	std	r8,VS_r8(r14)		// save r8-r12
	std	r9,VS_r9(r14)
	std	r10,VS_r10(r14)
	std	r11,VS_r11(r14)
	std	r12,VS_r12(r14)

	std	r2,VS_iar(r14)		// save original msr, iar
	std	r3,VS_msr(r14)

	mflr	r8			// pick up (original) lr, xer
	mfxer	r9

	// prepare values for rfid
	ld	r2,EL_excPgfltExceptionUser(r13)
	ld	r3,EL_msr(r13)

	std	r6,VS_cr(r14)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r14)
	std	r8,VS_lr(r14)
	std	r9,VS_xer(r14)

	mtsrr0	r2			// set iar for rfid
	mtsrr1	r3			// set msr for rfid

	ld	r6,EL_r4(r13)		// pick up original values of r4-r7
	ld	r7,EL_r5(r13)
	ld	r8,EL_r6(r13)
	ld	r9,EL_r7(r13)

	ld	r2,EL_toc(r13)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r13)	// load delta for R-to-V conversion

	mfsprg  r13,sprg2		// restore r13

	std	r6,VS_r4(r14)		// save original values of r4-r7
	std	r7,VS_r5(r14)
	std	r8,VS_r6(r14)
	std	r9,VS_r7(r14)

	add	r1,r14,r0		// construct V->R address of PS

	mfsprg	r14,sprg3		// restore r14

	rfid

pageFaultKernel:

	ld	r0,EL_vMapsRDelta(r13)	// load delta for V-to-R conversion
	// move stk ptr past current floor and make room for VolatileState
	la	r1,(STK_FLOOR-VS_SIZE)(r1)
	sub	r1,r1,r0		// convert stackptr to real

	std	r8,VS_r8(r1)		// save r8-r12
	std	r9,VS_r9(r1)
	std	r10,VS_r10(r1)
	std	r11,VS_r11(r1)
	std	r12,VS_r12(r1)

	std	r2,VS_iar(r1)		// save original msr, iar
	std	r3,VS_msr(r1)

	mflr	r8			// pick up lr, xer
	mfxer	r9

	// prepare values for rfid
	ld	r2,EL_excPgfltExceptionKernel(r13)
	ld	r3,EL_msr(r13)

	std	r6,VS_cr(r1)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r1)
	std	r8,VS_lr(r1)
	std	r9,VS_xer(r1)

	mtsrr0	r2			// set iar for rfid
	mtsrr1	r3			// set msr for rfid
	ld	r2,EL_toc(r13)		// set up kernel TOC for rfid

	ld	r6,EL_r4(r13)		// pick up original values of r4-r7
	ld	r7,EL_r5(r13)
	ld	r8,EL_r6(r13)
	ld	r9,EL_r7(r13)

	mfsprg  r13,sprg2		// restore r13

	std	r6,VS_r4(r1)		// save original values of r4-r7
	std	r7,VS_r5(r1)
	std	r8,VS_r6(r1)
	std	r9,VS_r7(r1)

	ld	r6,VS_r0(r14)		// retrieve original values of r0-r3
	ld	r7,VS_r1(r14)
	ld	r8,VS_r2(r14)
	ld	r9,VS_r3(r14)

	mfsprg	r14,sprg3		// restore r14

	std	r6,VS_r0(r1)		// save original values of r0-r3
	std	r7,VS_r1(r1)
	std	r8,VS_r2(r1)
	std	r9,VS_r3(r1)

	add	r1,r1,r0		// convert stackptr back to virtual

	rfid

mappingFault:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */

checkVMapsR:	// check the V->R range
 	ld	r14,EL_vMapsRStart(r13)	// load and compare to V->R start
	cmpld	r5,r14
	blt	checkKernelPSR		// less then start, check next range
	ld	r14,EL_vMapsREnd(r13)	// load and compare to V->R end
	cmpld	r5,r14
	bge	checkKernelPSR		// greater than end, check next range
	ld	r4,EL_vMapsRDelta(r13)	// load V->R adjustment
	li	r14,0x190		// bitset R=1,C=1 WIMG=0010 X=0 PP=00
	b	createPTEG

checkKernelPSR:	// check the kernelPSpecificRegion range
 	ld	r14,EL_kernelPSRStart(r13)// load and compare to PSR start
	cmpld	r5,r14
	blt	checkCommonPSR		// less then start, check next range
	ld	r14,EL_kernelPSREnd(r13)// load and compare to PSR end
	cmpld	r5,r14
	bge	checkCommonPSR		// greater than end, check next range
	ld	r4,EL_kernelPSRDelta(r13)// load PSR adjustment
	li	r14,0x190		// bitset R=1,C=1 WIMG=0010 PP=00
	b	createPTEG

checkCommonPSR:	// check the commonPSpecificRegion range
 	ld	r14,EL_commonPSRStart(r13)// load and compare to PSR start
	cmpld	r5,r14
	blt	pageFault		// less then start, real page fault
	ld	r14,EL_commonPSREnd(r13)// load and compare to PSR end
	cmpld	r5,r14
	bge	pageFault		// greater than end, real page fault
	ld	r4,EL_commonPSRDelta(r13)// load PSR adjustment
	// initial subrange of commonPSR is read-only
	// subrange above commonPSRWritable is writable
	ld	r14,EL_commonPSRWritable(r13)
	cmpld	r5,r14
					// assume read-only
	li	r14,0x193		// bitset R=1,C=1 WIMG=0010 PP=11
	blt	createPTEG
					// convert to writable
	li	r14,0x192		// bitset R=1,C=1 WIMG=0010 PP=10

createPTEG:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| protection bits
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| V-to-R delta
     */
	std	r3,EL_r3(r13)		// preserve r3

	sub	r4,r5,r4		// R = V - V-to-R delta
	rldimi	r4,r14,0,52		// concatenate rpn with protection bits
	// r4 now has the complete rpn word of the pte

	/*
	 * Find entry for faulting address in segment table.
	 * It should be present, otherwise we would have found our way
	 * to "segmentMiss" above.
	 */
	mfasr	r3			// get real address of segment table
	clrrdi	r3,r3,1			// clear the ASR[V] bit (purify the address)
	extrdi	r14,r5,5,31		// extract 5-bit congruence class, shift
	sldi	r14,r14,7		//   to be offset of start of STEG
	add	r14,r14,r3		// compute address of first STE,
	subi	r14,r14,16		//   less 16 to allow for update-form load
	li	r3,8
	mtctr	r3
L5:
	ldu	r3,16(r14)		// load first 8 bytes of STE (ESID, flags)
	mtcrf	0x02,r3			// move nibble containing V bit (0x80) into cr6
	bnl	cr6,L5			// loop if not a valid entry
	xor	r3,r3,r5		// see if ESID from segment table entry
	clrrdi.	r3,r3,28   		//   matches ESID from fault address
	bdnzf	eq,L5			// loop if no match and not end of STEG
	ld	r14,8(r14)		// load second 8 bytes of STE into r14 (VSID)

	rlwinm	r3,r5,20,16,31		// extract 16-bit virtual page number
	rlwinm	r5,r5,16,20,24          // get api into bits 52-56
	or	r5,r5,r14		// concatenate VSID into api
	ori	r5,r5,0x1		// turn on valid bit in VSID word
	// r5 now has the complete VSID word of the pte

	rldicl	r14,r14,52,25		// right-justify 39 bits of VSID
	xor	r14,r14,r3		// xor page index with VSID (hash)

	ld	r3,EL_pte(r13)		// load address of hashtable

	rldimi	r3,r14,7,46		// insert hash bits into hashtable addr
					// FIXME: assumes smallest hashtab
					//     patch this instruction for
					//     larger hashtables:
					//         rldimi r3,r14,7,46  - min
					//         rldimi r3,r14,7,45  - 2X
					//         rldimi r3,r14,7,44  - 4X
					//         rldimi r3,r14,7,43  - 8X
					//         ....
					//         rldimi r3,r14,7,18  - max
	// r3 now has the address of the relevant pte group

	li	r14,8			// load CTR with 8 for counting
	mtctr	r14

L4:	ld	r14,0(r3)		// load vsid-word
	rldicl.	r14,r14,1,62		// check for valid or lock bit on
	beq	storePTEG		// found a free pteg in prim
	la	r3,16(r3)		// to next entry
	bdnz	L4			// dec ctr and branch on zero

	/* FIXME if we ever use hardware change bits, we must
	 * do a tlbei/sync for the entry we evict to make sure
	 * the change bit is stored
	 * the evict counter is shared
	 * with InvertedPageTable.C
	 */
evictEntry:				// we must evict one entry
        /*
	 * Grab 3 random bits from the Time Base value; insert them into the
	 * the pteg address such that they are the entry number of a pte.
	 * Note that the "rlwimi" is appropriate, even in a 64-bit context like
	 * this.  It rotates a 32-bit quantity, the lower half of the TB, and
	 * inserts some bits from the rotated value into the lower half of
	 * the 64-bit target register, without disturbing the upper half of that reg.
	 */
	mftb	r14			// get current time base (clock) value
	la	r3,-128(r3)		// back to beginning of pteg
	rlwimi	r3,r14,27,25,27		// insert offset into pteg address
	ld	r14,0(r3)		// make sure its not locked
	rldicl.	r14,r14,1,63		// check for lock bit on
	beq+    storePTEG
	xori	r3,r3,0x10		// use a different one - it cant be
					// locked (this almost never happens)

storePTEG:
    /*
     *  At this point all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| vsid word of pte
     *	r4		| exceptionLocal.r4	| rpn word of pte
     *	r3		| exceptionLocal.r3	| pte address
     */
	std r5,0(r3)			// store pte
	std r4,8(r3)

	mtcr	r6			// restore cr
	mtctr	r7			// restore ctr

	ld	r3,EL_r3(r13)		// restore r3
	ld	r4,EL_r4(r13)		// restore r4
	ld	r5,EL_r5(r13)		// restore r5
	ld	r6,EL_r6(r13)		// restore r6
	ld	r7,EL_r7(r13)		// restore r7

	mfsprg	r14,sprg3		// restore r14
	mfsprg  r13,sprg2		// restore r13
	rfid				// resume
CODE_END(exc_dsi_handler)

/*
 * -------------------------------------------------------------
 *  External Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_exi_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_exi,r13,r14)
	mfsprg	r14,sprg1		// r14 <- current low-level save area

	std	r0,VS_r0(r14)		// save volatile registers
	std	r1,VS_r1(r14)
	std	r2,VS_r2(r14)
	std	r3,VS_r3(r14)
	std	r4,VS_r4(r14)
	std	r5,VS_r5(r14)
	std	r6,VS_r6(r14)
	// r7 currently in EL
	std	r8,VS_r8(r14)
	std	r9,VS_r9(r14)
	std	r10,VS_r10(r14)
	std	r11,VS_r11(r14)
	std	r12,VS_r12(r14)

	ld	r3,EL_r7(r13)		// retrieve original value of r7

	mfsrr0	r4			// pick up original iar
	mfsrr1	r5			// pick up original msr
	mfcr	r6			// pick up cr
	// ctr already in r7
	mflr	r8			// pick up lr
	mfxer	r9			// pick up xer

	// prepare values for rfid
	ld	r1,EL_excIOInterrupt(r13)
	ld	r2,EL_msr(r13)
	mtsrr0	r1			// set iar for rfid
	mtsrr1	r2			// set msr for rfid
	ld	r2,EL_toc(r13)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r13)	// load delta for R-to-V conversion

	mfsprg  r13,sprg2		// restore r13

	std	r3,VS_r7(r14)		// save r7
	std	r4,VS_iar(r14)		// save original iar, msr
	std	r5,VS_msr(r14)
	std	r6,VS_cr(r14)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r14)
	std	r8,VS_lr(r14)
	std	r9,VS_xer(r14)

	add	r1,r14,r0		// construct V->R address of PS

	mfsprg	r14,sprg3		// restore r14
	rfid
CODE_END(exc_exi_handler)

/*
 * -------------------------------------------------------------
 *  Decrementer Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_dec_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_dec,r13,r14)
	mfsprg	r14,sprg1		// r14 <- current low-level save area

	std	r0,VS_r0(r14)		// save volatile registers
	std	r1,VS_r1(r14)
	std	r2,VS_r2(r14)
	std	r3,VS_r3(r14)
	std	r4,VS_r4(r14)
	std	r5,VS_r5(r14)
	std	r6,VS_r6(r14)
	// r7 currently in EL
	std	r8,VS_r8(r14)
	std	r9,VS_r9(r14)
	std	r10,VS_r10(r14)
	std	r11,VS_r11(r14)
	std	r12,VS_r12(r14)

	ld	r3,EL_r7(r13)		// retrieve original value of r7

	mfsrr0	r4			// pick up original iar
	mfsrr1	r5			// pick up original msr
	mfcr	r6			// pick up cr
	// ctr already in r7
	mflr	r8			// pick up lr
	mfxer	r9			// pick up xer

	// prepare values for rfid
	ld	r1,EL_excDecInterrupt(r13)
	ld	r2,EL_msr(r13)
	mtsrr0	r1			// set iar for rfid
	mtsrr1	r2			// set msr for rfid
	ld	r2,EL_toc(r13)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r13)	// load delta for R-to-V conversion

	mfsprg  r13,sprg2		// restore r13

	std	r3,VS_r7(r14)		// save r7
	std	r4,VS_iar(r14)		// save original iar, msr
	std	r5,VS_msr(r14)
	std	r6,VS_cr(r14)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r14)
	std	r8,VS_lr(r14)
	std	r9,VS_xer(r14)

	add	r1,r14,r0		// construct V->R address of PS

	mfsprg	r14,sprg3		// restore r14
	rfid
CODE_END(exc_dec_handler)

/*
 * -------------------------------------------------------------
 *  Alignment Check Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_ali_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_trc,r13,r14)
	li	r14,EXC_ALI		// load trap number
	b	trap_common
CODE_END(exc_ali_handler)

/*
 * -------------------------------------------------------------
 *  Trace Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_trc_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_trc,r13,r14)
	li	r14,EXC_TRC		// load trap number
	b	trap_common
CODE_END(exc_trc_handler)

/*
 * -------------------------------------------------------------
 *  Program Check Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_pgm_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r13		| sprg2			| &exceptionLocal (real)
     *	r14		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_pgm,r13,r14)
	li	r14,EXC_PGM		// load trap number
	// fall through

#define EL_TVS(elReg,psOffset) (EL_trapVolatileState+(psOffset))(elReg)

trap_common:
	std	r6,EL_TVS(r13,VS_r6)	// save r6-r3 (at least temporarily)
	std	r5,EL_TVS(r13,VS_r5)
	std	r4,EL_TVS(r13,VS_r4)
	std	r3,EL_TVS(r13,VS_r3)

	mfcr	r6			// pick up cr

	mfsrr1	r5			// pick up original msr
	mfsrr0	r4			// pick up original iar

	andi.	r3,r5,PSL_PR		// branch if kernel-mode trap
	beq	trapKernel

	mr	r3,r14			// move trapNumber to architected reg
	mfsprg	r14,sprg1		// r14 <- current low-level save area

	std	r12,VS_r12(r14)		// save volatile registers
	std	r11,VS_r11(r14)
	std	r10,VS_r10(r14)
	std	r9,VS_r9(r14)
	std	r8,VS_r8(r14)
	// r7 currently in EL
	// r6-r3 currently in EL_TVS
	std	r2,VS_r2(r14)
	std	r1,VS_r1(r14)
	std	r0,VS_r0(r14)

	mflr	r8			// pick up lr,xer
	mfxer	r9

	std	r4,VS_iar(r14)		// save original iar,msr
	std	r5,VS_msr(r14)
	std	r6,VS_cr(r14)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r14)
	std	r8,VS_lr(r14)
	std	r9,VS_xer(r14)

	// prepare values for rfid
	ld	r4,EL_excTrapExceptionUser(r13)
	ld	r5,EL_msr(r13)

	ld	r6,EL_TVS(r13,VS_r3)	// retrieve original values of r3-r7
	ld	r7,EL_TVS(r13,VS_r4)
	ld	r8,EL_TVS(r13,VS_r5)
	ld	r9,EL_TVS(r13,VS_r6)
	ld	r10,EL_r7(r13)

	mtsrr0	r4			// set iar for rfid
	mtsrr1	r5			// set msr for rfid
	ld	r2,EL_toc(r13)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r13)	// load delta for R-to-V conversion

	mfsprg  r13,sprg2		// restore r13

	std	r6,VS_r3(r14)		// save r3-r7
	std	r7,VS_r4(r14)
	std	r8,VS_r5(r14)
	std	r9,VS_r6(r14)
	std	r10,VS_r7(r14)

	add	r1,r14,r0		// construct V->R address of PS

	mfsprg	r14,sprg3		// restore r14

	rfid

trapKernel:
	mr	r3,r14			// move trapNumber to architected reg
	mfsprg	r14,sprg3		// restore r14

	std	r12,EL_TVS(r13,VS_r12)	// save volatile registers
	std	r11,EL_TVS(r13,VS_r11)
	std	r10,EL_TVS(r13,VS_r10)
	std	r9,EL_TVS(r13,VS_r9)
	std	r8,EL_TVS(r13,VS_r8)
	// r7 currently in EL
	// r6-r3 already saved in EL_TVS
	std	r2,EL_TVS(r13,VS_r2)
	std	r1,EL_TVS(r13,VS_r1)
	std	r0,EL_TVS(r13,VS_r0)

	mflr	r8			// pick up lr,xer
	mfxer	r9
	ld	r10,EL_r7(r13)		// pick up original r7

	std	r4,EL_TVS(r13,VS_iar)	// save original iar,msr
	std	r5,EL_TVS(r13,VS_msr)

	// prepare values for rfid
	ld	r4,EL_excTrapExceptionKernel(r13)
	ld	r5,EL_msr(r13)

	std	r6,EL_TVS(r13,VS_cr)	// save cr, ctr, lr, xer
	std	r7,EL_TVS(r13,VS_ctr)
	std	r8,EL_TVS(r13,VS_lr)
	std	r9,EL_TVS(r13,VS_xer)

	mtsrr0	r4			// set iar for rfid
	mtsrr1	r5			// set msr for rfid
	ld	r2,EL_toc(r13)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r13)	// load delta for R-to-V conversion

	std	r10,EL_TVS(r13,VS_r7)	// save original r7

					// construct V->R address of PS
	la	r1,EL_trapVolatileState(r13)
	add	r1,r1,r0

	mfsprg  r13,sprg2		// restore r13

	rfid
CODE_END(exc_pgm_handler)
