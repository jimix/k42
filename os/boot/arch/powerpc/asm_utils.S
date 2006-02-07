##############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: asm_utils.S,v 1.14 2005/03/15 08:06:53 mergen Exp $
#############################################################################
#define __ASSEMBLY__
#include "asm32.h"	
#include <sys/kinclude.H>
#include <misc/arch/powerpc/asdef.h>
#define MSR_SFr56 0x80		// MSR SF bit shifted right 56
	
###############################################################################
#  A couple of glue routines:
#      __exit()  Just spins for eternity. It should never be called.
#		But is here for gdb purposes so please leave it.
#      _ptrgl    This is xlc weirdness. Jumps through function pointers
#		all go through this routine.
###############################################################################
#

#define WRITE(char,raddr,rs1,rs2)					\
	eieio			;					\
	lbz rs1,0(raddr)	;					\
	eieio			;					\
        andi. rs2,rs1,4		;					\
        beq+ 0,-16		;					\
	eieio			;					\
	li rs2, char		;					\
	stb rs2,16(raddr)	;					\
	eieio			; 

#define RWRITE(char)							\
	li	r23, 0		;					\
	ori	r23, r23, 32768	;					\
	sldi	r23, r23, 16	;					\
	oris	r23, r23, 1	;					\
	ori	r23, r23, 12320	;					\
	mfspr	r27, 1012	;					\
	lis	r25, 0x100	;					\
	sldi	r25, r25, 16	;					\
	or	r25, r25, r27	; #r25 has bit 23 enabled		\
	sync			;					\
	mtspr	1012, r25	;					\
	isync			;					\
	eieio			;					\
	lbz	r26,0(r23)	;					\
	eieio			;					\
        andi.	r26,r26,4	;					\
        beq+	0,-16		;					\
	eieio			;					\
	li	r24,90		;					\
	stb	r24,16(r23)	;					\
	eieio			;					\
	sync			;					\
	mtspr	1012, r27	;					\
	isync			;					
	

#if 0
#define SHOW_REG(r)							\
	mr r13, r		;					\
	bl writeReg		; 
#else
#define SHOW_REG(r)
#endif		


C_TEXT_ENTRY(getMSR)
	mfmsr	r4
	std	r4, 0(r3)
	blr
C_TEXT_END(getMSR)

C_TEXT_ENTRY(setMSR)
	ld	r4, 0(r3)
	mtmsrd  r4
	blr
C_TEXT_END(setMSR)

C_TEXT_ENTRY(writeReg)
	mr	r30, r13
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
C_TEXT_END(writeReg)
	
# R23 -- ser port addr
# R24 -- character to write
# R25-R27 reserved
C_TEXT_ENTRY(rwriteChar)
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
C_TEXT_END(rwriteChar)


		
C_TEXT_ENTRY(__exit)	
.L_forever:		
	bl      .L_forever			# Sit here forever
C_TEXT_END(__exit)
	############
	# NOTREACHED
	############


##############################################################################
# copy data and flush caches
# (destination overlaps end of source, so copy backwards)
#	r3 - end source address
#	r4 - end destination address
#	r5 - size of section
##############################################################################
#
C_TEXT_ENTRY(relocate)
	mtctr	r5
rel0:	lwzu	r0, -4(r4)	# load word
	stwu	r0, -4(r3)	# store word
	dcbst	r0, r3		# flush write
	sync
	icbi	r0, r3		# invalidate target
	bdnz	rel0
	isync

	blr

C_TEXT_END(relocate)

C_TEXT_ENTRY(_ptrgl)
	lwz     r0, 0(r11)	# load addr of code of called function.
	stw     r2, 20(r1)	# save current TOC address.
	mtctr   r0		# move branch target to Count register.
	lwz     r2, 4(r11)	# set new TOC address.
	bctr			# go to callee.

	#############
	# NOTREACHED
	#############
C_TEXT_END(_ptrgl)


##############################################################################
# reset msr with rfid, jump to realFN()
# Jump to entry point
#	r3 - arg to realFN()
#	r4 - realFN() address 
#	r5 - new msr value ptr
##############################################################################
#
C_TEXT_ENTRY(resetReal)
	rldicl	r6, r6, 0, 32
	mr	r23, r6
	ld	r5, 0(r5)
	mtsrr1	r5
	mtsrr0  r4
	rfid
C_TEXT_END(resetReal)
	
##############################################################################
# Load OF arguments, target stack, and target TOC
# Calculate MSR
# Jump to entry point
#	r3 - OF params structure
##############################################################################
#
C_TEXT_ENTRY(launch)
	ld	r9, 0(r3)	# asr
	ld	r8, 8(r3)	# sdr1
	ld	r7, 16(r3)	# msr
	ld	r6, 24(r3)	# iar
	ld	r2, 32(r3)	# toc
	ld	r1, 40(r3)	# stack

	## asr is non existant on SLB machine and on HV
	## but this intruction is a no-op so we do not guard it
	mtasr   r9		# segment table address to ASR

	cmpldi	r8, 0		# sdr1 == 0 implies onHV == 1
	beq	skip_hvpriv
	mtsdr1	r8		# page table address to SDR1
