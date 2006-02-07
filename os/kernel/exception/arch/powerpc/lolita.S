/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lolita.S,v 1.70 2005/08/22 14:54:08 rosnbrg Exp $
 *****************************************************************************/
#include <sys/kinclude.H>
#include <sys/arch/powerpc/asmConstants.H>
#include <sys/syscalls.H>
#include <sys/memoryMap.H>
#include <misc/hardware.H>
#include <mmu.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <misc/arch/powerpc/trap.h>

#include <sys/hcall.h>

/*
 * -------------------------------------------------------------
 *         LOw Level Interface To Assembler  (LOLITA)
 * -------------------------------------------------------------
 *
 *	General Register Usage
 *	sprg0:	 physical address of exceptionLocal
 *      sprg1:	 physical address of current low-level save area
 *	sprg2:	 scratch (usually used to preserve r14)
 *	sprg3:	 scratch (usually used to preserve r15)
 */

#define INC_COUNTER(offset,baseReg,scratchReg)\
	ld	scratchReg,(offset)(baseReg);\
	addi	scratchReg,scratchReg,1;\
	std	scratchReg,(offset)(baseReg)

/*
 * This code fragment is copied to the system-call exception handler address
 * (see init/arch/powerpc/initKernelMappings.C).
 */
CODE_ENTRY(lolita_exc_sc)
	/*
	 * On entry, r0 contains the svc index shifted left 16 bits.  R11 and
	 * r12 are available.  We corrupt these two registers and pass all
	 * others intact to the individual system call handler (which may or
	 * may not preserve them).  Note that we do not pass along either the
	 * original iar (currently in srr0) or the original msr (currently in
	 * srr1).
	 *
 	 * Entered via hardware interrupt with:
 	 *    MSR.EE=0   Exernal interrupts disabled
         *    MSR.IR=0   Instruction relocate off (I-fetch uses REAL addresses)
 	 *    MSR.DR=0   Data relocate off (loads & stores use REAL addresses)
	 */
	mfsprg	r12,sprg0		// r12 <- &exceptionLocal (real)
	rlwinm.	r11,r0,ROT_RIGHT_32(16-3),SHIFT_L((SYSCALL_LIMIT-1),3)
					// svc offset = (idx & mask) << 3
	addi	r11,r11,EL_svc		// r11 = svc[idx] offset in excLocal
	beq-	lolita_exc_sc_nonnative	// special-case nonnative svcs (idx==0)
	ldx	r11,r12,r11		// r11 <- exceptionLocal.svc[idx]
    lolita_exc_sc_continue:
	mtsrr0	r11			// iar for rfid
	ld	r11,EL_msr(r12)		// r11 <- kernel-mode msr
	mtsrr1	r11			// msr for rfid
	ld	r12,EL_elocalVirt(r12)	// get EL addressability for exc. level
	rfid				// continue at exception level
    lolita_exc_sc_nonnative:
#if 0	// We use syscall numbers above 0x1000 for low-level debugging stuff
	cmpldi	r0,0x1000
	blt+	lolita_exc_sc_skip
	ld	r11,EL_lolitaSVCHandler(r12)
	mtctr	r11
	bctr
    lolita_exc_sc_skip:
#endif
	INC_COUNTER(EL_num_nonnative_svc,r12,r11)
	mfsprg	r11,sprg1		// r11 <- current low-level save area
	std	r1,VS_r1(r11)		// save r1
	mfsrr0	r1			// retrieve iar
	std	r2,VS_r2(r11)		// save r2
	mfsrr1	r2			// retrieve msr
	std	r1,VS_iar(r11)		// save iar
	mflr	r1			// retrieve lr
	std	r2,VS_msr(r11)		// save msr
	std	r1,VS_lr(r11)		// save lr
	ld	r11,EL_svc(r12)		// r11 <- exceptionLocal.svc[0]
	b	lolita_exc_sc_continue
CODE_LABEL(lolita_exc_sc_end)
CODE_END(lolita_exc_sc)

CODE_ENTRY(lolita_trap_exc)
        mtsprg	sprg2,r14		// preserve r14
        mfsprg	r14,sprg0		// r14 <- &exceptionLocal (real)
        mtsprg	sprg3,r15		// preserve r15
//#define LOLITA_TRAP
#ifdef LOLITA_TRAP
//debug stuff can be used to diagnose machine checks following
//ping code in MPinit - another cpu must watch for this storage to change	
	mfcr	r15
	cmpldi	r14, 0
	bne	0f
	mfsrr0	r14
	std	r14, 0x188(0)
	mfsrr1	r14
	std	r14, 0x190(0)
	mfdar	r14
	std	r14, 0x198(0)
	mfdsisr r14
	std	r14, 0x1a0(0)
	bl	.+4
	mflr	r14
	std	r14, 0x180(0)
	b	.
0:	mtcr	r15	
#endif	

#undef DEBUG_MC
#ifdef  DEBUG_MC
	/* the code in lolita_trap_exc is copied during initialization in
	   all the location where excecution resumes after an interrupt.
	   Thus the address where this code is located is an indication
	   at run-time of the time of expetional condition we are handling.
	   In this stub we are interested in tracing machine check and
	   system reset interrupts and stopping the processors after
	   on occurs.  The resumption instruction address for system
	   return is 0x100 and for machine check 0x200.  At 0x300 
	   is the handler for a data storage interrupt.  Thus we
	   save and stop only if we are executing below 300
	 */

	/* r14 save area
 	   r15 saved and available
	*/
	std     r15,EL_lolita_hv_lr(r14)
	bl      .+4
        mflr    r15				
	// If we are neither the machine check not the system reset handler
	// then branch around and do nothing.  Else we stop the machine after
	// saving something relevant.
	std     r1,EL_lolita_hv_r1(r14)		// preserve r1
	mfcr    r1				// preserve cr
	cmpldi  r15,0x300			// what kind of handler are we?
        bge     1f				// not mc, not reset do nothing

	mfsrr0  r15
	std     r15,EL_lolita_hv_srr0(r14)
        mfsrr1  r15
	std     r15,EL_lolita_hv_srr1(r14)
	std     r3,EL_lolita_hv_r3(r14)
	std     r12,EL_lolita_hv_r12(r14)
	b	.				// STOP

1:
	mtcr	r1				// restore cr
	ld	r1,EL_lolita_hv_r1(r14)		// restore r1
#endif	// DEBUG_MC

	std	r7,EL_lolita_r7(r14)	// preserve r7
	ld	r15,EL_trapHandler(r14)	// real address of trap code
	mflr	r7			// preserve lr
	mtlr	r15			// load handler into lr for branch
	blrl				// branch to exception handler
CODE_LABEL(lolita_trap_exc_end)
CODE_END(lolita_trap_exc)


#define EX_REG(num)						\
l##num:								;\
	isync							;\
	sync							;\
	ld	r16, 0(r15)					;\
	cmpdi	r16, 0						;\
	isync							;\
	sync							;\
	bne	l##num						;\
	isync							;\
	std	r##num, 0(r16)					

CODE_ENTRY(exc_dbg_handler)
	li	r15, 0
	oris	r15, r4, cpu_starter@h
	ori	r15, r15, cpu_starter@l
	EX_REG(0)
	EX_REG(1)
	EX_REG(2)
	EX_REG(3)
	EX_REG(4)
	EX_REG(5)
	EX_REG(6)
	EX_REG(7)
	EX_REG(8)
	EX_REG(9)
	EX_REG(10)
	EX_REG(11)
	EX_REG(12)
	EX_REG(13)
	EX_REG(14)
	EX_REG(15)
	EX_REG(16)
	EX_REG(17)
	EX_REG(18)
	EX_REG(19)
	EX_REG(20)
	EX_REG(21)
	EX_REG(22)
	EX_REG(23)
	EX_REG(24)
	EX_REG(25)
	EX_REG(26)
	EX_REG(27)
	EX_REG(28)
	EX_REG(29)
	EX_REG(30)
	EX_REG(31)
end_loop:
	nop
	b end_loop
CODE_END(exc_dbg_handler)	
	
CODE_ENTRY(exc_pgm_handler)
	mflr	r16
	mfctr	r16
	mfdar	r7
	
	mfsrr0	r15
	mfsrr1  r7
	mfmsr	r7
