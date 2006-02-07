#ifndef __ATOMIC_H_
#define __ATOMIC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: atomic.h,v 1.47 2004/01/16 16:33:07 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *
 * Definitions of small atomic operations.  Normally, these are
 * inlined.  However, sometimes, for debugging or because of a
 * compiler problem, inlining needs to be turned off.  Do this by
 * not defining __INLINED_ATOMIC here.
 *
 * When inlining is in force, there is actually no need for atomic.C
 * but we compile it anyway - it's a good compile check on this code and
 * doesn't hurt anything.  When inlining is off, atomic.C provides the only
 * definitions of the functions and must be built.
 *
 * CompareAndStore operations return 1 on success, 0 on failure.
 * **************************************************************************/

#define __INLINED_ATOMIC

#if defined(__INLINED_ATOMIC) && !defined(__COMPILE_ATOMIC_C)
#define INLINE inline
#else /* #if defined(__INLINED_ATOMIC) && ... */
#define INLINE
#endif /* #if defined(__INLINED_ATOMIC) && ... */

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

/*
 * FIXME:  We no longer believe that it's possible to define a
 *         machine-independent interface for all the types of memory
 *         synchronization that are needed on different weakly-consistent
 *         machines.  All uses of these atomic operations that require
 *         memory synchronization should be subsumed into machine-dependent
 *         higher-level functions that know what synchronization is
 *         required.
 */

/*
 * Synchronization operations needed after a logical "acquire" operation
 * and before a logical "release" operation.
 */
INLINE void SyncAfterAcquire();
INLINE void SyncBeforeRelease();

/*
 * Functions guaranteed to load a value from memory.
 */
INLINE uval64 FetchAndNop64(volatile uval64 *ptr);
INLINE uval FetchAndNop(volatile uval *ptr);
INLINE sval FetchAndNopSigned(volatile sval *ptr);

/*
 * The base versions of these atomic operations make no guarantees about
 * memory consistency with respect to locations other than the actual
 * location on which the operation is performed.  In particular, they do not
 * necessarily "kill" all of memory as far as the C compiler is concerned
 * and they do not do any sort of hardware memory synchronization.
 *
 */
/* ----- operations on natural units ------------ */
INLINE uval CompareAndStore(volatile uval *ptr, uval oval, uval nval);
INLINE uval Swap(volatile uval *ptr, uval nval);
INLINE uval FetchAndClear(volatile uval *ptr);
INLINE uval FetchAndAdd(volatile uval* ptr, uval val);
INLINE sval FetchAndAddSigned(volatile sval* ptr, sval val);
INLINE void AtomicAdd(volatile uval *ptr, uval val);
/* ----- operations on 64-bit quantities ------------ */
INLINE uval CompareAndStore64(volatile uval64 *ptr, uval64 oval, uval64 nval);
INLINE uval64 FetchAndClear64(volatile uval64 *ptr);
INLINE uval64 FetchAndStore64(volatile uval64 *ptr, uval64 val);
INLINE uval64 FetchAndAdd64(volatile uval64 *ptr, uval64 val);
INLINE sval64 FetchAndAddSigned64(volatile sval64 *ptr, sval64 val);
INLINE uval64 FetchAndOr64(volatile uval64 *ptr, uval64 val);
INLINE uval64 FetchAndAnd64(volatile uval64 *ptr, uval64 val);
INLINE void AtomicAnd64(volatile uval64 *ptr, uval64 val);
/* ----- operations on 32-bit quantities ------------ */
INLINE uval32 CompareAndStore32(volatile uval32 *ptr, uval32 oval, uval32 nval);
INLINE uval32 Swap32(volatile uval32 *ptr, uval32 nval);
INLINE uval32 FetchAndClear32(volatile uval32 *ptr);
INLINE uval32 FetchAndOr32(volatile uval32 *ptr, uval32 val);
INLINE void AtomicOr32(volatile uval32 *ptr, uval32 val);
INLINE void AtomicAnd32(volatile uval32 *ptr, uval32 val);
INLINE void AtomicAdd32(volatile uval32 *ptr, uval32 val);

/*
 * The "Volatile" versions of these atomic operations "kill" all of memory
 * as far as the C compiler is concerned (forcing reloads of all values
 * cached in registers), but they do not do any additional memory
 * synchronization.
 */
/* ----- operations on natural units ------------ */
INLINE uval CompareAndStoreVolatile(volatile uval *ptr,
				    uval oval, uval nval);
