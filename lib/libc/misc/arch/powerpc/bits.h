#ifndef __BITS_H_
#define __BITS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: bits.h,v 1.5 2004/03/14 14:50:09 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine dependent part of bits.h
 * **************************************************************************/

inline uval
FindFirstOne(uval32 vector)
{
    uval bitnum;

    __asm__ ("\n"
	"	neg %0,%1	\n"
	"	and %0,%0,%1	\n"
	"	cntlzw %0,%0	\n"
	"	subfic %0,%0,32	\n"
	: "=&r" (bitnum) : "r" (vector));

    return bitnum;
}

inline uval
FindFirstOne(uval64 vector)
{
    uval bitnum;

    __asm__ ("\n"
	"	neg %0,%1	\n"
	"	and %0,%0,%1	\n"
	"	cntlzd %0,%0	\n"
	"	subfic %0,%0,64	\n"
	: "=&r" (bitnum) : "r" (vector));

    return bitnum;
}

// ffs(~x)

inline uval
FindFirstZero(uval32 vector)
{
    uval bitnum;

    __asm__ ("\n"
	"	addi %0,%1,1	\n"
	"	andc %0,%0,%1	\n"
	"	cntlzw %0,%0	\n"
	"	subfic %0,%0,32	\n"
	: "=&r" (bitnum) : "r" (vector));

    return bitnum;
}

inline uval
FindFirstZero(uval64 vector)
{
    uval bitnum;

    __asm__ ("\n"
	"	addi %0,%1,1	\n"
	"	andc %0,%0,%1	\n"
	"	cntlzd %0,%0	\n"
	"	subfic %0,%0,64	\n"
	: "=&r" (bitnum) : "r" (vector));

    return bitnum;
}

inline uval
CountTrailingZeros(uval32 vector)
{
    uval count;

    __asm__ ("\n"
	"	addi %0,%1,-1	\n"
	"	andc %0,%0,%1	\n"
	"	cntlzw %0,%0	\n"
	"	subfic %0,%0,32	\n"
	: "=&r" (count) : "r" (vector));

    return count;
}

inline uval
CountTrailingZeros(uval64 vector)
{
    uval count;

    __asm__ ("\n"
	"	addi %0,%1,-1	\n"
	"	andc %0,%0,%1	\n"
	"	cntlzd %0,%0	\n"
	"	subfic %0,%0,64	\n"
	: "=&r" (count) : "r" (vector));

    return count;
}

#endif /* #ifndef __BITS_H_ */