__done2:
	nop
	b __done2
CODE_END(exc_pgm_handler)	
CODE_ENTRY(lolita_mchk_exc)
	ba	0x2000
#ifdef TESTCHECKSTOP
	bla	0x2000
#endif	
CODE_LABEL(lolita_mchk_exc_end)
CODE_END(lolita_mchk_exc)
	
CODE_ENTRY(lolita_mchk_code)
#ifdef TESTCHECKSTOP
	lwz	r31, 0x204(r0)
	stw	r31, 0x100(r0)
	stw	r31, 0x300(r0)
	stw	r31, 0x400(r0)
	stw	r31, 0x500(r0)
	stw	r31, 0x600(r0)
	stw	r31, 0x700(r0)
	stw	r31, 0x800(r0)
	stw	r31, 0x900(r0)
	stw	r31, 0xa00(r0)
	stw	r31, 0xb00(r0)
	stw	r31, 0xc00(r0)
	stw	r31, 0xd00(r0)
	stw	r31, 0xe00(r0)
	stw	r31, 0xf00(r0)
#endif
	mflr	r31
	li	r3,0x2F		//forward slash
	bl	mchk_putchar
	li	r3,0x20
	bl	mchk_putchar
	bl	mchk_putchar
	bl	mchk_putchar
	bl	mchk_putnl
	mr	r3, r31
	bl	mchk_putword
	mfsrr0	r3
	bl	mchk_putword
	mfsrr1  r3
	bl	mchk_putword
	mfmsr	r3
	bl	mchk_putword
	mr	r3, r0
	bl	mchk_putword
	bl	mchk_putnl
#ifdef TESTCHECKSTOP
	li	r0,0x1111;rldimi r0,r0,16,0;rldimi r1,r0,32,0
	li	r0,0x7afa;rldimi r0,r0,16,0;rldimi r31,r0,32,0	
	li	r0,0x7cde;rldimi r0,r0,16,0;rldimi r0,r0,32,0		
	li	r2,0x1234
	li	r3,0x5678
	mtctr	r3
	li	r3,0x6789
	mtlr	r3
	li	r3,0x7ead
	li	r4, 0x5000
	sldi	r4, r4, 16
	std	r4, 0(r4)
	dcbst	r0, r4
	mtlr	r4
	blr
#endif	
	b	$
mchk_putnl:
	li	r3, 10
mchk_putchar:
	li	r4, -1
	sldi	r4,r4,27
	mfmsr	r5
	ori	r5,r5,PSL_DR
	mtmsr	r5
	isync
	li	r5, 0x02		//MCR_DTR
	stb	r5,0x3f8+4(r4)
	eieio
	lbz	r5,0x3f8+5(r4)
	eieio
	andi.	r5,r5,0x20
	beq	$-12
	stb	r3,0x3f8+0(r4)
	eieio
	mfmsr	r5
	xori	r5,r5,PSL_DR
	mtmsr	r5
	isync
	blr
mchk_puthex:	
	cmpldi	r3, 10
	addi	r3, r3, 0x30
	blt	$+8
	addi	r3, r3, 7
	b	mchk_putchar
mchk_putword:	
	mflr	r17
	mr	r16, r3
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	rldicl	r3, r16, 4, 60
	sldi	r16, r16, 4
	bl	mchk_puthex
	bl	mchk_putnl
	mtlr	r17
	blr	
	
CODE_LABEL(lolita_mchk_code_end)
CODE_END(lolita_mchk_code)
	
/*
 * This code fragment is copied to each exception-handler address for which
 * the full machine state must be preserved.  The load from the EL_handlers
 * array is patched specifically in each copy to load the appropriate entry
 * from the array (see init/arch/powerpc/initKernelMappings.C).
 *
 * Exception handlers are entered via hardware interrupt with:
 *    MSR.EE=0   Exernal interrupts disabled
 *    MSR.IR=0   Instruction relocate off (I-fetch uses REAL addresses)
 *    MSR.DR=0   Data relocate off (loads & stores use REAL addresses)
 */
CODE_ENTRY(lolita_involuntary_exc)
        mtsprg  sprg2,r14		// preserve r14
        mfsprg  r14,sprg0		// r14 <- &exceptionLocal (real)
        mtsprg  sprg3,r15		// preserve r15
	std	r7,EL_lolita_r7(r14)	// preserve r7

CODE_LABEL(lolita_involuntary_exc_ldhdlr)
	ld	r15,EL_handlers(r14)	// PATCHED to load correct entry
	mfctr   r7			// preserve ctr
	mtctr	r15			// load handler into ctr for branch
	bctr				// branch to exception handler
CODE_LABEL(lolita_involuntary_exc_end)
CODE_END(lolita_involuntary_exc)

#define SAVE_LOLITA_STATE(base) \
	std r0, EL_lolita_hv_r0(base) ;  \
	std r1, EL_lolita_hv_r1(base) ;  \
	std r2, EL_lolita_hv_r2(base) ;  \
	std r3, EL_lolita_hv_r3(base) ;  \
	std r4, EL_lolita_hv_r4(base) ;  \
	std r5, EL_lolita_hv_r5(base) ;  \
	std r6, EL_lolita_hv_r6(base) ;  \
	std r7, EL_lolita_hv_r7(base) ;  \
	std r8, EL_lolita_hv_r8(base) ;  \
	std r9, EL_lolita_hv_r9(base) ;  \
	std r10, EL_lolita_hv_r10(base) ; \
	std r11, EL_lolita_hv_r11(base) ; \
	std r12, EL_lolita_hv_r12(base) ; \
	mfctr  r12		; \
	std r12, EL_lolita_hv_ctr(base)	; \
	mflr  r12		; \
	std r12, EL_lolita_hv_lr(base)	; \
	mfcr   r12		; \
	std r12, EL_lolita_hv_cr(base) ; \
	mfsrr0 r6		; \
	mfsrr1 r7		; \
	std r6, EL_lolita_hv_srr0(base) ; \
	std r7, EL_lolita_hv_srr1(base) 

#define RESTORE_LOLITA_STATE(base) \
	ld r6, EL_lolita_hv_srr0(base) ; \
	ld r7, EL_lolita_hv_srr1(base) ; \
	mtsrr0 r6		; \
	mtsrr1 r7		; \
	ld r0, EL_lolita_hv_r0(base) ; \
	ld r1, EL_lolita_hv_r1(base) ; \
	ld r2, EL_lolita_hv_r2(base) ; \
	ld r3, EL_lolita_hv_r3(base) ; \
	ld r4, EL_lolita_hv_r4(base) ; \
	ld r5, EL_lolita_hv_r5(base) ; \
	ld r6, EL_lolita_hv_r6(base) ; \
	ld r7, EL_lolita_hv_r7(base) ; \
	ld r8, EL_lolita_hv_r8(base) ; \
	ld r9, EL_lolita_hv_r9(base) ; \
	ld r10, EL_lolita_hv_r10(base) ; \
	ld r11, EL_lolita_hv_r11(base) ; \
	ld r12,  EL_lolita_hv_ctr(base)	; \
	mtctr r12		; \
	ld r12,  EL_lolita_hv_lr(base)	; \
	mtlr r12		; \
	ld r12,  EL_lolita_hv_cr(base)	; \
	mtcr r12		; \
	ld r12, EL_lolita_hv_r12(base)
		
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
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */

	INC_COUNTER(EL_num_null,r14,r15)
	mtctr	r7
	ld	r7,EL_lolita_r7(r14)
	mfsprg  r15,sprg3
	mfsprg  r14,sprg2
	rfid
CODE_END(exc_null_handler)


/*
 * -------------------------------------------------------------
 *  Dec null Interrupt - for use during initialization
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_dec_null_handler)
    /*
     * Entered with interrupts disabled, REAL addressing mode
     *
     * Set the decrementer to a non-negative value. An architectural
     * change triggers interrupts not only on postive to negative
     * transitions but on any negative value.		
     *		
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */

	INC_COUNTER(EL_num_null,r14,r15)
	mtctr	r7
	lis     r7, 0x7fff
	mtdec   r7
	ld	r7,EL_lolita_r7(r14)
	mfsprg  r15,sprg3
	mfsprg  r14,sprg2
	rfid