INLINE uval SwapVolatile(volatile uval *ptr, uval nval);
INLINE uval FetchAndClearVolatile(volatile uval *ptr);
INLINE uval FetchAndAddVolatile(volatile uval* ptr, uval val);
INLINE sval FetchAndAddSignedVolatile(volatile sval* ptr, sval val);
INLINE void AtomicAddVolatile(volatile uval *ptr, uval val);
/* ----- operations on 64-bit quantities ------------ */
INLINE uval CompareAndStore64Volatile(volatile uval64 *ptr,
				      uval64 oval, uval64 nval);
INLINE uval64 FetchAndClear64Volatile(volatile uval64 *ptr);
INLINE uval64 FetchAndStore64Volatile(volatile uval64 *ptr, uval64 val);
INLINE uval64 FetchAndAdd64Volatile(volatile uval64 *ptr, uval64 val);
INLINE sval64 FetchAndAddSigned64Volatile(volatile sval64 *ptr, sval64 val);
INLINE uval64 FetchAndOr64Volatile(volatile uval64 *ptr, uval64 val);
INLINE void AtomicAnd64Volatile(volatile uval64 *ptr, uval64 val);
/* ----- operations on 32-bit quantities ------------ */
INLINE uval32 CompareAndStore32Volatile(volatile uval32 *ptr,
				      uval32 oval, uval32 nval);
INLINE uval32 Swap32Volatile(volatile uval32 *ptr, uval32 nval);
INLINE uval32 FetchAndClear32Volatile(volatile uval32 *ptr);
INLINE uval32 FetchAndOr32Volatile(volatile uval32 *ptr, uval32 val);
INLINE void AtomicOr32Volatile(volatile uval32 *ptr, uval32 val);
INLINE void AtomicAnd32Volatile(volatile uval32 *ptr, uval32 val);
INLINE void AtomicAdd32Volatile(volatile uval32 *ptr, uval32 val);

/*
 * The "Synced" versions of these atomic operations "kill" all of memory
 * and also perform whatever memory synchronization operations necessary
 * on weakly-consistent machines.
 */
/* ----- operations on natural units ------------ */
INLINE uval CompareAndStoreSynced(volatile uval *ptr,
				  uval oval, uval nval);
INLINE uval SwapSynced(volatile uval *ptr, uval nval);
INLINE uval FetchAndClearSynced(volatile uval *ptr);
INLINE uval FetchAndAddSynced(volatile uval* ptr, uval val);
INLINE sval FetchAndAddSignedSynced(volatile sval* ptr, sval val);
INLINE void AtomicAddSynced(volatile uval *ptr, uval val);
/* ----- operations on 64-bit quantities ------------ */
INLINE uval CompareAndStore64Synced(volatile uval64 *ptr,
				    uval64 oval, uval64 nval);
INLINE uval64 FetchAndStore64Synced(volatile uval64 *ptr, uval64 val);
INLINE uval64 FetchAndClear64Synced(volatile uval64 *ptr);
INLINE uval64 FetchAndAdd64Synced(volatile uval64 *ptr, uval64 val);
INLINE sval64 FetchAndAddSigned64Synced(volatile sval64 *ptr, sval64 val);
INLINE uval64 FetchAndOr64Synced(volatile uval64 *ptr, uval64 val);
INLINE void AtomicAnd64Synced(volatile uval64 *ptr, uval64 val);
/* ----- operations on 32-bit quantities ------------ */
INLINE uval32 CompareAndStore32Synced(volatile uval32 *ptr,
				      uval32 oval, uval32 nval);
INLINE uval32 Swap32Synced(volatile uval32 *ptr, uval32 nval);
INLINE uval32 FetchAndClear32Synced(volatile uval32 *ptr);
INLINE uval32 FetchAndOr32Synced(volatile uval32 *ptr, uval32 val);
INLINE void AtomicOr32Synced(volatile uval32 *ptr, uval32 val);
INLINE void AtomicAnd32Synced(volatile uval32 *ptr, uval32 val);
INLINE void AtomicAdd32Synced(volatile uval32 *ptr, uval32 val);
INLINE uval32 FetchAndAdd32(volatile uval32 *ptr, uval32 val);
#undef INLINE

#if defined(__INLINED_ATOMIC) || defined(__COMPILE_ATOMIC_C)

/*
 * The machine-dependent atomic.h provides implementations of the basic
 * operations needed to support all the machine-independent interfaces.
 * They all operate on unsigned operands that are explicitly 32 bits or
 * 64 bits in size.  There are no "natural units" or signed versions.
 * The machine-dependent routines have an underscore prefix and are
 * unconditionally inlined.
 */

#include __MINC(atomic.h)

