#ifndef __ATOMIC_BIT_VEC_64_H_
#define __ATOMIC_BIT_VEC_64_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AtomicBitVec64.H,v 1.1 2002/07/10 00:37:25 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 64-bit bit vector with atomic ops.
 * **************************************************************************/
#include <sync/atomic.h>

class AtomicBitVec64 {
    enum { BITS_PER_BYTE=8 };

    volatile uval64 bits;

public:
    AtomicBitVec64()	{ clearAll(); };
    uval setSize()	{ return sizeof(bits) * BITS_PER_BYTE; };
    uval64 getBits() { return bits; };
    void  setBit( uval64 b ) {
	FetchAndOr64(&bits, ( ((uval64) 1) << b));
    }

    //Atomically, flip all bits if all are set to 0
    uval flipAllIfZero() {
	return CompareAndStore64(&bits,0ULL,~0ULL);
    }
    AtomicBitVec64 fetchAndSetBit( uval64 b ) {
	AtomicBitVec64 old;
	old.bits = FetchAndOr64(&bits, ( ((uval64) 1) << b));
	return old;
    }
    void  clearBit( uval64 b ) {
	AtomicAnd64(&bits, ~(((uval64) 1) << b));
    }
    uval64  fetchAndClearBit(uval64 b) {
	AtomicBitVec64 old;
	old.bits = FetchAndAnd64(&bits, ~(((uval64) 1) << b));
	return old.bits;
    }
    uval  isBitSet( uval64 b )	{
	return (bits & ( ((uval64) 1) << b)) != 0;
    }
    void  setFirstBits(uval64 n) {
	bits = (((uval64) 1) << n) - 1;
    }
    void  clearFirstBits(uval64 n) {
	bits = ~((((uval64) 1) << n) - 1);
    }

    void  clearAll() {
	bits = 0;
    }
    void  setAll() {
	bits = ~0;
    }

    //Find and set the lowest-order 0 bit
    uval64 setFindFirstUnSet()
    {
	uval64 i = 0;
	uval64 old;
	do {
	    old = bits;
	    uval64 b = old;
	    for ( i = 0 ; i<setSize() ; i++ ) {
		if ( !(b & 0x1) ) {
		    break;
		}
		b = b >> 1;
	    }
	} while (i!= setSize() &&
	       !CompareAndStore64(&bits, old, old | (((uval64)1)<<i)));
	return i;
    }

    uval64 clearFindFirstOne()
    {
	uval64 i = 0;
	uval64 old;
	do {
	    old = bits;
	    uval64 b = old;
	    for ( i = 0 ; i < setSize() ; i++ ) {
		if ( b & 0x1 ) {
		    break;
		}
		b = b >> 1;
	    }
	} while (i!= setSize() &&
	       !CompareAndStore64(&bits, old, old & ~(((uval64)1)<<i)));
	return i;
    }

    uval findFirstOne( )		// Really should not be inlined
    {
	uval i;
	uval b = bits;
	if (!b) return setSize();
        for ( i = 0 ; i < setSize() ; i++ ) {
	    if (b & (0x1ULL << i)) {
		break;
	    }
	}
	return i;
    }

    uval findFirstUnSet( )		// Really should not be inlined
    {
	uval i;
	uval b = bits;
	if (!b) return 0;
        for ( i = 0 ; i < setSize() ; i++ ) {
	    if ( !(b & (0x1ULL << i)) ) {
		break;
	    }
	}
	return i;
    }

    void applyMask(const AtomicBitVec64 &mask)
    {
	AtomicAnd64(&bits, mask.bits);
    }

};
#endif /* #ifndef __ATOMIC_BIT_VEC_64_H_ */