CODE_END(exc_dec_null_handler)

/*
 * -------------------------------------------------------------
 *  Instruction Segment Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_isgi_handler)
    /*
     * Entered with interrupts disabled, REAL addressing mode
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_isgi,r14,r15)

	std	r5,EL_lolita_r5(r14)	// preserve r5
	mfsrr0  r5			// get fault address from SRR0
	b	sgfltCommon
CODE_END(exc_isgi_handler)

/*
 * -------------------------------------------------------------
 *  Data Segment Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_dsgi_handler)
    /*
     * Entered with interrupts disabled, REAL addressing mode
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_dsgi,r14,r15)

	std	r5,EL_lolita_r5(r14)	// preserve r5
	mfdar   r5			// get fault address from DAR
					// fall thru
sgfltCommon:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     *	r5		| exceptionLocal.r5	| fault address
     */

	ld	r15,EL_kernelRegionsEnd(r14)
	std	r6,EL_lolita_r6(r14)	// preserve r6
	mfcr	r6			// preserve cr
	cmpld	r5,r15			// compare faultAddr : kernelRegionsEnd
	std	r4,EL_lolita_r4(r14)	// preserve r4
	lis	r4,0x20			// fault info = segment lookaside miss
	blt	pageFault		// branch if NOT kernel-address fault

    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info, available
     */

	std	r3,EL_lolita_r3(r14)	// save R3
	std	r2,EL_lolita_r2(r14)	// and R2
	/*
	 * Construct the VSID for the faulting address.  For kernel
	 * addresses, the VSID is simply 0x0001 concatenated with the
	 * high-order 36 bits (the ESID) of the fault address.
	 */
 	ld	r15,EL_vMapsRStart(r14)	// load V->R start
	clrrdi	r4,r5,28		// mask off non-ESID
	ori	r4,r4,0x0001		// prepend 0x0001 to obtain VSID
	rldicl	r4,r4,48,0		// align VSID to left of register
	// r4 now has the complete default VSID value
	cmpld	r5,r15			// compare fault addr with V->R start
 	ld	r15,EL_vMapsREnd(r14)	// load V->R end
	blt	finishVSID		// less than V->R, no large pages
	cmpld	r5,r15			// compare fault addr with V->R end
	ld	r15,EL_lgPg_numSizes(r14)// get number of large page sizes
	bge	nextVSID		// beyond V->R, no default large pages
	and.	r15,15,r15		// test large page sizes
	beq	finishVSID		// branch if zero, no large pages
	/*
	 * We use large pages for selected ranges of mapping faults.
	 * This code assumes there is only one large page size of 16M bytes
	 * (GP GQ GR ...) or, for machines with multiple large page sizes,
	 * HID reg setup causes PTE_LP = SLB_LS = 0 to select 16MB pages.
	 * See mem/arch/powerpc/InvertedPageTable.C.
	 */
	ori	r4,r4,0x100		// set L = 1, LS = 0 for large pages
	b	finishVSID
	/*
	 * Processor specific memory needs a different vsid for
	 * each processor in case the page table is shared by several
	 * processors.  We check for those regions here, assuming
	 * each is no larger than a single segment
	 */
nextVSID:	
	ld	r15, EL_kernelPSRStart(r14)
	cmpld   r5,r15
	blt     finishVSID
	ld      r15, EL_kernelPSREnd(r14)
	cmpld   r5,r15
	bge	0f
	ld      r4, EL_kernelPSRVSID(r14)
	b       finishVSID
0:	
	ld	r15, EL_commonPSRStart(r14)
	cmpld   r5,r15
	blt     finishVSID
	ld      r15, EL_commonPSREnd(r14)
	cmpld   r5,r15
	bge	finishVSID
	ld      r4, EL_commonPSRVSID(r14)
finishVSID:
	ori	r4,r4,0x400		// set Ks = 0, Kp = 1, N,C = 0	
	// r4 now has the complete VSID word of the SLBE	
	clrrdi	r5,r5,28		// mask off non-ESID
    /*
     * we need to add mapping faults to the current
     * cache.
     *
     */
	ld	r15, EL_currentSegmentTable(r14)
	ld	r2,EL_vMapsRDelta(r14)  // load delta for V-to-R conversion
	oris	r5,r5,0x800		// set V = 1 in ESID word
	sub	r15,r15,r2		// convert seg table addr to real
	ld      r3,ST_slbNext(r15)	// slb slot to load
	or	r5,r5,r3		// insert SLB index
	// r5 now has the complete ESID word of the SLBE
	slbmte	r4,r5			// load chosen SLB entry
	// now update slbNext
	addi	r2,r3,1 		// increment SLB index
	ld	r3,ST_cacheMax(r15)	// number cached
	cmpldi	r2,ST_NUMBER_OF_SLBS-1	// compare with maximum index
	ble	saveSLBIndex		// skip if not too large
	mr	r2,r3			// next free after cache
saveSLBIndex:
	std	r2,ST_slbNext(r15)	// save updated SLB index
	ld	r2,EL_segLoad(r14)	// check for locked cache
	mtctr	r3			// number of cached to load
	la	r3,ST_SLBCache-16(r15)	// set up to search cache
	cmpldi	r2,EL_SOFTWARE_LOCKED	// is cache locked
	beq-	resume2			// then skip cache update
0:	ldu	r2,16(r3)		// since entry might be there already
	cmpld	r2, r4			// check for matching vsid word
	beq-	resume2			// already cached
	bdnz	0b
	ld	r3, ST_cacheNext(r15)	// next cache slot to fill (wraps)
	ld	r2, ST_cacheMax(r15)	// number of cache slots in use
	addi	r3, r3, 1		// check high water mark
	cmpld	r2, r3			// compare cacheMax to new cacheNext
	bge	1f
	std     r3, ST_cacheMax(r15)	// N.B. can't exceed NUMCACHED
1:	mr	r2, r3
	cmpldi	r2, ST_NUMCACHED	// because we wrap cacheNext here
	bne+	2f
	ld	r2, ST_numBolted(r15)	// back to numBolted
2:	subi	r3, r3, 1		// restore value to target index
	rldimi  r5, r3, 0, 52		// replace index in ESID word
	sldi	r3, r3, 4		// multiply by array element
	std	r2, ST_cacheNext(r15)	// store new cacheNext value
	la	r2, ST_SLBCache(r15)	// origin of cache
	add	r2, r2, r3		// SLBCache[old-cacheNext-value]
	std	r4, 0(r2)		// save new vsid and esid words
	std	r5, 8(r2)
	b	resume2
CODE_END(exc_dsgi_handler)

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
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_isi,r14,r15)

	std	r4,EL_lolita_r4(r14)	// preserve r4
	std	r5,EL_lolita_r5(r14)	// preserve r5
        mfsrr1  r4			// get fault info from SRR1
	mfsrr0  r5			// get fault address from SRR0

	b	pgfltCommon
	
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
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_dsi,r14,r15)

	std	r4,EL_lolita_r4(r14)	// preserve r4
	std	r5,EL_lolita_r5(r14)	// preserve r5
        mfdsisr r4			// get fault info from DSISR
	mfdar   r5			// get fault address from DAR
					// fall thru

pgfltCommon:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */
	
	ld	r15,EL_kernelRegionsEnd(r14)
	std	r6,EL_lolita_r6(r14)	// preserve r6
	mfcr	r6			// preserve cr
	cmpld	r5,r15			// it's a kernel-address fault if
	bge	kernAddrFault		//     faultAddr >= kernelRegionsEnd

