#ifndef __MACROS_H_
#define __MACROS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: macros.H,v 1.17 2004/06/03 14:37:29 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines a bunch of generally useful macros
 * **************************************************************************/
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif /* #ifndef MIN */
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif /* #ifndef MAX */
#define PAGE_ROUND_UP(x) (((x)-1+PAGE_SIZE) & (~PAGE_MASK))
#define PAGE_ROUND_DOWN(x) ((x) & (~PAGE_MASK))
#define SEGMENT_ROUND_UP(x) (((x)-1+SEGMENT_SIZE) & (~SEGMENT_MASK))
#define SEGMENT_ROUND_DOWN(x) ((x) & (~SEGMENT_MASK))

#define ALIGN_UP(addr,align)   (((uval)(addr) + ((align)-1)) & ~((align)-1))
#define NUM_UVALS(x) (((x) + (sizeof(uval)-1)) / sizeof(uval))

#define ZERO_EXT(x) uval(uval32(x))

// start is lowest bit value number, len goes to the left, place is final
// offset from the lowest bit
#define EXTRACT_BITS( val, start, len, place ) 			\
	(((((uval)(val)) >> (start)) << (sizeof(uval)*8-(len))) \
	>> (sizeof(uval)*8-(len)-(place)))


/*
 * The standard does not allow offsetof to be applied to anything
 * other than a Plain Old Data structure, so we fake gcc out by adding
 * and then substracting a BASE_PTR. This is okay, since we don't do multiple
 * inheritance, which is where a problem may occur.
 */
#define OFFSETOF(TYPE,EL)\
    (((uval) &(((TYPE *) 0xFFFF)->EL)) - 0xFFFF)

#endif /* #ifndef __MACROS_H_ */
