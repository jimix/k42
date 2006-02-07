#ifndef __BIT_VEC_H_
#define __BIT_VEC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BitVec.H,v 1.6 2004/10/08 21:40:06 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Various things to manipulate bits and bit vectors
 * **************************************************************************/

class BitVec64 {
    enum { BITS_PER_BYTE=8 };

    uval bits;

public:
    BitVec64()			{ clearAll(); }
    uval setSize()		{ return sizeof(bits) * BITS_PER_BYTE; }

    void  setBit( uval b )	{ bits |= ( ((uval) 1) << b); }
    void  clearBit( uval b )	{ bits &= ~(((uval) 1) << b); }
    uval  isBitSet( uval b )	{ return (bits & ( ((uval) 1) << b)) != 0; }
    void  setFirstBits(uval n) { bits = (((uval) 1) << n) - 1; }
    void  clearFirstBits(uval n) { bits = ~((((uval) 1) << n) - 1); }
    void  clearAll( )		{ bits = 0; }
    void  setAll( )		{ bits = ~0; }

    uval findFirstOne( )		// Really should not be inlined
    {
	int i;
	uval b = bits;
        for( i = 0 ; b != 0 ; i++ ) {
	    if ( b & 0x1 ) {
		return i;
	    }
	    b = b >> 1;
	}
	return setSize();
    }

    uval findFirstUnSet( )		// Really should not be inlined
    {
	int i;
	uval b = bits;
        for( i = 0 ; b != 0 ; i++ ) {
	    if ( !(b & 0x1) ) {
		return i;
	    }
	    b = b >> 1;
	}
	return setSize();
    }

    void applyMask(const BitVec64 &mask)
    {
	bits &= mask.bits;
    }

};

#define NUM_BIT_ELEMENTS( n, align )  \
	( (((n) + (align) - 1) & ~((align)-1))/align )
#define BIT_ELEMENT(n,align)          ( (n) / (align) )
#define BIT_OFFSET( n, align  )       ( (n) & ((align) - 1) )
template <uval NUMBITS>
class BitVec {
    enum { BITS_PER_BYTE=8 };
    enum { BITS_PER_ELEMENT=sizeof(uval)*BITS_PER_BYTE };

    uval _elements[ NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT)];
public:
    uval setSize()		{ return NUMBITS; }

    BitVec()
    {
	clearAll();
    }

    void  setBit( uval b )
    {
	_elements[BIT_ELEMENT(b,BITS_PER_ELEMENT)] |=
	    ( ((uval) 1) << (BIT_OFFSET(b,BITS_PER_ELEMENT)) );
    }
    void  clearBit( uval b )
    {
	_elements[BIT_ELEMENT(b,BITS_PER_ELEMENT)] &=
	    ~(((uval) 1) << (BIT_OFFSET(b,BITS_PER_ELEMENT)) );
    }
    uval  isBitSet( uval b )
    {
        uval retvalue;
	retvalue = (_elements[BIT_ELEMENT(b,BITS_PER_ELEMENT)] &
		( ((uval) 1) << (BIT_OFFSET(b,BITS_PER_ELEMENT)))) != 0;
	return(retvalue);
    }
    void  setFirstBits(uval n)
    {
	int i=BIT_ELEMENT(n,BITS_PER_ELEMENT);
	_elements[i++]= ((((uval) 1) << BIT_OFFSET(n,BITS_PER_ELEMENT))-1);
	for(;i<NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT);i++)
	    _elements[i]=0;
	for(i=BIT_ELEMENT(n,BITS_PER_ELEMENT)-1;i>=0;i--)
	    _elements[i]= ~((uval)0);
    }
    void  clearFirstBits(uval n)
    {
        int i=BIT_ELEMENT(n,BITS_PER_ELEMENT);
	_elements[i++]= ~((((uval) 1) << BIT_OFFSET(n,BITS_PER_ELEMENT)) - 1);
	for(;i<NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT);i++)
	    _elements[i]=~((uval)0);
	for(i=BIT_ELEMENT(n,BITS_PER_ELEMENT)-1;i>=0;i--)
	    _elements[i]= 0;
    }
    void  clearAll( )
    {
	for(int i=0; i < NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT); i++ ) {
	    _elements[i]=0;
	}
    }
    void  setAll( )
    {
	for(int i=0; i < NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT); i++ ) {
	    _elements[i]=~(uval)0;
	}
    }
    uval findFirstOne( )		// Really should not be inlined
    {
	int i,j;
	uval b;
	for (j=0;
	     j<NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT);
	     j++)
	{
	    b = _elements[j];
	    for( i = 0 ; b != 0 ; i++ ) {
		if( b & 0x1 )
		    return (i+(j * BITS_PER_ELEMENT));
		b = b >> 1;
	    }
	}
	return setSize();
    }

    void applyMask(const BitVec &mask)
    {
	for(int i=0; i < NUM_BIT_ELEMENTS(NUMBITS,BITS_PER_ELEMENT); i++ ) {
	    _elements[i] &= mask._elements[i];
	}
    }

};

#endif /* #ifndef __BIT_VEC_H_ */

