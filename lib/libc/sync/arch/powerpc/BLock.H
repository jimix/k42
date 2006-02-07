#ifndef __BLOCK_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __BLOCK_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BLock.H,v 1.6 2001/10/05 21:48:28 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Blocking locks, this is the standard locking
 * type included everywhere.
 * **************************************************************************/

extern "C" void BitBLock_Acquire(volatile uval64 *lptr,
				 uval64 lockBitMask,
				 uval64 waitBitMask);
extern "C" void BitBLock_AcquireAndFetch(volatile uval64 *lptr,
					 uval64 lockBitMask,
					 uval64 waitBitMask,
					 uval64 *datap);
extern "C" uval BitBLock_TryAcquire(volatile uval64 *lptr,
				    uval64 lockBitMask,
				    uval64 waitBitMask);
extern "C" uval BitBLock_TryAcquireAndFetch(volatile uval64 *lptr,
					    uval64 lockBitMask,
					    uval64 waitBitMask,
					    uval64 *datap);
extern "C" void BitBLock_Release(volatile uval64 *lptr,
				 uval64 lockBitMask,
				 uval64 waitBitMask);
extern "C" void BitBLock_StoreAndRelease(volatile uval64 *lptr,
					 uval64 lockBitMask,
					 uval64 waitBitMask,
					 uval64 data);