uval64
FetchAndNop64(volatile uval64 *ptr)
{
    return *(volatile uval64 *) ptr;
}

uval
FetchAndNop(volatile uval *ptr)
{
    return *(volatile uval *) ptr;
}

sval
FetchAndNopSigned(volatile sval *ptr)
{
    return *(volatile sval *) ptr;
}

uval
CompareAndStore(volatile uval *ptr, uval oval, uval nval)
{
    uval retvalue;
#if (_SIZEUVAL == 8)
    retvalue = _CompareAndStore64((volatile uval64 *) ptr,
			      (uval64) oval, (uval64) nval);
#else /* #if (_SIZEUVAL == 8) */
    retvalue =  _CompareAndStore32((volatile uval32 *) ptr,
			      (uval32) oval, (uval32) nval);
#endif /* #if (_SIZEUVAL == 8) */
    return (retvalue);
}

uval
Swap(volatile uval *ptr, uval nval)
{
#if (_SIZEUVAL == 8)
    return (uval) _FetchAndStore64((volatile uval64 *) ptr, (uval64) nval);
#else /* #if (_SIZEUVAL == 8) */
    return (uval) _FetchAndStore32((volatile uval32 *) ptr, (uval32) nval);
#endif /* #if (_SIZEUVAL == 8) */
}

uval
FetchAndClear(volatile uval *ptr)
{
#if (_SIZEUVAL == 8)
    return (uval) _FetchAndStore64((volatile uval64 *) ptr, (uval64) 0);
#else /* #if (_SIZEUVAL == 8) */
    return (uval) _FetchAndStore32((volatile uval32 *) ptr, (uval32) 0);
#endif /* #if (_SIZEUVAL == 8) */
}

uval
FetchAndAdd(volatile uval* ptr, uval val)
{
#if (_SIZEUVAL == 8)
    return (uval) _FetchAndAdd64((volatile uval64 *) ptr, (uval64) val);
#else /* #if (_SIZEUVAL == 8) */
    return (uval) _FetchAndAdd32((volatile uval32 *) ptr, (uval32) val);
#endif /* #if (_SIZEUVAL == 8) */
}

sval
FetchAndAddSigned(volatile sval* ptr, sval val)
{
    return (sval) FetchAndAdd((volatile uval *) ptr, (uval) val);
}

void
AtomicAdd(volatile uval *ptr, uval val)
{
#if (_SIZEUVAL == 8)
    (void) _FetchAndAdd64((volatile uval64 *) ptr, (uval64) val);
#else /* #if (_SIZEUVAL == 8) */
    (void) _FetchAndAdd32((volatile uval32 *) ptr, (uval32) val);
#endif /* #if (_SIZEUVAL == 8) */
}

uval
CompareAndStore64(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
    return _CompareAndStore64(ptr, oval, nval);
}

uval64
FetchAndClear64(volatile uval64 *ptr)
{
    return _FetchAndStore64(ptr, (uval64) 0);
}

uval64
FetchAndStore64(volatile uval64 *ptr, uval64 val)
{
    return _FetchAndStore64(ptr, val);
}

uval64
FetchAndAdd64(volatile uval64 *ptr, uval64 val)
{
    return _FetchAndAdd64(ptr, val);
}

sval64
FetchAndAddSigned64(volatile sval64 *ptr, sval64 val)
{
    return (sval64) _FetchAndAdd64((volatile uval64 *) ptr, (uval64) val);
}

uval64
FetchAndOr64(volatile uval64 *ptr, uval64 val)
{
    return _FetchAndOr64(ptr, val);
}

uval64
FetchAndAnd64(volatile uval64 *ptr, uval64 val)
{
    return _FetchAndAnd64(ptr, val);
}

void
AtomicAnd64(volatile uval64 *ptr, uval64 val)
{
    (void) _FetchAndAnd64(ptr, val);
}

uval32
CompareAndStore32(volatile uval32 *ptr, uval32 oval, uval32 nval)
{
    return _CompareAndStore32(ptr, oval, nval);
}

uval32
Swap32(volatile uval32 *ptr, uval32 nval)
{
    return _FetchAndStore32(ptr, nval);
}

uval32
FetchAndClear32(volatile uval32 *ptr)
{
    return _FetchAndStore32(ptr, 0);
}

uval32
FetchAndOr32(volatile uval32 *ptr, uval32 val)
{
    return _FetchAndOr32(ptr, val);
}

void
AtomicOr32(volatile uval32 *ptr, uval32 val)
{
    (void) _FetchAndOr32(ptr, val);
}

