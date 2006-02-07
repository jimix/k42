#ifndef __GENERIC64_ATOMIC_H_
#define __GENERIC64_ATOMIC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: atomic.h,v 1.4 2002/03/13 15:13:47 marc Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine dependent part of atomic.h
 * **************************************************************************/

void
SyncAfterAcquire()
{
}

void
SyncBeforeRelease()
{
}

/*
 * NOTE:  The ptr parameters to these routines are cast to character pointers
 *        in order to prevent any strict-aliasing optimizations the compiler
 *        might otherwise attempt.
 */

inline uval
_CompareAndStore64(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
    return(0);
}

inline uval
_CompareAndStore32(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
    return(0);
}

inline uval64
_FetchAndStore64(volatile uval64 *ptr, uval64 val)
{
    return(0);
}

inline uval32
_FetchAndStore32(volatile uval32 *ptr, uval32 val)
{
    return(0);
}

inline uval64
_FetchAndAdd64(volatile uval64 *ptr, uval64 val)
{
    return(0);
}

inline uval64
_FetchAndOr64(volatile uval64 *ptr, uval64 val)
{
    return(0);
}

inline uval64
_FetchAndAnd64(volatile uval64 *ptr, uval64 val)
{
    return(0);
}

inline uval32
_FetchAndOr32(volatile uval32 *ptr, uval32 val)
{
    return(0);
}

inline uval32
_FetchAndAnd32(volatile uval32 *ptr, uval32 val)
{
    return(0);
}

inline uval32
_FetchAndAdd32(volatile uval32 *ptr, uval32 val)
{
    return(0);
}

#endif /* #ifndef __GENERIC64_ATOMIC_H_ */
