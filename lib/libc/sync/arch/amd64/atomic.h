#ifndef __AMD64_ATOMIC_H_
#define __AMD64_ATOMIC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: atomic.h,v 1.10 2001/11/28 16:21:33 pdb Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine dependent part of atomic.C
 * **************************************************************************/

#include <misc/utilities.H>

/* Processor consistent machine ordered stores, nothing special to do 
 */
void SyncAfterAcquire()
{
  /* empty body */
}
void SyncBeforeRelease()
{
  /* empty body */
}


/*
 * NOTE:  The ptr parameters to these routines are cast to character pointers
 *        in order to prevent any strict-aliasing optimizations the compiler
 *        might otherwise attempt.
 */
#define __xg(x) ((volatile char *)(x))

#ifdef CONFIG_SMP
#define LOCK_PREFIX "lock ; "
#else /* #ifdef CONFIG_SMP */
#define LOCK_PREFIX ""
#endif /* #ifdef CONFIG_SMP */


/*
 * Note: no "lock" prefix even on SMP: xchg always implies lock anyway
 * Note 2: xchg has side effect, so that attribute volatile is necessary,
 *        but generally the primitive is invalid, *ptr is output argument. --ANK
 */
inline uval64
_FetchAndStore64(volatile uval64 * ptr, uval64 x)
{
  __asm__ __volatile__("xchgq %0,%1"
		       :"=r" (x)
		       :"m" (*__xg(ptr)), "0" (x)
		       :"memory");
  return x;
}

inline uval32
_FetchAndStore32(volatile uval32 * ptr, uval32 x)
{
  __asm__ __volatile__("xchg %0,%1"
		       :"=r" (x)
		       :"m" (*__xg(ptr)), "0" (x)
		       :"memory");
  return x;
}

inline uval64
_CompareAndStore64(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
/*
    if(*ptr == oval) {
        *ptr = nval;
        return 1;
    }
    return 0;
*/
	unsigned long prev;

	__asm__ __volatile__(LOCK_PREFIX "cmpxchgq %1,%2"
                                     : "=a"(prev)
                                     : "q"(nval), "m"(*__xg(ptr)), "0"(oval)
                                     : "memory");
	return (prev == oval);
}

inline uval32
_CompareAndStore32(volatile uval32 *ptr, uval32 oval, uval32 nval)
{
/*
    if(*ptr == oval) {
        *ptr = nval;
        return 1;
    }
    return 0;
*/
	uval32 prev;

	__asm__ __volatile__(LOCK_PREFIX "cmpxchg %1,%2"
                                     : "=a"(prev)
                                     : "q"(nval), "m"(*__xg(ptr)), "0"(oval)
                                     : "memory");
	return (prev == oval);
}

inline uval64
_FetchAndAdd64(volatile uval64 *ptr, uval64 val)
{
    sval64 oval;
    sval64 tmp;

    do {
      oval = *ptr;
      tmp = oval + val;
    }
    while (!_CompareAndStore64(ptr, oval, tmp));
    return oval;
}

inline uval64
_FetchAndOr64(volatile uval64 *ptr, uval64 val)
{
    sval64 oval;
    sval64 tmp;

    do {
      oval = *ptr;
      tmp = oval | val;
    }
    while (!_CompareAndStore64(ptr, oval, tmp));
    return oval;
}

inline uval32
_FetchAndAdd32(volatile uval32 *ptr, uval32 val)
{
    uval32 oval;
    uval32 tmp;

    do {
      oval = *ptr;
      tmp = oval + val;
    }
    while (!_CompareAndStore32(ptr, oval, tmp));
    return oval;
}

inline uval64
_FetchAndAnd64(volatile uval64 *ptr, uval64 val)
{
    sval64 oval;
    sval64 tmp;

    do {
      oval = *ptr;
      tmp = oval & val;
    }
    while (!_CompareAndStore64(ptr, oval, tmp));
    return oval;
}

inline uval32
_FetchAndOr32(volatile uval32 *ptr, uval32 val)
{
    sval32 oval;
    sval32 tmp;

    do {
      oval = *ptr;
      tmp = oval | val;
    }
    while (!_CompareAndStore32(ptr, oval, tmp));
    return oval;
}

inline uval32
_FetchAndAnd32(volatile uval32 *ptr, uval32 val)
{
    sval32 oval;
    sval32 tmp;

    do {
      oval = *ptr;
      tmp = oval & val;
    }
    while (!_CompareAndStore32(ptr, oval, tmp));
    return oval;
}

#endif /* #ifndef __AMD64_ATOMIC_H_ */