void
AtomicAnd32(volatile uval32 *ptr, uval32 val)
{
    (void) _FetchAndAnd32(ptr, val);
}

void
AtomicAdd32(volatile uval32 *ptr, uval32 val)
{
    (void) _FetchAndAdd32(ptr, val);
}

uval32
FetchAndAdd32(volatile uval32 *ptr, uval32 val)
{
    return _FetchAndAdd32(ptr, val);
}

inline void
KillMemory()
{
    __asm__ __volatile__ ("# KillMemory" : : : "memory");
}

uval
CompareAndStoreVolatile(volatile uval *ptr, uval oval, uval nval)
{
    uval rval;
    KillMemory();
    rval = CompareAndStore(ptr, oval, nval);
    KillMemory();
    return rval;
}

uval
SwapVolatile(volatile uval *ptr, uval nval)
{
    uval rval;
    KillMemory();
    rval = Swap(ptr, nval);
    KillMemory();
    return rval;
}

uval
FetchAndClearVolatile(volatile uval *ptr)
{
    uval rval;
    KillMemory();
    rval = FetchAndClear(ptr);
    KillMemory();
    return rval;
}

uval
FetchAndAddVolatile(volatile uval* ptr, uval val)
{
    uval rval;
    KillMemory();
    rval = FetchAndAdd(ptr, val);
    KillMemory();
    return rval;
}

sval
FetchAndAddSignedVolatile(volatile sval* ptr, sval val)
{
    sval rval;
    KillMemory();
    rval = FetchAndAddSigned(ptr, val);
    KillMemory();
    return rval;
}

void
AtomicAddVolatile(volatile uval *ptr, uval val)
{
    KillMemory();
    (void) AtomicAdd(ptr, val);
    KillMemory();
}

uval
CompareAndStore64Volatile(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
    uval rval;
    KillMemory();
    rval = CompareAndStore64(ptr, oval, nval);
    KillMemory();
    return rval;
}

uval64
FetchAndClear64Volatile(volatile uval64 *ptr)
{
    uval64 rval;
    KillMemory();
    rval = FetchAndClear64(ptr);
    KillMemory();
    return rval;
}

uval64
FetchAndStore64Volatile(volatile uval64 *ptr, uval64 val)
{
    uval64 rval;
    KillMemory();
    rval = FetchAndStore64(ptr, val);
    KillMemory();
    return rval;
}

uval64
FetchAndAdd64Volatile(volatile uval64 *ptr, uval64 val)
{
    uval64 rval;
    KillMemory();
    rval = FetchAndAdd64(ptr, val);
    KillMemory();
    return rval;
}

sval64
FetchAndAddSigned64Volatile(volatile sval64 *ptr, sval64 val)
{
    sval64 rval;
    KillMemory();
    rval = FetchAndAddSigned64(ptr, val);
    KillMemory();
    return rval;
}

uval64
FetchAndOr64Volatile(volatile uval64 *ptr, uval64 val)
{
    uval64 rval;
    KillMemory();
    rval = FetchAndOr64(ptr, val);
    KillMemory();
    return rval;
}

void
AtomicAnd64Volatile(volatile uval64 *ptr, uval64 val)
{
    KillMemory();
    (void) AtomicAnd64(ptr, val);
    KillMemory();
}

uval32
CompareAndStore32Volatile(volatile uval32 *ptr, uval32 oval, uval32 nval)
{
    uval rval;
    KillMemory();
    rval = CompareAndStore32(ptr, oval, nval);
    KillMemory();
    return rval;
}

uval32
Swap32Volatile(volatile uval32 *ptr, uval32 nval)
{
    uval32 rval;
    KillMemory();
    rval = Swap32(ptr, nval);
    KillMemory();
    return rval;
}

uval32
FetchAndClear32Volatile(volatile uval32 *ptr)
{
    uval32 rval;
    KillMemory();
    rval = FetchAndClear32(ptr);
    KillMemory();
    return rval;
}

uval32
FetchAndOr32Volatile(volatile uval32 *ptr, uval32 val)
{
    uval32 rval;
    KillMemory();
    rval = FetchAndOr32(ptr, val);
    KillMemory();
    return rval;
}

void
AtomicOr32Volatile(volatile uval32 *ptr, uval32 val)
{
    KillMemory();
    (void) AtomicOr32(ptr, val);
    KillMemory();
}

void
AtomicAnd32Volatile(volatile uval32 *ptr, uval32 val)
{
    KillMemory();
    (void) AtomicAnd32(ptr, val);
    KillMemory();
}

void
AtomicAdd32Volatile(volatile uval32 *ptr, uval32 val)
{
    KillMemory();
    (void) AtomicAdd32(ptr, val);
    KillMemory();
}


