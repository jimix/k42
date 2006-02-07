/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BLockAsm.S,v 1.7 2000/08/07 14:10:50 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    Optimized machinery for acquiring and releasing BitBLocks.
 * **************************************************************************/

#include <sys/kinclude.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>

/*
 * extern "C" void BitBLock_Acquire(volatile uval64 *lptr,
 *                                  uval64 lockBitMask,
 *                                  uval64 waitBitMask);
 */
C_TEXT_ENTRY(BitBLock_Acquire)
    Acquire_Loop:
	ldarx	r11,0,r3		# current = *lptr	[linked]
	or	r0,r11,r4		# nval = current | lockBitMask
	stdcx.	r0,0,r3			# *lptr = nval		[conditional]
	bne-	Acquire_Loop		# try again if store failed
	isync
	and.	r0,r11,r4		# if ((current & lockBitMask) == 0)
	beqlr+				#     return
	li	r6,0			# datap = NULL (for SlowAcquireAndFetch)
	b	C_TEXT(BitBLock_SlowAcquireAndFetch)
C_TEXT_END(BitBLock_Acquire)

/*
 * extern "C" void BitBLock_AcquireAndFetch(volatile uval64 *lptr,
 *                                          uval64 lockBitMask,
 *                                          uval64 waitBitMask,
 *                                          uval64 *datap);
 */
C_TEXT_ENTRY(BitBLock_AcquireAndFetch)
    AcquireAndFetch_Loop:
	ldarx	r11,0,r3		# current = *lptr	[linked]
	or	r0,r11,r4		# nval = current | lockBitMask
	stdcx.	r0,0,r3			# *lptr = nval		[conditional]
	bne-	AcquireAndFetch_Loop	# try again if store failed
	isync
	std	r11,0(r6)		# *datap = current (speculative)
	and.	r0,r11,r4		# if ((current & lockBitMask) == 0)
	beqlr+				#     return
	b	C_TEXT(BitBLock_SlowAcquireAndFetch)
C_TEXT_END(BitBLock_AcquireAndFetch)

/*
 * extern "C" uval BitBLock_TryAcquire(volatile uval64 *lptr,
 *                                     uval64 lockBitMask,
 *                                     uval64 waitBitMask);
 */
C_TEXT_ENTRY(BitBLock_TryAcquire)
    TryAcquire_Loop:
	ldarx	r11,0,r3		# current = *lptr	[linked]
	or	r0,r11,r4		# nval = current | lockBitMask
	stdcx.	r0,0,r3			# *lptr = nval		[conditional]
	bne-	TryAcquire_Loop		# try again if store failed
	isync
	li	r3,1			# rc = SUCCESS (speculative)
	and.	r0,r11,r4		# if ((current & lockBitMask) == 0)
	beqlr+				#     return
	li	r3,0			# rc = FAILURE
	blr				# return
C_TEXT_END(BitBLock_TryAcquire)

/*
 * extern "C" uval BitBLock_TryAcquireAndFetch(volatile uval64 *lptr,
 *                                             uval64 lockBitMask,
 *                                             uval64 waitBitMask,
 *                                             uval64 *datap);
 */
C_TEXT_ENTRY(BitBLock_TryAcquireAndFetch)
    TryAcquireAndFetch_Loop:
	ldarx	r11,0,r3		# current = *lptr	[linked]
	or	r0,r11,r4		# nval = current | lockBitMask
	stdcx.	r0,0,r3			# *lptr = nval		[conditional]
	bne-	TryAcquireAndFetch_Loop	# try again if store failed
	isync
	li	r3,1			# rc = SUCCESS (speculative)
	std	r11,0(r6)		# *datap = current (speculative)
	and.	r0,r11,r4		# if ((current & lockBitMask) == 0)
	beqlr+				#     return
	li	r3,0			# rc = FAILURE
	blr				# return
C_TEXT_END(BitBLock_TryAcquireAndFetch)

/*
 * extern "C" void BitBLock_Release(volatile uval64 *lptr,
 *                                  uval64 lockBitMask,
 *                                  uval64 waitBitMask);
 */
C_TEXT_ENTRY(BitBLock_Release)
	sync
    Release_Loop:
	ldarx	r11,0,r3		# current = *lptr	[linked]
	and.	r0,r11,r5		# if ((current & waitBitMask) != 0)
	bne-	Release_Slow		#     goto Slow
	andc	r0,r11,r4		# nval = current & ~lockBitMask
	stdcx.	r0,0,r3			# *lptr = nval		[conditional]
	bne-	Release_Loop		# try again if store failed
	blr				# return
    Release_Slow:
	andc	r6,r11,r4		# data = current & ~lockBitMask
	andc	r6,r6,r5		# data = data & ~waitBitMask
	b	C_TEXT(BitBLock_SlowStoreAndRelease)
C_TEXT_END(BitBLock_Release)

/*
 * extern "C" void BitBLock_StoreAndRelease(volatile uval64 *lptr,
 *                                          uval64 lockBitMask,
 *                                          uval64 waitBitMask,
 *                                          uval64 data);
 */
C_TEXT_ENTRY(BitBLock_StoreAndRelease)
	sync
    StoreAndRelease_Loop:
	ldarx	r11,0,r3		# current = *lptr	[linked]
	and.	r0,r11,r5		# if ((current & waitBitMask) != 0)
	bne-	StoreAndRelease_Slow	#     goto Slow
	stdcx.	r6,0,r3			# *lptr = data		[conditional]
	bne-	StoreAndRelease_Loop	# try again if store failed
	blr				# return
    StoreAndRelease_Slow:
	b	C_TEXT(BitBLock_SlowStoreAndRelease)
C_TEXT_END(BitBLock_StoreAndRelease)