skip_hvpriv:

	ld	r4, 56(r3)	# vsid0
	ld	r5, 64(r3)	# esid0
	ld	r3, 48(r3)	# bootInfo
	slbia
	sync
	isync

	cmpldi	r4,0		# compare vsid0 with 0
	beq	goRFID		# branch if not software SLB
	mfmsr	r10		# get current msr

	li	r0,MSR_SFr56	# load a bit for SF
	sldi	r0,r0,56	# shift to SF position
	or	r10,r10,r0	# or SF bit into msr bits
	mtmsrd	r10		# go to 64-bit mode
	isync			# syncronize context

	slbmte	r4,r5		# load 0th SLB entry

	clrrdi	r10,r10,60	# preserve current HV bit
	or	r7,r7,r10	# put it into new msr

goRFID:
	mtsrr1	r7		# new 64-bit MSR
	mtsrr0	r6		# load destination PC

	rfid			# Geronimo

C_TEXT_END(launch)

##############################################################################
# secondary processor spin loop
#	r3 - spin address in BootInfo struct
#	8(r3) - address of actual spin loop (which has been copied into the
#               BootInfo struct so that it will survive fast reboots)
##############################################################################
#
C_TEXT_ENTRY(spin_loop)
	li	r0, MSR_SFr56	# load bit for SF
	sldi	r0, r0, 56	# shift to SF position
	mtmsrd	r0
	isync

spin1:
	ld	r0, 0(r3)
	cmpdi	r0, 0
	beq+	spin1
	isync			# barrier to prevent prefetch

	li	r0, 0
	mttbl	r0		# zero timebase lower
	mttbu	r0		# zero timebase upper
	isync			# barrier

	ld	r0, 8(r3)	# branch to actual spin loop
	mtlr	r0
	blr

C_TEXT_END(spin_loop)

###############################################################################
#
#      The following loop is copied into BootInfo and executed there by
#      all secondary processors.
#
###############################################################################
	.globl	spin_loop_start
	.globl	spin_loop_end
spin_loop_start:
	li	r0, 0		# clear value to spin on
	std	r0, 0(r3)
	sync			# make sure that it is seen
.L_subloop:		
	ld	r0, 0(r3)	# loop here
	cmpdi	r0, 0
	beq+	.L_subloop
	isync			# barrier to prevent prefetch
	mtlr	r0		# load target into LR
	li	r0, 1		# acknowledge awakening
	std	r0, 0(r3)
	ld	r3, 8(r3)	# load argument
	blr
spin_loop_end:

###############################################################################
#
#      issue a sync instruction
#
###############################################################################
#
C_TEXT_ENTRY(sync)
	sync
	blr
C_TEXT_END(sync)

###############################################################################
#
#      zero timebase on master processor
#
###############################################################################
#
C_TEXT_ENTRY(zerotb)
	li	r3, 0
	mttbl	r3		# zero timebase lower
	mttbu	r3		# zero timebase upper
	isync			# barrier
	blr
C_TEXT_END(zerotb)

###############################################################################
#
#      change status of com1
#
###############################################################################
#
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
	mfmsr   r9
	ori     r8, r9, 0x10		# turn on data relocate
	mtmsrd  r8
	isync				# Ensure BATs loaded

	li	r4,0
	stb	r4, 0x3FC(r5)		# store value to LEDs
	eieio				# order store
	mtmsrd	r9			# restore MSR
	isync
	mtdbatu 0,r4
	isync
	blr
C_TEXT_END(marctest)

###############################################################################
#
#      64-bit division, remainder
#
#      Invoked in 32-bit mode, so 64-bit operands occupy two regs
#
#      This works because (a) we're on a 64-bit machine and (b) we know
#      we won't be interrupted (and thus the top halves of the regs won't
#      get trashed).
#
#      Input:   
#          Dividend in r3,r4
#          Divisor  in r5,r6
#      Output:
#          Quotient  in r3,r4
#          Remainder in r5,r6
#
###############################################################################

C_TEXT_ENTRY(__divi64)
	insrdi  r4, r3, 32, 0	# assemble dividend in r4
	insrdi  r6, r5, 32, 0	# assemble divisor in r6
	divd 	r0, r4, r6	# quotient r0 <- r4 / r6
	mulld	r3, r0, r6	# r3 <- quotient r0 * divisor r6
	subf	r6, r3, r4	# remainder r6 <- dividend r4 - r3
	extrdi  r5, r6, 32, 0	# upper half of remainder in r5
	extsw	r6, r6		# lower half of remainder in r6
	extrdi	r3, r0, 32, 0	# upper half of quotient  in r3
	extsw	r4, r0		# lower half of quotient  in r4
	blr
C_TEXT_END(__divi64)

C_TEXT_ENTRY(hcall32_enter)
	ld	r5, 8(r4)	#  index
	ld	r6, 16(r4)	#  vsidWord
	ld	r7, 24(r4)	#  rpnWord
	ld	r4, 0(r4)	#  flags
	stw	r3, -8(r1)	#  use the lr slot (is this the right offset?
	li	r3, 8		#  H_ENTER
	.long 0x44000022	#  hcall insn
	lwz	r12, -8(r1)	#  bring back the storage pointer
	std	r4, 0(r12)	#  store index returned
	blr
C_TEXT_END(hcall32_enter)