uval
CompareAndStoreSynced(volatile uval *ptr, uval oval, uval nval)
{
    uval rval;
    SyncBeforeRelease();
    rval = CompareAndStoreVolatile(ptr, oval, nval);
    SyncAfterAcquire();
    return rval;
}

uval
SwapSynced(volatile uval *ptr, uval nval)
{
    uval rval;
    SyncBeforeRelease();
    rval = SwapVolatile(ptr, nval);
    SyncAfterAcquire();
    return rval;
}

uval
FetchAndClearSynced(volatile uval *ptr)
{
    uval rval;
    SyncBeforeRelease();
    rval = FetchAndClearVolatile(ptr);
    SyncAfterAcquire();
    return rval;
}

uval
FetchAndAddSynced(volatile uval* ptr, uval val)
{
    uval rval;
    SyncBeforeRelease();
    rval = FetchAndAddVolatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

sval
FetchAndAddSignedSynced(volatile sval* ptr, sval val)
{
    sval rval;
    SyncBeforeRelease();
    rval = FetchAndAddSignedVolatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

void
AtomicAddSynced(volatile uval *ptr, uval val)
{
    SyncBeforeRelease();
    AtomicAddVolatile(ptr, val);
    SyncAfterAcquire();
}

uval
CompareAndStore64Synced(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
    uval rval;
    SyncBeforeRelease();
    rval = CompareAndStore64Volatile(ptr, oval, nval);
    SyncAfterAcquire();
    return rval;
}

uval64
FetchAndClear64Synced(volatile uval64 *ptr)
{
    uval64 rval;
    SyncBeforeRelease();
    rval = FetchAndClear64Volatile(ptr);
    SyncAfterAcquire();
    return rval;
}

uval64
FetchAndStore64Synced(volatile uval64 *ptr, uval64 val)
{
    uval64 rval;
    SyncBeforeRelease();
    rval = FetchAndStore64Volatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

uval64
FetchAndAdd64Synced(volatile uval64 *ptr, uval64 val)
{
    uval64 rval;
    SyncBeforeRelease();
    rval = FetchAndAdd64Volatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

sval64
FetchAndAddSigned64Synced(volatile sval64 *ptr, sval64 val)
{
    sval64 rval;
    SyncBeforeRelease();
    rval = FetchAndAddSigned64Volatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

uval64
FetchAndOr64Synced(volatile uval64 *ptr, uval64 val)
{
    uval64 rval;
    SyncBeforeRelease();
    rval = FetchAndOr64Volatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

void
AtomicAnd64Synced(volatile uval64 *ptr, uval64 val)
{
    SyncBeforeRelease();
    AtomicAnd64Volatile(ptr, val);
    SyncAfterAcquire();
}

uval32
CompareAndStore32Synced(volatile uval32 *ptr, uval32 oval, uval32 nval)
{
    uval32 rval;
    SyncBeforeRelease();
    rval = CompareAndStore32Volatile(ptr, oval, nval);
    SyncAfterAcquire();
    return rval;
}

uval32
Swap32Synced(volatile uval32 *ptr, uval32 nval)
{
    uval32 rval;
    SyncBeforeRelease();
    rval = Swap32Volatile(ptr, nval);
    SyncAfterAcquire();
    return rval;
}

uval32
FetchAndClear32Synced(volatile uval32 *ptr)
{
    uval32 rval;
    SyncBeforeRelease();
    rval = FetchAndClear32Volatile(ptr);
    SyncAfterAcquire();
    return rval;
}

uval32
FetchAndOr32Synced(volatile uval32 *ptr, uval32 val)
{
    uval32 rval;
    SyncBeforeRelease();
    rval = FetchAndOr32Volatile(ptr, val);
    SyncAfterAcquire();
    return rval;
}

void
AtomicOr32Synced(volatile uval32 *ptr, uval32 val)
{
    SyncBeforeRelease();
    AtomicOr32Volatile(ptr, val);
    SyncAfterAcquire();
}

void
AtomicAnd32Synced(volatile uval32 *ptr, uval32 val)
{
    SyncBeforeRelease();
    AtomicAnd32Volatile(ptr, val);
    SyncAfterAcquire();
}

void
AtomicAdd32Synced(volatile uval32 *ptr, uval32 val)
{
    SyncBeforeRelease();
    AtomicAdd32Volatile(ptr, val);
    SyncAfterAcquire();
}

#endif /* #if defined(__INLINED_ATOMIC) || ... */

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef __ATOMIC_H_ */
