##############################################################################
# K42: (C) Copyright IBM Corp. 2002.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: asm_util.S,v 1.6 2004/02/27 17:14:18 mostrows Exp $
#############################################################################
#define __ASSEMBLY__
#include <sys/kinclude.H>
#include <misc/asm.h>
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
C_TEXT_ENTRY(__exit)
.Lforever:		
	bl      .Lforever			# Sit here forever
C_TEXT_END(__exit)

	############
	# NOTREACHED
	############


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
# copy data 8 bytes at a time
#	r3 - destination address
#	r4 - source address
#	r5 - size of section in double-words
##############################################################################
#
C_TEXT_ENTRY(copyDWords)
	mtctr	r5
	la	r3, -8(r3)	# prepare for store-with-update
	la	r4, -8(r4)	# prepare for load-with-update
copy0:	ldu	r0, 8(r4)	# load word
	stdu	r0, 8(r3)	# store word
	bdnz	copy0

	blr

C_TEXT_END(copyDWords)	

##############################################################################
# Synchronize data and instruction caches for a range of addresses
#	r3 - start address
#	r4 - end address
#	r5 - cache line size
##############################################################################
#
C_TEXT_ENTRY(syncICache)
	sync
sync0:	dcbst	r0,r3		# flush write
	icbi	r0,r3		# invalidate target
	add	r3,r3,r5
	cmpd	r3,r4
	blt	sync0
	isync

	blr

C_TEXT_END(syncICache)

##############################################################################
# Write a charatcter to the Com2 serial port
#	r3 - character to be written
##############################################################################
#
C_TEXT_ENTRY(writeCom2)

	li	r4,0xF8		# get Com2 real address (270's only)
#	li	r4,0x3FD3	# get Com2 real address (Regatta only?)
	sldi	r4,r4,28
	ori	r4,r4,0x2F8

	mfmsr	r10		# get current msr
	li	r0,MSR_SFr56	# load a bit for SF
	sldi    r0,r0,56	# shift to SF position
	or	r0,r10,r0	# or SF bit into msr
	sync
	mtmsrd	r0		# go to 64-bit mode
	isync

#	mfspr	r9,1012		# get hid4 (GP, GQ, GP_UL only)
#	li	r0,1		# get a bit 63
#	sldi	r0,r0,40	# shift to bit 23 (real mode cache inhibit)
#	or	r0,r9,r0	# or bit 23 into hid4
#	sync
#	mtspr	1012,r0		# update hid4
#	isync

#define MCR	4		// Modem Control Register
#define MCR_DTR 0x01		// MCR value for Data-Terminal-Ready
#define LSR	5		// Line Status Register
#define LSR_THRE 0x20		// LSR value for Trans-Holding-Reg-Empty
	li	r0,MCR_DTR	# get MCR_DTR value
	eieio
	stb	r0,MCR(r4)	# write MCR
	eieio
	lbz	r0,LSR(r4)	# get LSR
	eieio
	andi.	r0,r0,LSR_THRE	# and with LSR_THRE value
	beq+	-16		# loop if still transmitting
	eieio
	stb	r3,0(r4)	# write character
	eieio

	sync
	mtspr	1012,r9		# restore hid4
	isync

	sync
	mtmsrd	r10		# restore msr
	isync

	blr			# return

C_TEXT_END(writeCom2)

##############################################################################
# Write a charatcter to the zilog serial port
#	r3 - character to be written
#	r4 - serial port addr
##############################################################################
#
C_TEXT_ENTRY(__writeZilog)
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
	stb	r3,16(r4)	
	eieio			
	sync			 
	mtspr	1012, r7	
	isync		
	blr	 
C_TEXT_END(__writeZilog)
	
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

	mtasr   r9		# segment table address to ASR
	mtsdr1	r8		# page table address to SDR1

	ld	r4, 56(r3)	# vsid0
	ld	r5, 64(r3)	# esid0
	ld	r8, 72(r3)	# vsid1
	ld	r9, 80(r3)	# esid1
	ld	r3, 48(r3)	# bootInfo

	slbia
	sync
	isync
#	tlbia
#	.long 0x7C0002E4
	li	r0,256
	mtctr	r0
	li	r10,0
launch_TLBIA_Loop:
	tlbie	r10
	addi	r10,r10,0x1000
	bdnz	launch_TLBIA_Loop
	sync
	isync

	cmpldi	r4,0		# compare vsid0 with 0
	beq	goRFID		# branch if not software SLB
	mfmsr	r10		# get current msr
	li	r0,MSR_SFr56	# load a bit for SF
	sldi	r0,r0,56	# shift to SF position
	or	r10,r10,r0	# or SF bit into msr
	mtmsrd	r10		# go to 64-bit mode
	isync			# syncronize context
	slbmte	r4,r5		# load 0th SLB entry
	slbmte	r8,r9		# load 1st SLB entry

goRFID:
	mtsrr1	r7		# new 64-bit MSR
	mtsrr0	r6		# load destination PC

	rfid			# Geronimo

C_TEXT_END(launch)

##############################################################################
# secondary processor spin loop
#	r3 - spin address in BootInfo struct
##############################################################################
#
C_TEXT_ENTRY(spin_loop)
	li	r5, MSR_SFr56	# load a bit for SF
	sldi	r5, r5, 56	# shift to SF position
	mtmsrd	r5
	isync

spin1:
	ld	r7, 0(r3)
	cmpdi	cr0, r7, 0
	beq+	cr0, spin1
	isync			# barrier to prevent prefetch

	li	r9, 0
	mttbl	r9		# zero timebase lower
	mttbu	r9		# zero timebase upper
	isync			# barrier
	std	r9, 0(r3)	# clear semaphore
	sync			# make sure that it is seen

spin2:
	ld	r7, 0(r3)
	cmpdi	cr0, r7, 0
	beq+	cr0, spin2
	isync			# barrier to prevent prefetch

	mtlr	r7		# load target into LR
	ld	r3, 8(r3)	# load argument
	blr

C_TEXT_END(spin_loop)

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





