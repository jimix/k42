/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: openfirmnote.S,v 1.29 2004/08/20 11:14:52 mostrows Exp $
 *****************************************************************************/

#include <sys/kinclude.H>
#include <misc/asm.h>
#include <misc/hardware.H>
#include <misc/arch/powerpc/asdef.h>

/************************************************************
 * Pad beginning of kernel image for mkboot_chrp scribble area
 * AIX linker always relocates _text symbol to beginning of
 * text section.
 *
 * NB: This entry point MUST appear first in the file because
 *     AIX linker relocates the entire object file as a whole.
 ************************************************************/

CODE_ENTRY(_text)
        nop
        nop
        nop
        nop
	nop
	nop
	nop
	nop
        nop
        nop
        nop
        blr
CODE_END(_text)

/************************************************************
 * Switch to 32-bit mode and call Open Firmware Entry
 * (must be called with EE already disabled)
 *
 *	r3 - RTAS arg list real-mode address
 *	r4 - RTAS base
 *	r5 - RTAS realmode trampoline
 *	r6 - RTAS entry
 ************************************************************/

	.align 2
C_TEXT_ENTRY(openfirmware)
	mflr	r0
	std	r13,-152(r1)
	std	r14,-144(r1)
	std	r15,-136(r1)
	std	r16,-128(r1)
	std	r17,-120(r1)
	std	r0,STK_LR(r1)	# save caller LR
	std	r18,-112(r1)
	std	r19,-104(r1)
	std	r20, -96(r1)
	std	r21, -88(r1)
	mfcr	r0
	std	r22, -80(r1)
	std	r23, -72(r1)
	std	r24, -64(r1)
	std	r25, -56(r1)
	std	r0,STK_CR(r1)	# save caller CR
	std	r26, -48(r1)
	std	r27, -40(r1)
	std	r28, -32(r1)
	std	r29, -24(r1)
	std	r30, -16(r1)
	std	r31,  -8(r1)
	stdu	r1, -200(r1)	# allocate stack frame
	mr	r11,r5		# target IAR: real address of ofreal
	li	r12,(PSL_HV>>50)
	sldi	r12,r12,50
	ori	r12,r12,(PSL_ME|PSL_RI)
				# target MSR: hypv, mchnchk, rcvrbl
#	.long 0x7C0007CE	# SIMOS breakpoint
	std	r2,STK_TOC(r1)	# save caller TOC
	bl	ofrfid		# establish local return address
	ld	r2,STK_TOC(r1)	# restore TOC
#	.long 0x7C0007CE	# SIMOS breakpoint
	la	r1,  200(r1)
	ld	r31,  -8(r1)
	ld	r30, -16(r1)
	ld	r29, -24(r1)
	ld	r0,STK_CR(r1)	# restore CR
	ld	r28, -32(r1)
	ld	r27, -40(r1)
	ld	r26, -48(r1)
	mtcr	r0
	ld	r25, -56(r1)
	ld	r24, -64(r1)
	ld	r23, -72(r1)
	ld	r22, -80(r1)
	ld	r0,STK_LR(r1)	# restore LR
	ld	r21, -88(r1)
	ld	r20, -96(r1)
	ld	r19,-104(r1)
	ld	r18,-112(r1)
	mtlr	r0
	ld	r17,-120(r1)
	ld	r16,-128(r1)
	ld	r15,-136(r1)
	ld	r14,-144(r1)
	ld	r13,-152(r1)
	extsw	r3, r3		# Return signed 64-bit quantity
#	.long 0x7C0007CE	# SIMOS breakpoint
	blr			# return to caller
	.long	0x00000000
ofrfid:
	mfmsr	r31		# preserve MSR
	mflr	r29		# preserve return IAR
	mr	r27,r1		# preserve SP
	cmpwi	0, r6, 0	# cr0 = RTAS entry zero?
	mtctr	r6		# real mode address of OF entry
#	.long 0x7C0007CE	# SIMOS breakpoint
	b	CODE(ExceptionLocal_SafeRFID)
				# jump to ofreal in real mode
C_TEXT_END(openfirmware)

	.align 2
C_TEXT_ENTRY(ofreal)
	sradi	r26,r27,32	# Split SP into r26,r27
	sradi	r28,r29,32	# Split IAR into r28,r29
	sradi	r30,r31,32	# Split MSR into r30,r31
	bne+	0, $+12		# if RTAS entry zero, trap to SimOS
	.long 0x7C0007CA	# .long 0x7C0007CA - SIMOS RTAS emulation
#	b	$+8
#	bctrl			# bctrl - call RTAS service
# following 4 instrs are temporary patch until nmi call is debugged
	b	$+16
	lwz	r0,0(r3)	# get OF op token
	cmplwi	r0,0x30		# compare with ibm-nmi-register
	bnectrl			# bctrl - call RTAS service
	insrdi	r31,r30,32,0	# Concatenate r30,r31
	insrdi	r29,r28,32,0	# Concatenate r28,r29
	insrdi	r27,r26,32,0	# Concatenate r26,r27
	mtsrr1	r31		# MSR before call to openfirmware
	mtsrr0	r29		# IAR from ofrfid call
	mr	r1,r27		# Restore SP
#	.long 0x7C0007CE	# SIMOS breakpoint
	rfid			# return to openfirmware function
C_TEXT_END(ofreal)

#if 0
// r3 rfid to (real value of real_tick)
// r4 io addr (real)
// r5 character
C_TEXT_ENTRY(__tick)
	li	r10,(PSL_HV>>50)
	sldi	r10,r10,50
	ori	r10,r10,(PSL_ME|PSL_RI)
	mtsrr0	r3
	mtsrr1	r10
	mfmsr	r10
	rfid
GLBL_LABEL(real_tick)
	mfspr	r7, 1012	 
	lis	r8, 0x100	
	sldi	r8, r8, 16	
	or	r8, r8, r7	 
	sync			
	mtspr	1012, r8	
	isync			
	eieio			
	lbz	r9,0(r4)	 
	eieio			
        andi.	r9,r9,4		
        beq+	0,-16		
	eieio			
	stb	r5, 16(r4)	
	eieio			
	sync			 
	mtspr	1012, r7	
	isync			 
	
	mtsrr1	r10
	mflr	r4
	mtsrr0	r4
	rfid
C_TEXT_END(__tick)
#endif