pageFault:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */

	mfsprg	r15,sprg1		// r15 <- current low-level save area

	std	r3,VS_r3(r15)		// save r3
	mfsrr1	r3			// pick up original msr
	std	r2,VS_r2(r15)		// save r2
	mfsrr0	r2			// pick up original iar
	std	r0,VS_r0(r15)		// save r0
	andi.	r0,r3,PSL_PR		// test for kernel-mode page fault
	std	r1,VS_r1(r15)		// save r1

	beq	pageFaultKernel		// branch if kernel-mode page fault

	// user-mode page fault
	std	r8,VS_r8(r15)		// save r8-r13
	std	r9,VS_r9(r15)
	std	r10,VS_r10(r15)
	std	r11,VS_r11(r15)
	std	r12,VS_r12(r15)
	std	r13,VS_r13(r15)

	std	r2,VS_iar(r15)		// save original msr, iar
	std	r3,VS_msr(r15)

	mflr	r8			// pick up (original) lr, xer
	mfxer	r9

	// prepare values for rfid
	ld	r2,EL_excPgfltExceptionUser(r14)
	ld	r3,EL_msr(r14)

	std	r6,VS_cr(r15)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r15)
	std	r8,VS_lr(r15)
	std	r9,VS_xer(r15)

	mtsrr0	r2			// set iar for rfid
	mtsrr1	r3			// set msr for rfid

	ld	r6,EL_lolita_r4(r14)	// pick up original values of r4-r7
	ld	r7,EL_lolita_r5(r14)
	ld	r8,EL_lolita_r6(r14)
	ld	r9,EL_lolita_r7(r14)

	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r14)	// load delta for R-to-V conversion

	mfsprg  r14,sprg2		// restore r14

	std	r6,VS_r4(r15)		// save original values of r4-r7
	std	r7,VS_r5(r15)
	std	r8,VS_r6(r15)
	std	r9,VS_r7(r15)

	add	r1,r15,r0		// construct V->R address of PS

	mfsprg	r15,sprg3		// restore r15

	rfid

pageFaultKernel:

	ld	r0,EL_vMapsRDelta(r14)	// load delta for V-to-R conversion
	// move stk ptr past current floor and make room for VolatileState
	la	r1,(STK_FLOOR-VS_SIZE)(r1)
	sub	r1,r1,r0		// convert stackptr to real

	std	r8,VS_r8(r1)		// save r8-r13
	std	r9,VS_r9(r1)
	std	r10,VS_r10(r1)
	std	r11,VS_r11(r1)
	std	r12,VS_r12(r1)
	std	r13,VS_r13(r1)

	std	r2,VS_iar(r1)		// save original msr, iar
	std	r3,VS_msr(r1)

	mflr	r8			// pick up lr, xer
	mfxer	r9

	// prepare values for rfid
	ld	r2,EL_excPgfltExceptionKernel(r14)
	ld	r3,EL_msr(r14)

	std	r6,VS_cr(r1)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r1)
	std	r8,VS_lr(r1)
	std	r9,VS_xer(r1)

	mtsrr0	r2			// set iar for rfid
	mtsrr1	r3			// set msr for rfid
	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid

	ld	r6,EL_lolita_r4(r14)	// pick up original values of r4-r7
	ld	r7,EL_lolita_r5(r14)
	ld	r8,EL_lolita_r6(r14)
	ld	r9,EL_lolita_r7(r14)

	mfsprg  r14,sprg2		// restore r14

	std	r6,VS_r4(r1)		// save original values of r4-r7
	std	r7,VS_r5(r1)
	std	r8,VS_r6(r1)
	std	r9,VS_r7(r1)

	ld	r6,VS_r0(r15)		// retrieve original values of r0-r3
	ld	r7,VS_r1(r15)
	ld	r8,VS_r2(r15)
	ld	r9,VS_r3(r15)

	mfsprg	r15,sprg3		// restore r15

	std	r6,VS_r0(r1)		// save original values of r0-r3
	std	r7,VS_r1(r1)
	std	r8,VS_r2(r1)
	std	r9,VS_r3(r1)

	add	r1,r1,r0		// convert stackptr back to virtual

	rfid

kernAddrFault:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */

    /*
     * Register save warning.  All paths to pageFault must NOT use
     * r3.  But all paths to the resume code must save r3, because
     * we call resume3 at the end.
     */	
	andis.	r15,r4,0x4000		// check for page table miss
	bne+	pageTableMiss
	andis.	r15,r4,0x0020		// check for segment miss
	bne+	segmentMiss
	b	pageFault		// real page fault

pageTableMiss:
	/*
	 * The faulting address distinguishes mapping faults from real
	 * page faults.  We check the various mapping-fault ranges in
	 * increasing-address order.
	 */
	// check the V->R range
 	ld	r15,EL_vMapsRStart(r14)	// load and compare to V->R start
	cmpld	r5,r15
	blt	pageFault		// less than start, real page fault
	ld	r15,EL_vMapsREnd(r14)	// load and compare to V->R end
	cmpld	r5,r15
	bge	checkKernelPSR		// greater than end, check next range
	ld	r4,EL_vMapsRDelta(r14)	// load V->R adjustment
	li	r15,0x190		// bitset R=1,C=1 WIMG=0010 X=0 PP=00
	
#if 0
	// map first 64K read only for testing
	sub	r15,r5,r4
	cmpldi	r15,0xffff
	li	r15,0x190		// bitset R=1,C=1 WIMG=0010 X=0 PP=00	
	bgt	createPTE_L
	li	r15,0x193		// bitset R=1,C=1 WIMG=0010 X=0 PP=11	
createPTE_L:
#endif		

	std	r3,EL_lolita_r3(r14)	// preserve r3
	ld	r3,EL_lgPg_numSizes(r14)// get number of large page sizes
	and.	r3,r3,r3		// test it for zero
	sub	r3,r5,r4		// R = V - V-to-R delta
	beq	createPTE1		// branch if no large pages
	/*
	 * We use large pages for selected ranges of mapping faults.
	 * This code assumes there is only one large page size of 16M bytes
	 * (GP GQ GR ...) or, for machines with multiple large page sizes,
	 * HID reg setup causes PTE_LP = SLB_LS = 0 to select 16MB pages.
	 * See mem/arch/powerpc/InvertedPageTable.C.
	 */
	rldimi	r3,r15,0,40		// or ARPN with LP = 0, RCWIMGPP above
	// r3 now has the complete RPN word of the PTE
	clrrdi	r4,r5,24		// mask off ESID/AVPI
	oris	r4,r4,0x4		// turn on large page L bit in VSID word
	rldicl	r5,r5,40,60		// extract 4-bit virtual page index
	b	createPTE2

checkKernelPSR:
	// check the kernelPSpecificRegion range (r3 not yet saved)
 	ld	r15,EL_kernelPSRStart(r14)// load and compare to PSR start
	cmpld	r5,r15
	blt	pageFault		// less than start, real page fault
	ld	r15,EL_kernelPSREnd(r14)// load and compare to PSR end
	cmpld	r5,r15
	bge	checkCommonPSR		// greater than end, check next range
	ld	r4,EL_kernelPSRDelta(r14)// load PSR adjustment
	std	r3,EL_lolita_r3(r14)	// preserve r3
	sub	r3,r5,r4		// R = V - V-to-R delta
	ld	r4,EL_kernelPSRVSID(r14)// proc specific vsid
	li	r15,0x190		// bitset R=1,C=1 WIMG=0010 PP=00
	b	createPTEV

checkCommonPSR:
	// check the commonPSpecificRegion range (r3 not yet saved)
 	ld	r15,EL_commonPSRStart(r14)// load and compare to PSR start
	cmpld	r5,r15
	blt	pageFault		// less than start, real page fault
	ld	r15,EL_commonPSREnd(r14)// load and compare to PSR end
	cmpld	r5,r15
	bge	checkTracing		// greater than end, check next range
	ld	r4,EL_commonPSRDelta(r14)// load PSR adjustment
	std	r3,EL_lolita_r3(r14)	// preserve r3
	sub	r3,r5,r4		// R = V - V-to-R delta
	ld	r4,EL_commonPSRVSID(r14)// proc. specific vsid
	// initial subrange of commonPSR is read-only
	// subrange above commonPSRWritable is writable
	ld	r15,EL_commonPSRWritable(r14)
	cmpld	r5,r15
					// assume read-only
	li	r15,0x193		// bitset R=1,C=1 WIMG=0010 PP=11
	blt	createPTEV
					// convert to writable
	li	r15,0x192		// bitset R=1,C=1 WIMG=0010 PP=10

	//N.B. all paths to this point must have saved r3
