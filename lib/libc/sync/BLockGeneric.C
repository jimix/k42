/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BLockGeneric.C,v 1.7 2003/01/16 19:43:29 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Generic implementation of Bit-Blocking locks, for
 * architectures that don't implement optimized versions (and for
 * debugging builds on all architectures).
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sync/BLock.H>
#include <defines/lock_options.H>

#ifdef ENABLE_LOCK_CONTEXT_ASSERTIONS
void Lock_AssertProperCallingContext()
{
    Scheduler::AssertEnabled();
    Scheduler::AssertNonMigratable();
}
#endif

void
BitBLockGeneric_Acquire(volatile uval64 *lptr,
			uval64 lockBitMask,
			uval64 waitBitMask)
{
    Lock_AssertProperCallingContext();

    uval64 curval;
    curval = FetchAndOr64Volatile(lptr, lockBitMask);
    SyncAfterAcquire();
    if ((curval & lockBitMask) == 0) return;
    BitBLock_SlowAcquireAndFetch(lptr, lockBitMask, waitBitMask, NULL);
}

void
BitBLockGeneric_AcquireAndFetch(volatile uval64 *lptr,
				uval64 lockBitMask,
				uval64 waitBitMask,
				uval64 *datap)
{
    Lock_AssertProperCallingContext();

    uval64 curval;
    curval = FetchAndOr64Volatile(lptr, lockBitMask);
    SyncAfterAcquire();
    *datap = curval;
    if ((curval & lockBitMask) == 0) return;
    BitBLock_SlowAcquireAndFetch(lptr, lockBitMask, waitBitMask, datap);
}

uval
BitBLockGeneric_TryAcquire(volatile uval64 *lptr,
			   uval64 lockBitMask,
			   uval64 waitBitMask)
{
    Lock_AssertProperCallingContext();

    uval64 curval;
    curval = FetchAndOr64Volatile(lptr, lockBitMask);
    SyncAfterAcquire();
    if ((curval & lockBitMask) == 0) return 1;	// SUCCESS
    return 0;	// FAILURE
}

uval
BitBLockGeneric_TryAcquireAndFetch(volatile uval64 *lptr,
				   uval64 lockBitMask,
				   uval64 waitBitMask,
				   uval64 *datap)
{
    Lock_AssertProperCallingContext();

    uval64 curval;
    curval = FetchAndOr64Volatile(lptr, lockBitMask);
    SyncAfterAcquire();
    *datap = curval;
    if ((curval & lockBitMask) == 0) return 1;	// SUCCESS
    return 0;	// FAILURE
}

void
BitBLockGeneric_Release(volatile uval64 *lptr,
			uval64 lockBitMask,
			uval64 waitBitMask)
{
    Lock_AssertProperCallingContext();

    uval64 curval, newval;
    SyncBeforeRelease();
    curval = (*lptr) & ~waitBitMask;
    newval = curval & ~lockBitMask;
    if (CompareAndStore64Volatile(lptr, curval, newval)) return;
    BitBLock_SlowStoreAndRelease(lptr, lockBitMask, waitBitMask, newval);
}

void
BitBLockGeneric_StoreAndRelease(volatile uval64 *lptr,
				uval64 lockBitMask,
				uval64 waitBitMask,
				uval64 data)
{
    Lock_AssertProperCallingContext();

    uval64 curval;
    SyncBeforeRelease();
    curval = (*lptr) & ~waitBitMask;
    if (CompareAndStore64Volatile(lptr, curval, data)) return;
    BitBLock_SlowStoreAndRelease(lptr, lockBitMask, waitBitMask, data);
}
