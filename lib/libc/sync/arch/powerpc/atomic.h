#ifndef __POWERPC_ATOMIC_H_
#define __POWERPC_ATOMIC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: atomic.h,v 1.41 2004/01/02 10:46:12 jimix Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine dependent part of atomic.h
 * **************************************************************************/

void
SyncAfterAcquire()
{
    __asm__ __volatile__ ("isync" : : : "memory");
}

void
SyncBeforeRelease()
{
    __asm__ __volatile__ ("sync" : : : "memory");
}

/*
 * NOTE:  The ptr parameters to these routines are cast to character pointers
 *        in order to prevent any strict-aliasing optimizations the compiler
 *        might otherwise attempt.
 */

inline uval
_CompareAndStore64(volatile uval64 *ptr, uval64 oval, uval64 nval)
{
    uval tmp;

    __asm__ ("\n"
	"# _CompareAndStore64						\n"
	"	ldarx	%1,0,%4		# tmp = (*ptr)	[linked]	\n"
	"	cmpld	%1,%2		# if (tmp != oval)		\n"
	"	bne-	$+20		#     goto failure		\n"
	"	stdcx.	%3,0,%4		# (*ptr) = nval	[conditional]	\n"
	"	bne-	$-16		# if (store failed) retry	\n"
	"	li	%1,1		# tmp = SUCCESS			\n"
	"	b	$+8		# goto end			\n"
	"	li	%1,0		# tmp = FAILURE			\n"
	"# end _CompareAndStore64					\n"
	: "=m" (*(char*)ptr), "=&r" (tmp)
	: "r" (oval), "r" (nval), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return tmp;
}

/*
 * NOTE:  The ptr parameters to these routines are cast to character pointers
 *        in order to prevent any strict-aliasing optimizations the compiler
 *        might otherwise attempt.
 */

inline uval
_CompareAndStore32(volatile uval32 *ptr, uval32 oval, uval32 nval)
{
    uval tmp;

    __asm__ ("\n"
	"# _CompareAndStore32						\n"
	"	lwarx	%1,0,%4		# tmp = (*ptr)	[linked]	\n"
	"	cmplw	%1,%2		# if (tmp != oval)		\n"
	"	bne-	$+20		#     goto failure		\n"
	"	stwcx.	%3,0,%4		# (*ptr) = nval	[conditional]	\n"
	"	bne-	$-16		# if (store failed) retry	\n"
	"	li	%1,1		# tmp = SUCCESS			\n"
	"	b	$+8		# goto end			\n"
	"	li	%1,0		# tmp = FAILURE			\n"
	"# end _CompareAndStore32					\n"
	: "=m" (*(char*)ptr), "=&r" (tmp)
	: "r" (oval), "r" (nval), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return tmp;
}

inline uval64
_FetchAndStore64(volatile uval64 *ptr, uval64 val)
{
    uval64 oval;

    __asm__ ("\n"
	"# _FetchAndStore64						\n"
	"	ldarx	%1,0,%3		# oval = (*ptr)	[linked]	\n"
	"	stdcx.	%2,0,%3		# (*ptr) = val	[conditional]	\n"
	"	bne-	$-8		# if (store failed) retry	\n"
	"# end _FetchAndStore64						\n"
	: "=m" (*(char*)ptr), "=&r" (oval)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval32
_FetchAndStore32(volatile uval32 *ptr, uval32 val)
{
    uval32 oval;

    __asm__ ("\n"
	"# _FetchAndStore32						\n"
	"	lwarx	%1,0,%3		# oval = (*ptr)	[linked]	\n"
	"	stwcx.	%2,0,%3		# (*ptr) = val	[conditional]	\n"
	"	bne-	$-8		# if (store failed) retry	\n"
	"# end _FetchAndStore32						\n"
	: "=m" (*(char*)ptr), "=&r" (oval)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval64
_FetchAndAdd64(volatile uval64 *ptr, uval64 val)
{
    sval64 oval;
    sval64 tmp;

    __asm__ ("\n"
	"# _FetchAndAdd64						\n"
	"	ldarx	%1,0,%4		# oval = (*ptr)	[linked]	\n"
	"	add	%2,%1,%3	# tmp = oval + val		\n"
	"	stdcx.	%2,0,%4		# (*ptr) = tmp	[conditional]	\n"
	"	bne-	$-12		# if (store failed) retry	\n"
	"# end _FetchAndAdd64						\n"
	: "=m" (*(char*)ptr), "=&r" (oval), "=&r" (tmp)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval64
_FetchAndOr64(volatile uval64 *ptr, uval64 val)
{
    uval64 oval;
    uval64 tmp;

    __asm__ ("\n"
	"# _FetchAndOr64						\n"
	"	ldarx	%1,0,%4		# oval = (*ptr)	[linked]	\n"
	"	or	%2,%1,%3	# tmp = oval OR mask		\n"
	"	stdcx.	%2,0,%4		# (*ptr) = tmp	[conditional]	\n"
	"	bne-	$-12		# if (store failed) retry	\n"
	"# end _FetchAndOr64						\n"
	: "=m" (*(char*)ptr), "=&r" (oval), "=&r" (tmp)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval64
_FetchAndAnd64(volatile uval64 *ptr, uval64 val)
{
    uval64 oval;
    uval64 tmp;

    __asm__ ("\n"
	"# _FetchAndAnd64						\n"
	"	ldarx	%1,0,%4		# oval = (*ptr)	[linked]	\n"
	"	and	%2,%1,%3	# tmp = oval AND val		\n"
	"	stdcx.	%2,0,%4		# (*ptr) = tmp	[conditional]	\n"
	"	bne-	$-12		# if (store failed) retry	\n"
	"# end _FetchAndAnd64						\n"
	: "=m" (*(char*)ptr), "=&r" (oval), "=&r" (tmp)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval32
_FetchAndOr32(volatile uval32 *ptr, uval32 val)
{
    uval32 oval;
    uval32 tmp;

    __asm__ ("\n"
	"# _FetchAndOr32						\n"
	"	lwarx	%1,0,%4		# oval = (*ptr)	[linked]	\n"
	"	or	%2,%1,%3	# tmp = oval OR val		\n"
	"	stwcx.	%2,0,%4		# (*ptr) = tmp	[conditional]	\n"
	"	bne-	$-12		# if (store failed) retry	\n"
	"# end _FetchAndOr32						\n"
	: "=m" (*(char*)ptr), "=&r" (oval), "=&r" (tmp)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval32
_FetchAndAnd32(volatile uval32 *ptr, uval32 val)
{
    uval32 oval;
    uval32 tmp;

    __asm__ ("\n"
	"# _FetchAndAnd32						\n"
	"	lwarx	%1,0,%4		# oval = (*ptr)	[linked]	\n"
	"	and	%2,%1,%3	# tmp = oval AND val		\n"
	"	stwcx.	%2,0,%4		# (*ptr) = tmp	[conditional]	\n"
	"	bne-	$-12		# if (store failed) retry	\n"
	"# end _FetchAndAnd32						\n"
	: "=m" (*(char*)ptr), "=&r" (oval), "=&r" (tmp)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

inline uval32
_FetchAndAdd32(volatile uval32 *ptr, uval32 val)
{
    uval32 oval;
    uval32 tmp;

    __asm__ ("\n"
	"# _FetchAndAdd32						\n"
	"	lwarx	%1,0,%4		# oval = (*ptr)	[linked]	\n"
	"	add	%2,%1,%3	# tmp = oval + val		\n"
	"	stwcx.	%2,0,%4		# (*ptr) = tmp	[conditional]	\n"
	"	bne-	$-12		# if (store failed) retry	\n"
	"# end _FetchAndAdd32						\n"
	: "=m" (*(char*)ptr), "=&r" (oval), "=&r" (tmp)
	: "r" (val), "r" (ptr), "m" (*(char*)ptr)
	: "cc"
	);

    return oval;
}

#endif /* #ifndef __POWERPC_ATOMIC_H_ */