createPTEV:
	rldimi	r3,r15,0,52		// or RPN with RCWIMGPP from above
	// r3 now has the complete RPN word of the PTE
	rldicl	r5,r5,52,48		// extract 16-bit virtual page index
	b	createWithVsid
	

checkTracing:
	// check the tracing range (r3 not yet saved)
 	ld	r15,EL_traceRgnStart(r14)// load and compare to tracing start
	cmpld	r5,r15
	blt	pageFault		// less than start, real page fault
	ld	r15,EL_traceRgnEnd(r14)	// load and compare to tracing end
	cmpld	r5,r15
	bge	checkIO			// greater than end, check next range
	ld	r4,EL_traceRgnDelta(r14)// load tracing adjustment
	li	r15,0x192		// bitset R=1,C=1 WIMG=0010 PP=10
	std	r3,EL_lolita_r3(r14)	// preserve r3
	sub	r3,r5,r4		// R = V - V-to-R delta
	b	createPTE1

checkIO:
	// check the I/O range (r3 not yet saved)
 	ld	r15,EL_ioRgnStart(r14)	// load and compare to I/O start
	cmpld	r5,r15
	blt	pageFault		// less than start, real page fault
	ld	r15,EL_ioRgnEnd(r14)	// load and compare to I/O end
	cmpld	r5,r15
	bge	pageFault		// greater than end, real page fault

	// do mapping of I/O chunk sub-ranges in the I/O range
	ld	r4,EL_ioRgnMap(r14)	// load I/O range map address
	rldicl	r15,r5,64-LOG_IOCHUNK_SIZE+3,64-LOG_NUM_IOCHUNKS-3
	andi.	r15,r15,(1<<(LOG_NUM_IOCHUNKS+3))-8  // get I/O chunk index
	std	r3,EL_lolita_r3(r14)	// preserve r3
	ldx	r3,r4,r15		// load real address for I/O chunk
	li	r15,0x1B8		// bitset R=1,C=1 WIMG=0111 PP=00
	rldicl	r4,r5,0,64-LOG_IOCHUNK_SIZE  // get offset within chunk
	add	r3,r3,r4		// add it to chunk real address

createPTE1:
	rldimi	r3,r15,0,52		// or RPN with RCWIMGPP from above
	// r3 now has the complete RPN word of the PTE
	clrrdi	r4,r5,23		// mask off ESID/AVPI
	rldicl	r5,r5,52,48		// extract 16-bit virtual page index

createPTE2:
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| VPI for page size
     *	r4		| exceptionLocal.r4	| ESID/AVPI for page size
     *	r3		| exceptionLocal.r3	| RPN word for PTE
     */

	/*
	 * Construct the VSID for the faulting address.  For mapping-fault
	 * addresses, the VSID is simply 0x0001 concatenated with the
	 * high-order 36 bits (the ESID) of the fault address.  We also need
	 * the next 5 (or less) bits (the AVPI field, preserved above).
	 */
	ori	r4,r4,0x0001		// prepend 0x0001 to obtain VSID/AVPI
	rldicl	r4,r4,48,0
createWithVsid:	
	ori	r4,r4,0x1		// turn on valid bit in VSID word
	// r4 now has the complete VSID word of the PTE

	rldicl	r15,r4,52,25		// right-justify 39 bits of VSID
	xor	r15,r15,r5		// xor page index with VSID (hash)

	ld	r5,EL_pte(r14)		// load address of hashtable

	/*
	 * The following instruction inserts the hash bits (pteg index) into
	 * the hashtable address.  Insertion works because the hashtable is
	 * suitably aligned.  The number of bits to be inserted depends on the
	 * size of the hashtable, which is determined at boot time.  Code in
	 * InitKernelMappings() patches this instruction at that time.  The
	 * instruction assembled here is appropriate for a minimum-size
	 * hashtable.
	 */
CODE_LABEL(lolita_insert_pteg_idx)
	rldimi	r5,r15,7,46		// insert hash bits into hashtable addr
					//     For larger hashtables:
					//         rldimi r5,r15,7,46  - min
					//         rldimi r5,r15,7,45  - 2X
					//         rldimi r5,r15,7,44  - 4X
					//         rldimi r5,r15,7,43  - 8X
					//         ....
					//         rldimi r5,r15,7,18  - max
	// r5 now has the address of the relevant PTE group

	INC_COUNTER(EL_num_map_fault,r14,r15)

		
	/*
	 * If this is a shared page table, must aquire the lolita
	 * spin lock.  Do it here because we have free registers
	 */
	ld	r15, EL_lolitaLock(r14)
	cmpldi  r15,0
	beq+    notShared
	std	r2,EL_lolita_r2(r14)	// preserve r2	
spin:	
	ldarx	r2, 0, r15
	cmpldi	r2, 0
	bne-	spin
	stdcx.	r15, 0, r15		//r15 convenient non-zero value
	bne-	spin
	lwsync
notShared:	

	/*
	 * This location will get patched at runtime and become
	 * a NOP if we are on HV
	 */
CODE_LABEL(lolita_insert_pteg_HV_patch)
	b	not_onHV

	SAVE_LOLITA_STATE(r14)
	b	hv_enter

hv_enter_retry:	
	RESTORE_LOLITA_STATE(r14)	

hv_enter:
	srdi	r5, r5, 4		// divide by pte size to get pte index
	mr	r6, r4
	mr	r7, r3
	li	r4, 0
	li	r3, H_ENTER
	HSC				// hcall
	cmpdi	r3, 0
	beq	hv_no_err
	cmpdi	r3, H_PTEG_FULL
	beq	hv_evict
	.long	0x7c0007ce		// SIMOS breakpoint
	b .

hv_evict:
	li	r15, 0			// FIXME should start at random pte
	b	hv_evict_loop+4

hv_evict_loop:
	addi	r15, r15, 1
	RESTORE_LOLITA_STATE(r14)	
	srdi	r5, r5, 4		// divide by pte size to get pte index
	rldimi	r5, r15, 0, 61
	li	r4, 0
	li	r3, H_CLEAR_REF
	HSC				// hcall
	cmpdi	r3, 0
	beq	hv_test_ref
	.long	0x7c0007ce		// SIMOS breakpoint
	b .

hv_test_ref:
	andi.	r4, r4, PTE_R_MASK
	bne	hv_evict_loop
	RESTORE_LOLITA_STATE(r14)	
	srdi	r5, r5, 4		// divide by pte size to get pte index
	rldimi	r5, r15, 0, 61
	li	r4, 0
	li	r3, H_READ
	HSC				// hcall
	cmpdi	r3, 0
	beq	hv_test_bolted
	.long	0x7c0007ce		// SIMOS breakpoint
	b .

hv_test_bolted:
	andi.	r4, r4, PTE_BOLTED_MASK
	bne	hv_evict_loop
	RESTORE_LOLITA_STATE(r14)	
	srdi	r5, r5, 4		// divide by pte size to get index
	rldimi	r5, r15, 0, 61
	li	r4, 0
	li	r3, H_REMOVE
	HSC				// hcall
	cmpdi	r3, 0
	beq	hv_enter_retry
	.long	0x7c0007ce		// SIMOS breakpoint
	b .

hv_no_err:	
	RESTORE_LOLITA_STATE(r14)
	b	finishPF

	/*
	 * This is the non HV path
	 */	
not_onHV:
	li	r15,8			// load CTR with 8 for counting
	mtctr	r15
	la	r5,-16(r5)		// prepare for load-with-update

loopFindAvail:
	ldu	r15,16(r5)		// load vsid-word
	andi.	r15,r15,0x11		// check for valid or bolted bit on
	beq	storePTE		// branch if available (both bits off)
	bdnz	loopFindAvail		// loop thru all 8 entries

	/*
	 * No available PTE.  We have to evict one.  We'll mark the PTE we
	 * store as the "last" PTE in this group.
	 */
	INC_COUNTER(EL_num_map_evict,r14,r15)
	ori	r4,r4,0x08		// turn on last bit in vsid word

	/*
	 * Find the current "last" PTE in the group, if there is one.  Starting
	 * there, search for a PTE with the referenced bit off, clearing any
	 * referenced bits we encounter during the search.  The loop will
	 * terminate as long as there's at least one non-bolted entry.  At this
	 * point r5 points to the last PTE in the group.
	 */
	li	r15,8			// reload count register
	mtctr	r15
	la	r5,-(8*16)(r5)		// back up to before first entry
