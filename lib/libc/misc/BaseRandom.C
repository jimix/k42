/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BaseRandom.C,v 1.4 2002/10/10 13:08:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Re-entrant random number generator
 * This code was taken from glibc and ported almost unrecognizably :].
 * Origin copyright and Derivation statement follows.
 * **************************************************************************/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * This is derived from the Berkeley source:
 *	@(#)random.c	5.5 (Berkeley) 7/6/88
 * It was reworked for the GNU C Library by Roland McGrath.
 * Rewritten to be reentrant by Ulrich Drepper, 1995
 */

/*
 * Basically this is the equivalent of using glibc's random_r(3)
 * initializing the state by calling initstate_r(3) with a state
 * buffer 246 bytes in size.
 */


#include <sys/sysIncs.H>
#include <misc/BaseRandom.H>

void
BaseRandom::init(uval seed)
{
    sval32 word;
    sval32 *dst;
    sval kc;

    type = 4;
    degree =  63;
    separation = 1;
    stateArray = &stateMem[1];
    endPtr = &stateArray[degree];

    if (seed == 0) {
	seed = 1;
    }
    dst = stateArray;

    word = seed;
    kc = degree;

    for (sval i = 1; i < kc; i++) {
	sval hi =  word / 127773;
	sval lo = word % 127773;
	word = 16807 * lo - 2836 * hi;
	if (word < 0) {
	    word += ((uval32)(~0)) >> 1; //2147483647;
	}
	*++dst = word;
    }

    frontPtr = &stateArray[separation];
    rearPtr = &stateArray[0];
    kc *= 10;
    while (--kc >= 0) {
	// throw out the first set of numbers
	(void) getVal();
    }

    stateArray[-1] = (*rearPtr - (uval)stateArray) * 5 + 4;
};

sval32
BaseRandom::getVal()
{
    sval32 result;
    sval32 *fptr = frontPtr;
    sval32 *rptr = rearPtr;
    sval32 val;

    *fptr += *rptr;
    val = *fptr;
    /* Chucking least random bit.  */
    result = (val >> 1) & (uval32(~0) >> 1);
    ++fptr;
    if (fptr >= endPtr) {
	fptr = stateArray;
	++rptr;
    } else {
	++rptr;
	if (rptr >= endPtr) {
	    rptr = stateArray;
	}
    }
    frontPtr = fptr;
    rearPtr = rptr;

    return result;
}



