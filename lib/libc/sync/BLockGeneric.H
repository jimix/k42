#ifndef __BLOCK_GENERIC_H_
#define __BLOCK_GENERIC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BLockGeneric.H,v 1.5 2001/10/12 20:22:23 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Generic implementation of Bit-Blocking locks, for
 * architectures that don't implement optimized versions.
 * **************************************************************************/

void BitBLockGeneric_Acquire(volatile uval64 *lptr,
			     uval64 lockBitMask,
			     uval64 waitBitMask);
void BitBLockGeneric_AcquireAndFetch(volatile uval64 *lptr,
				     uval64 lockBitMask,
				     uval64 waitBitMask,
				     uval64 *datap);
uval BitBLockGeneric_TryAcquire(volatile uval64 *lptr,
			        uval64 lockBitMask,
				uval64 waitBitMask);
uval BitBLockGeneric_TryAcquireAndFetch(volatile uval64 *lptr,
				        uval64 lockBitMask,
					uval64 waitBitMask,
				        uval64 *datap);
void BitBLockGeneric_Release(volatile uval64 *lptr,
			     uval64 lockBitMask,
			     uval64 waitBitMask);
void BitBLockGeneric_StoreAndRelease(volatile uval64 *lptr,
				     uval64 lockBitMask,
				     uval64 waitBitMask,
				     uval64 data);

inline void BitBLock_Acquire(volatile uval64 *lptr,
			     uval64 lockBitMask,
			     uval64 waitBitMask) {
    BitBLockGeneric_Acquire(lptr, lockBitMask, waitBitMask);
}

inline void BitBLock_AcquireAndFetch(volatile uval64 *lptr,
				     uval64 lockBitMask,
				     uval64 waitBitMask,
				     uval64 *datap) {
    BitBLockGeneric_AcquireAndFetch(lptr, lockBitMask, waitBitMask, datap);
}

inline uval BitBLock_TryAcquire(volatile uval64 *lptr,
				uval64 lockBitMask,
				uval64 waitBitMask) {
    return BitBLockGeneric_TryAcquire(lptr, lockBitMask, waitBitMask);
}

inline uval BitBLock_TryAcquireAndFetch(volatile uval64 *lptr,
					uval64 lockBitMask,
					uval64 waitBitMask,
					uval64 *datap) {
    uval retvalue;
    retvalue = BitBLockGeneric_TryAcquireAndFetch(lptr, lockBitMask, waitBitMask,
									datap);
    return(retvalue);
}

inline void BitBLock_Release(volatile uval64 *lptr,
			     uval64 lockBitMask,
			     uval64 waitBitMask) {
    BitBLockGeneric_Release(lptr, lockBitMask, waitBitMask);
}

inline void BitBLock_StoreAndRelease(volatile uval64 *lptr,
				     uval64 lockBitMask,
				     uval64 waitBitMask,
				     uval64 data) {
    BitBLockGeneric_StoreAndRelease(lptr, lockBitMask, waitBitMask, data);
}

#endif /* #ifndef __BLOCK_GENERIC_H_ */
