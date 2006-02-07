	.file   "asm_util.s"

##############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
# $Id: asm_util.s,v 1.3 2002/01/16 18:17:06 rosnbrg Exp $
#############################################################################


	.machine "ppc64"

###############################################################################
#  A couple of glue routines:
#      __exit()  Just spins for eternity. It should never be called.
#		But is here for gdb purposes so please leave it.
#      _ptrgl    This is xlc weirdness. Jumps through function pointers
#		all go through this routine.
###############################################################################
#
	S_PROLOG(__exit)
	bl      $			# Sit here forever

	############
	# NOTREACHED
	############

	S_EPILOG
	FCNDES(__exit)

	S_PROLOG(_ptrgl)
	lwz     r0, 0(r11)	# load addr of code of called function.
	stw     r2, 20(r1)	# save current TOC address.
	mtctr   r0		# move branch target to Count register.
	lwz     r2, 4(r11)	# set new TOC address.
	bctr			# go to callee.

	#############
	# NOTREACHED
	#############

	S_EPILOG
	FCNDES(_ptrgl)

##############################################################################
# copy data 8 bytes at a time
#	r3 - destination address
#	r4 - source address
#	r5 - size of section in double-words
##############################################################################
#
	S_PROLOG(copyDWords)
	mtctr	r5
	la	r3, -8(r3)	# prepare for store-with-update
	la	r4, -8(r4)	# prepare for load-with-update
copy0:	ldu	r0, 8(r4)	# load word
	stdu	r0, 8(r3)	# store word
	bdnz	copy0

	blr

	S_EPILOG
	FCNDES(copyDWords)

##############################################################################
# Synchronize data and instruction caches for a range of addresses
#	r3 - start address
#	r4 - end address
#	r5 - cache line size
##############################################################################
#
	S_PROLOG(syncICache)
	sync
sync0:	dcbst	r0,r3		# flush write
	icbi	r0,r3		# invalidate target
	add	r3,r3,r5
	cmpd	r3,r4
	blt	sync0
	isync

	blr

	S_EPILOG
	FCNDES(syncICache)

##############################################################################
# Load OF arguments, target stack, and target TOC
# Calculate MSR
# Jump to entry point
#	r3 - OF params structure
##############################################################################
#
	.machine "ppc64"
	S_PROLOG(launch)

	ld	r9, 0(r3)	# asr
	ld	r8, 8(r3)	# sdr1
	ld	r7, 16(r3)	# msr
	ld	r6, 24(r3)	# iar
	ld	r2, 32(r3)	# toc
	ld	r1, 40(r3)	# stack
	ld	r3, 48(r3)	# bootInfo

	mtasr   r9		# segment table address to ASR
	mtsdr1	r8		# page table address to SDR1

	slbia
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

	mtsrr1	r7		# new 64-bit MSR
	mtsrr0	r6		# load destination PC

	rfid			# Geronimo

	S_EPILOG
	FCNDES(launch)

##############################################################################
# secondary processor spin loop
#	r3 - spin address in BootInfo struct
##############################################################################
#
	S_PROLOG(spin_loop)
	li	r5, 0x1000	# MSR_ME
	li	r10, 1		# something to rotate
	rotrdi	r10, r10, 1	# now SF bit
	or	r5, r5, r10	# or SF into proto MSR
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

	S_EPILOG
	FCNDES(spin_loop)

###############################################################################
#
#      issue a sync instruction
#
###############################################################################
#
	S_PROLOG(sync)
	sync
	blr
	S_EPILOG
	FCNDES(sync)

###############################################################################
#
#      zero timebase on master processor
#
###############################################################################
#
	S_PROLOG(zerotb)
	li	r3, 0
	mttbl	r3		# zero timebase lower
	mttbu	r3		# zero timebase upper
	isync			# barrier
	blr
	S_EPILOG
	FCNDES(zerotb)

###############################################################################
#
#      change status of com1
#
###############################################################################
#
	S_PROLOG(marctest)
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
	S_EPILOG
	FCNDES(marctest)

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

	S_PROLOG(__divi64)
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
	S_EPILOG
	FCNDES(__divi64)





