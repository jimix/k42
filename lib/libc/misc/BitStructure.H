#ifndef __BIT_STRUCTURE_H_
#define __BIT_STRUCTURE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BitStructure.H,v 1.11 2001/10/05 21:47:46 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Machinery for defining and manipulating 64-bit
 * bit-field structures
 * **************************************************************************/

struct BitStructure {
    uval64 data;

    /*
     * In a series of bit-field declarations, the "predecessor" for the first
     * field is specified as "BIT_FIELD_START".  For this name the BIT_FIELD
     * macro expects to find the constant __BIT_FIELD_START_START.  We define
     * the constant here.
     */
    enum {
	__BIT_FIELD_START_START = 64
    };
};

#define __BIT_FIELD_MASK_UNSHIFTED(name) \
    ((uval64(2) << (__##name##_SIZE-1)) - 1)
    /*
     * We use (2 << (size-1)) rather than (1 << size) because the latter
     * results in a gcc compiler warning when size is 64.
     */

#define __BIT_FIELD_MASK(name) \
    (__BIT_FIELD_MASK_UNSHIFTED(name) << __##name##_START)

#define __BIT_FIELD(size,name,predecessor)				      \
    enum {								      \
	__##name##_SIZE = (size),					      \
	__##name##_START = __##predecessor##_START - (size)		      \
    };									      \
									      \
    inline uval								      \
    name(void)								      \
    {									      \
	return (uval) ((data >> __##name##_START) & 			      \
			__BIT_FIELD_MASK_UNSHIFTED(name));		      \
    }									      \
									      \
    inline void								      \
    name(uval newValue)							      \
    {									      \
	data = ((data & ~ __BIT_FIELD_MASK(name)) |			      \
		((uval64(newValue) & __BIT_FIELD_MASK_UNSHIFTED(name)) <<     \
							__##name##_START));   \
    }

#endif /* #ifndef __BIT_STRUCTURE_H_ */