loopFindLast:
	ldu	r15,16(r5)		// load vsid word
	andi.	r15,r15,0x08		// check for last bit on
	bne	foundLast		// branch if found
	bdnz	loopFindLast		// loop thru all 8 entries
	b	loopFindUnrefd		// no marked entry; start at beginning
foundLast:
	ld	r15,0(r5)		// load vsid word
	xori	r15,r15,0x08		// clear last bit (we know it's set)
	std	r15,0(r5)		// store vsid word
loopFindUnrefd:
	andi.	r15,r5,0x70		// extract index bits
	addi	r15,r15,0x10		// advance to next PTE
	rlwimi	r5,r15,0,0x70		// reinsert bits (possibly wrapping)
	ld	r15,8(r5)		// load rpn word
	andi.	r15,r15,0x100		// check referenced bit
	beq+	checkBolted		// ref bit off; use PTE unless bolted
	ld	r15,8(r5)		// reload rpn word
	xori	r15,r15,0x100		// clear ref bit (we know it's set)
	std	r15,8(r5)		// store rpn word
	b	loopFindUnrefd		// try next PTE
checkBolted:
	ld	r15,0(r5)		// make sure PTE is not bolted
	andi.	r15,r15,0x10		// check for bolted bit on
	bne-    loopFindUnrefd		// bolted, skip it
	// fall through to storePTE
	/*
	 * FIXME If we ever use hardware change bits, we must
	 * do a tlbie/sync for the entry we evict to make sure
	 * the change bit is updated.
	 */

storePTE:
	std r3,8(r5)			// store RPN word of PTE
	std r4,0(r5)			// store VSID word of PTE

finishPF:	
	/*
	 * If this is a shared page table, must release the lolita
	 * spin lock.
	 */
	ld	r15, EL_lolitaLock(r14)
	cmpldi  r15,0
	beq+    resume3
	li	r2, 0
	eieio				//export barrier for pte changes
	std	r2, 0(r15)		//release spin lock
resume2:		
	ld	r2,EL_lolita_r2(r14)	// restore r2	
	
resume3:
    /*
     *  At this point all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| available
     *	r4		| exceptionLocal.r4	| available
     *	r3		| exceptionLocal.r3	| available
     */
	ld	r3,EL_lolita_r3(r14)	// restore r3

resume4:
    /*
     *  At this point, registers are as described immediately above,
     *  except that register r3 has its original value.
     */
	mtcr	r6			// restore cr
	mtctr	r7			// restore ctr

	ld	r4,EL_lolita_r4(r14)	// restore r4
	ld	r5,EL_lolita_r5(r14)	// restore r5
	ld	r6,EL_lolita_r6(r14)	// restore r6
	ld	r7,EL_lolita_r7(r14)	// restore r7

	mfsprg	r15,sprg3		// restore r15
	mfsprg  r14,sprg2		// restore r14
	rfid				// resume

segmentMiss:
    /*
     * We're here because address translation could not find a segment table
     * entry for a kernel-address I-fetch, D-load, or D-store.
     *
     * For addresses in kernel space, we manufacture the appropriate segment
     * table entry here and install it.  We extract the Effective Segment ID
     * (ESID) from the faulting Effective Address (virtual address in K42
     * parlance), and from it we compute the Virtual Segment ID (VSID).
     * These two values plus some flags and protection keys make up the
     * segment table entry.
     *
     * The ESID of a faulting address consists of the high-order 36 bits.
     * For kernel addresses, we construct a 52-bit VSID by prepending 0x0001
     * to the ESID.  (The high order bit of the VSID is used as a lock bit
     * in page table entries.)
     *
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	r6		| exceptionLocal.r6	| original cr value
     *	ctr		| r7			| available
     *	cr		| r6			| available
     *	r5		| exceptionLocal.r5	| fault address
     *	r4		| exceptionLocal.r4	| fault info
     */
	INC_COUNTER(EL_num_segmiss,r14,r15)

	std	r3,EL_lolita_r3(r14)	// preserve r3

	/*
	 * Construct the VSID for the faulting address.  For kernel
	 * addresses, the VSID is simply 0x0001 concatenated with the
	 * high-order 36 bits (the ESID) of the fault address.
	 * We first check for PSR ranges which use vsids recorded
	 * in exceptionlocal and different for each processor
	 */
	clrrdi	r4,r5,28		// mask off ESID
	// now check for special ranges	
	ld	r3, EL_kernelPSRStart(r14)
	cmpld   r5,r3
	blt     1f
	ld      r3, EL_kernelPSREnd(r14)
	cmpld   r5,r3
	bge	0f
	ld      r3, EL_kernelPSRVSID(r14)
	b       2f
0:	
	ld	r3, EL_commonPSRStart(r14)
	cmpld   r5,r3
	blt     1f
	ld      r3, EL_commonPSREnd(r14)
	cmpld   r5,r3
	bge	1f
	ld      r3, EL_commonPSRVSID(r14)
	b	2f
	
1:	// construct the vsid
	ori	r3,r4,0x0001		// prepend 0x0001 to obtain VSID
	rldicl	r3,r3,48,0
	// r3 now has the complete VSID word of the STE
2:	
	ori	r4,r4,0x90		// set V=1, T=0, Ks=0, Kp=1
	// r4 now has the complete ESID word of the STE

	mfasr	r5			// get real address of segment table
	clrrdi	r5,r5,1			// clear ASR-V bit (64-bit bridge only)
	rldicl	r15,r4,36,59		// extract 5-bit STEG selector from ESID
	rldimi	r5,r15,7,52		// insert selector into segTable addr

	li	r15,8			// load CTR with number of STEs in STEG
	mtctr	r15
	la	r5,-16(r5)		// prepare for load-with-update

loopSTEG:
	ldu	r15,16(r5)		// load first 8 bytes of STE
	andi.	r15,r15,0x180		// test V bit and bolted bit
	beq	storeSTE		// branch if available (both bits off)
	bdnz	loopSTEG		// loop thru all 8 entries

        /*
	 * No available STE.  Select one at random to evict, using 3 bits from
	 * the Time Base register.  At this point r5 points to the last STE in
	 * the group.  We can simply replace the 3 bits that select an STE
	 * from the group.  We can use "rlwimi" because it doesn't affect the
	 * upper half of the target register.
	 */
	mftb	r15			// get current time base (clock) value
	rlwimi	r5,r15,27,0x70		// insert bits 52-54 into PTEG address
	ld	r15,0(r5)		// make sure it's not bolted
	andi.	r15,r15,0x100		// check for bolted bit on
	beq+    storeSTE		// not bolted, use it

	/*
	 * Randomly-selected entry is bolted.  Search for one that isn't.
	 */
	ori	r5,r5,0x70		// start with last entry in group
loopSTEG2:
	ld	r15,0(r5)		// load first 8 bytes of STE
	andi.	r15,r15,0x100		// check for bolted bit on
	beq+	storeSTE		// not bolted, use it
	subi	r5,r5,16		// back up one entry
	andi.	r15,r5,0x70
	tdeqi	r15,0x70		// panic if steg exhausted
	b	loopSTEG2

