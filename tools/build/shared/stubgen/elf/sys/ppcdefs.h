#ifndef __PPCDEFS_H_
#define __PPCDEFS_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppcdefs.h,v 1.8 2001/10/05 21:51:42 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

extern int c;

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

/*
 * Data Types Sizes (C and C++)
 * Defines:
 *	_PPC_SZINT
 *	_PPC_SZLONG
 *	_PPC_SZPTR
 *
 * These can take on the values: 32, 64, 128
 */

#ifndef _PPC_SZINT
#define _PPC_SZINT 32
#endif /* #ifndef _PPC_SZINT */

#ifndef _PPC_SZLONG
#define _PPC_SZLONG 32
#endif /* #ifndef _PPC_SZLONG */

#ifndef _PPC_SZPTR
#define _PPC_SZPTR 32
#endif /* #ifndef _PPC_SZPTR */

/*
 * Language Specific
 * Type __psint_t - a pointer sized int - this can be used:
 *	a) when casting a pointer so can perform e.g. a bit operation
 *	b) as a return code for functions incorrectly typed as int but
 *	   return a pointer.
 * User level code can also use the ANSI std ptrdiff_t, defined in stddef.h
 *	in place of __psint_t
 * Type __scint_t - a 'scaling' int - used when in fact one wants an 'int'
 *	that scales when moving to say 64 bit. (e.g. byte counts, bit lens)
 */

#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) || 1)

/*
 * assumes int is 32 -
 * otherwise there must be some other compiler basic type
 */
#if (_PPC_SZINT != 32)
#ifdef _PPC_SZINT
ERROR -- the macro "_PPC_SZINT" is set to _PPC_SZINT -- should be 32
#else /* #ifdef _PPC_SZINT */
ERROR -- the macro "_PPC_SZINT" is unset (currently, must be set to 32)
#endif /* #ifdef _PPC_SZINT */
#endif /* #if (_PPC_SZINT != 32) */

/*
 * Types __int32_t and __uint32_t are already typedef'd on cygwin systems but
 * not on AIX.  We rename them so that we can both typedef and use them on both
 * systems.
 */
#define __int32_t __INT32_T
#define __uint32_t __UINT32_T

typedef int __int32_t;
typedef unsigned  __uint32_t;

#if (_PPC_SZLONG == 64)

typedef long __int64_t;
typedef unsigned long __uint64_t;

#else /* #if (_PPC_SZLONG == 64) */

/*
 * long long is not ANSI so only define 64 bit types if we have ANSI
 * extensions.  _LONGLONG should be defined appropriately by the cc/CC
 * drivers, dependent on the mode of compilation (-xansi, -ansi, etc).
 */

#if !defined(_LONGLONG) && defined(_LANGUAGE_C_PLUS_PLUS) && defined(_SGI_SOURCE)
#define _LONGLONG     1
/* c++ new supports long long */
#endif /* #if !defined(_LONGLONG) && ... */

#if defined(_LONGLONG)

typedef long long __int64_t;
typedef unsigned long long  __uint64_t;

#else /* #if defined(_LONGLONG) */

typedef struct {
        int hi32;
        int lo32;
} __int64_t;
typedef struct {
        unsigned int hi32;
        unsigned int lo32;
} __uint64_t;

#endif /* #if defined(_LONGLONG) */

#endif /* #if (_PPC_SZLONG == 64) */

#if (_PPC_SZPTR == 32)
typedef __int32_t __psint_t;
typedef __uint32_t __psunsigned_t;
#endif /* #if (_PPC_SZPTR == 32) */

#if (_PPC_SZPTR == 64)
typedef __int64_t __psint_t;
typedef __uint64_t __psunsigned_t;
#endif /* #if (_PPC_SZPTR == 64) */

/*
 * If any fundamental type is 64 bit, then set the scaling type
 * to 64 bit
 */
#if (_PPC_SZPTR == 64) || (_PPC_SZLONG == 64) || (_PPC_SZINT == 64)

/* there exists some large fundamental type */
typedef __int64_t __scint_t;
typedef __uint64_t __scunsigned_t;

#else /* #if (_PPC_SZPTR == 64) || (_PPC_SZLONG ... */

/* a 32 bit world */
typedef __int32_t __scint_t;
typedef __uint32_t __scunsigned_t;

#endif /* #if (_PPC_SZPTR == 64) || (_PPC_SZLONG ... */

#endif /* #if (defined(_LANGUAGE_C) || ... */

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #ifndef __PPCDEFS_H_ */