storeSTE:
	std r3,8(r5)			// store VSID word of STE
	std r4,0(r5)			// store ESID word of STE

	b	resume3
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
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_exi,r14,r15)
	mfsprg	r15,sprg1		// r15 <- current low-level save area

	std	r0,VS_r0(r15)		// save volatile registers
	std	r1,VS_r1(r15)
	std	r2,VS_r2(r15)
	std	r3,VS_r3(r15)
	std	r4,VS_r4(r15)
	std	r5,VS_r5(r15)
	std	r6,VS_r6(r15)
	// r7 currently in EL
	std	r8,VS_r8(r15)
	std	r9,VS_r9(r15)
	std	r10,VS_r10(r15)
	std	r11,VS_r11(r15)
	std	r12,VS_r12(r15)
	std	r13,VS_r13(r15)

	ld	r3,EL_lolita_r7(r14)	// retrieve original value of r7

	mfsrr0	r4			// pick up original iar
	mfsrr1	r5			// pick up original msr
	mfcr	r6			// pick up cr
	// ctr already in r7
	mflr	r8			// pick up lr
	mfxer	r9			// pick up xer

	// prepare values for rfid
	ld	r1,EL_excIOInterrupt(r14)
	ld	r2,EL_msr(r14)
	mtsrr0	r1			// set iar for rfid
	mtsrr1	r2			// set msr for rfid
	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r14)	// load delta for R-to-V conversion

	mfsprg  r14,sprg2		// restore r14

	std	r3,VS_r7(r15)		// save r7
	std	r4,VS_iar(r15)		// save original iar, msr
	std	r5,VS_msr(r15)
	std	r6,VS_cr(r15)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r15)
	std	r8,VS_lr(r15)
	std	r9,VS_xer(r15)

	add	r1,r15,r0		// construct V->R address of PS

	mfsprg	r15,sprg3		// restore r15
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
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	INC_COUNTER(EL_num_dec,r14,r15)
	mfsprg	r15,sprg1		// r15 <- current low-level save area

	std	r0,VS_r0(r15)		// save volatile registers
	std	r1,VS_r1(r15)
	std	r2,VS_r2(r15)
	std	r3,VS_r3(r15)
	std	r4,VS_r4(r15)
	std	r5,VS_r5(r15)
	std	r6,VS_r6(r15)
	// r7 currently in EL
	std	r8,VS_r8(r15)
	std	r9,VS_r9(r15)
	std	r10,VS_r10(r15)
	std	r11,VS_r11(r15)
	std	r12,VS_r12(r15)
	std	r13,VS_r13(r15)

	ld	r3,EL_lolita_r7(r14)	// retrieve original value of r7

	mfsrr0	r4			// pick up original iar
	mfsrr1	r5			// pick up original msr
	mfcr	r6			// pick up cr
	// ctr already in r7
	mflr	r8			// pick up lr
	mfxer	r9			// pick up xer

	// prepare values for rfid
	ld	r1,EL_excDecInterrupt(r14)
	ld	r2,EL_msr(r14)
	mtsrr0	r1			// set iar for rfid
	mtsrr1	r2			// set msr for rfid
	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r14)	// load delta for R-to-V conversion

	mfsprg  r14,sprg2		// restore r14

	std	r3,VS_r7(r15)		// save r7
	std	r4,VS_iar(r15)		// save original iar, msr
	std	r5,VS_msr(r15)
	std	r6,VS_cr(r15)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r15)
	std	r8,VS_lr(r15)
	std	r9,VS_xer(r15)

	add	r1,r15,r0		// construct V->R address of PS

	mfsprg	r15,sprg3		// restore r15
	rfid
CODE_END(exc_dec_handler)

/*
 * -------------------------------------------------------------
 *  Performance Interrupt
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_perf_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original ctr value
     *	ctr		| r7			| available
     */
	mfsprg	r15,sprg1		// r15 <- current low-level save area

	std	r0,VS_r0(r15)		// save volatile registers
	std	r1,VS_r1(r15)
	std	r2,VS_r2(r15)
	std	r3,VS_r3(r15)
	std	r4,VS_r4(r15)
	std	r5,VS_r5(r15)
	std	r6,VS_r6(r15)
	// r7 currently in EL
	std	r8,VS_r8(r15)
	std	r9,VS_r9(r15)
	std	r10,VS_r10(r15)
	std	r11,VS_r11(r15)
	std	r12,VS_r12(r15)
	std	r13,VS_r13(r15)

	ld	r3,EL_lolita_r7(r14)	// retrieve original value of r7

	mfsrr0	r4			// pick up original iar
	mfsrr1	r5			// pick up original msr
	mfcr	r6			// pick up cr
	// ctr already in r7
	mflr	r8			// pick up lr
	mfxer	r9			// pick up xer

	// prepare values for rfid
	ld	r1,EL_excPerfInterrupt(r14)
	ld	r2,EL_msr(r14)
	mtsrr0	r1			// set iar for rfid
	mtsrr1	r2			// set msr for rfid
	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r14)	// load delta for R-to-V conversion

	mfsprg  r14,sprg2		// restore r14

	std	r3,VS_r7(r15)		// save r7
	std	r4,VS_iar(r15)		// save original iar, msr
	std	r5,VS_msr(r15)
	std	r6,VS_cr(r15)		// save cr, ctr, lr, xer
	std	r7,VS_ctr(r15)
	std	r8,VS_lr(r15)
	std	r9,VS_xer(r15)

	add	r1,r15,r0		// construct V->R address of PS

	mfsprg	r15,sprg3		// restore r15
	rfid
CODE_END(exc_perf_handler)

/*
 * -------------------------------------------------------------
 *  All Interrupts that map to traps
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_trap_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r14		| sprg2			| &exceptionLocal (real)
     *	r15		| sprg3			| available
     *	r7		| exceptionLocal.r7	| original lr value
     *	lr		| r7			| address from vector code
     */
#define EL_TVS(elReg,psOffset) (EL_trapVolatileState+(psOffset))(elReg)
	INC_COUNTER(EL_num_trap,r14,r15)

	std	r6,EL_TVS(r14,VS_r6)	// save r6-r3 (at least temporarily)
	std	r5,EL_TVS(r14,VS_r5)
	std	r4,EL_TVS(r14,VS_r4)
	std	r3,EL_TVS(r14,VS_r3)

	mfcr	r6			// pick up cr

	mfsrr1	r5			// pick up original msr
	mfsrr0	r4			// pick up original iar

	mflr	r3			// pick up address from vector code
	clrrdi	r3,r3,8			// convert to trap number

	andi.	r15,r5,PSL_PR		// test for kernel-mode trap
	beq	trapKernel

	mfsprg	r15,sprg1		// r15 <- current low-level save area

	std	r13,VS_r13(r15)		// save volatile registers
	std	r12,VS_r12(r15)
	std	r11,VS_r11(r15)
	std	r10,VS_r10(r15)
	std	r9,VS_r9(r15)
	std	r8,VS_r8(r15)
	// r7 currently in EL
	// r6-r3 currently in EL_TVS
	std	r2,VS_r2(r15)
	std	r1,VS_r1(r15)
	std	r0,VS_r0(r15)

	mfctr	r8			// pick up ctr, xer
	mfxer	r9

	std	r4,VS_iar(r15)		// save original iar, msr
	std	r5,VS_msr(r15)
	std	r6,VS_cr(r15)		// save cr, lr, ctr, xer
	std	r7,VS_lr(r15)
	std	r8,VS_ctr(r15)
	std	r9,VS_xer(r15)

	// prepare values for rfid
	ld	r4,EL_excTrapExceptionUser(r14)
	ld	r5,EL_msr(r14)

	ld	r6,EL_TVS(r14,VS_r3)	// retrieve original values of r3-r7
	ld	r7,EL_TVS(r14,VS_r4)
	ld	r8,EL_TVS(r14,VS_r5)
	ld	r9,EL_TVS(r14,VS_r6)
	ld	r10,EL_lolita_r7(r14)

	mtsrr0	r4			// set iar for rfid
	mtsrr1	r5			// set msr for rfid
	mfdsisr	r4			// pick up trap info (for EXC_ALI)
	mfdar	r5			// pick up trap aux info (for EXC_ALI)
	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r14)	// load delta for R-to-V conversion

	mfsprg  r14,sprg2		// restore r14

	std	r6,VS_r3(r15)		// save r3-r7
	std	r7,VS_r4(r15)
	std	r8,VS_r5(r15)
	std	r9,VS_r6(r15)
	std	r10,VS_r7(r15)

	add	r1,r15,r0		// construct V->R address of PS

	mfsprg	r15,sprg3		// restore r15

	rfid

trapKernel:
	mfsprg	r15,sprg3		// restore r15

	std	r13,EL_TVS(r14,VS_r13)	// save volatile registers
	std	r12,EL_TVS(r14,VS_r12)
	std	r11,EL_TVS(r14,VS_r11)
	std	r10,EL_TVS(r14,VS_r10)
	std	r9,EL_TVS(r14,VS_r9)
	std	r8,EL_TVS(r14,VS_r8)
	// r7 currently in EL
	// r6-r3 already saved in EL_TVS
	std	r2,EL_TVS(r14,VS_r2)
	std	r1,EL_TVS(r14,VS_r1)
	std	r0,EL_TVS(r14,VS_r0)

	mfctr	r8			// pick up ctr, xer
	mfxer	r9
	ld	r10,EL_lolita_r7(r14)	// pick up original r7

	std	r4,EL_TVS(r14,VS_iar)	// save original iar, msr
	std	r5,EL_TVS(r14,VS_msr)

	// prepare values for rfid
	ld	r4,EL_excTrapExceptionKernel(r14)
	ld	r5,EL_msr(r14)

	std	r6,EL_TVS(r14,VS_cr)	// save cr, lr, ctr, xer
	std	r7,EL_TVS(r14,VS_lr)
	std	r8,EL_TVS(r14,VS_ctr)
	std	r9,EL_TVS(r14,VS_xer)

	mtsrr0	r4			// set iar for rfid
	mtsrr1	r5			// set msr for rfid
	mfdsisr	r4			// pick up trap info (for EXC_ALI)
	mfdar	r5			// pick up trap aux info (for EXC_ALI)
	ld	r2,EL_toc(r14)		// set up kernel TOC for rfid
	ld	r0,EL_vMapsRDelta(r14)	// load delta for R-to-V conversion

	std	r10,EL_TVS(r14,VS_r7)	// save original r7

					// construct V->R address of PS
	la	r1,EL_trapVolatileState(r14)
	add	r1,r1,r0

	mfsprg  r14,sprg2		// restore r14

	rfid
CODE_END(exc_trap_handler)

/*
 * -------------------------------------------------------------
 *  SVCs handled here in lolita
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_lolita_svc_handler)
    /*
     * On entry all registers have their original values except:
     *
     *	register	| preserved in		| current value
     *	-----------------------------------------------------------
     *	r12		| not preserved		| &exceptionLocal (real)
     *	r11		| not preserved		| available
     *	ctr		| not preserved		| available
     *	cr0		| not preserved		| available
     */
	cmpldi	r0,0x1000
	beq	clear_ref_bits
	cmpldi	r0,0x1001
	beq	find_ref_bits
	cmpldi	r0,0x1002
	beq	reclear_ref_bits
	rfid

    clear_ref_bits:
	ld	r11,EL_pte(r12)		// load address of hashtable
	ld	r10,EL_logNumPTEs(r12)
	li	r9,1
	sld	r9,r9,r10
	mtctr	r9
      clear_ref_bits_loop:
	ld	r10,8(r11)
	ori	r10,r10,0x100		// turn on referenced bit
	xori	r10,r10,0x100		// turn off referenced bit
	std	r10,8(r11)
	addi	r11,r11,16
	bdnz	clear_ref_bits_loop
	rfid

    find_ref_bits:
	ld	r11,EL_pte(r12)		// load address of hashtable
	ld	r10,EL_logNumPTEs(r12)
	li	r9,1
	sld	r9,r9,r10
	mtctr	r9
	la	r10,EL_refdPTEArray(r12)
	la	r9,EL_RPA_SIZE(r10)
      find_ref_bits_loop:
	ld	r8,8(r11)
	andi.	r8,r8,0x100		// test referenced bit
	beq+	find_ref_bits_continue
	std	r11,0(r10)
	ld	r8,0(r11)
	std	r8,8(r10)
	ld	r8,8(r11)
	std	r8,16(r10)
	addi	r10,r10,24
	cmpld	r10,r9
	bge-	find_ref_bits_exit
      find_ref_bits_continue:
	addi	r11,r11,16
	bdnz	find_ref_bits_loop
      find_ref_bits_exit:
	la	r3,EL_refdPTEArray(r12)
	sub	r3,r10,r3
	li	r10,24
	divd	r3,r3,r10
	std	r3,EL_refdPTECount(r12)
	rfid

    reclear_ref_bits:
	la	r11,EL_refdPTEArray(r12)
	ld	r10,EL_refdPTECount(r12)
	cmpldi	r10,0
	beq-	reclear_ref_bits_exit
	mtctr	r10
      reclear_ref_bits_loop:
	ld	r10,0(r11)
	ld	r9,8(r10)
	ori	r9,r9,0x100		// turn on referenced bit
	xori	r9,r9,0x100		// turn off referenced bit
	std	r9,8(r10)
	addi	r11,r11,24
	bdnz	reclear_ref_bits_loop
      reclear_ref_bits_exit:
	rfid
CODE_END(exc_lolita_svc_handler)

/*
 * -------------------------------------------------------------
 *  PowerMac G5 Secondary CPU Startup
 * -------------------------------------------------------------
 */
CODE_ENTRY(exc_pmac_smp_code_start)
	li	r5, 0
	oris	r5, r5, cpu_starter@h
	ori	r5, r5, cpu_starter@l
	lwz	r3, 4(r5)

	#
	# Get out of the exception area
	#
	li	r6, 0
	oris	r6, r6, pmac_smp_spin_setup@h
	ori	r6, r6, pmac_smp_spin_setup@l

	li	r0, ((PSL_ISF|PSL_HV|PSL_SF)>>50)
	sldi	r0, r0, 50
	ori	r0, r0, 0x1000	# MSR_ME

	mtsrr0	r6
	mtsrr1	r0
	rfid
CODE_LABEL(exc_pmac_smp_code_end)
CODE_END(exc_pmac_smp_code_start)


CODE_ENTRY(pmac_smp_spin_setup)
	######################################################################
	# secondary processor spin loop
	#	r3 - spin address in BootInfo struct
	#	8(r3) - address of actual spin loop (which has been copied 
	#		into the BootInfo struct so that it will survive fast
	#		reboots)
	######################################################################
	li	r0,0
	sync
	mtspr	0x137,0		# Hypervisor interrupt offset
	isync

	ld	r0, 8(r3)	# load address of actual spin loop into lr
	mtlr	r0

	li	r0, 0
	std	r0, 8(r3)
	sync
	std	r0, 0(r3)	# tell the master processor we're here

    0:	ld	r4, 8(r3)	# wait for timebase from master
	cmpldi	r4, 0
	beq	0b

	srdi	r5, r4, 32	# load timebase with value from master
	mttbl	r0		# zero timebase lower
	mttbu	r5		# set timebase upper
	mttbl	r4		# set timebase lower
	isync			# barrier

	blr			# branch to actual spin loop
CODE_END(pmac_smp_spin_setup)
	
	
CODE_ENTRY(writeReg)
	li	r31,60
	mflr	r29
.LOOP:	
        srd	r0, r30,r31
        rldicl	r24 ,r0, 0, 60
	cmplwi  r0, r24, 9
	ble-	0, .DEC
	addi	r24, r24, 55
	b	.DIF
.DEC:
	addi	r24, r24, 48
.DIF:	
	bl	rwriteChar
        nop
        addi	r24, r31, -4
        extsw	r31, r24
        cmpwi	r0, r31, 0
        bge+	0,.LOOP
	li	r24, '\n'
	bl	rwriteChar
	li	r24, '\r'
	bl	rwriteChar
	mtlr	r29
	blr
CODE_END(writeReg)
	
# R23 -- ser port addr
# R24 -- character to write
# R25-R27 reserved
CODE_ENTRY(rwriteChar)
	li	r23, 0
	ori	r23, r23, 32768
	sldi	r23, r23, 16
	oris	r23, r23, 1
	ori	r23, r23, 12320
	mfspr	r27, 1012	;
	lis	r25, 0x100	;
	sldi	r25, r25, 16	;
	or	r25, r25, r27	; #r25 has bit 23 enabled
	sync			;
	mtspr	1012, r25	;
	isync			;
	eieio			;
	lbz	r26,0(r23)	;
	eieio			;
        andi.	r26,r26,4	;
        beq+	0,-16		;
	eieio			;
	stb	r24,16(r23)	;
	eieio			;
	sync			;
	mtspr	1012, r27	;
	isync			; 
	blr
CODE_END(rwriteChar)

