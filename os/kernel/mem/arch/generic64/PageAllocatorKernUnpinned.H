#ifndef PAGE_ALLOCATOR_KERN_PINNED_DEFH
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef PAGE_ALLOCATOR_KERN_PINNED_DEFH */

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernUnpinned.H,v 1.2 2001/10/19 20:02:13 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: machine-dependent component of unpinned allocator
 * **************************************************************************/

#define LOG_SIZE_UNPINNED 	26	// must be quasi-infinite in 64 bit
#define SIZE_UNPINNED 		(1UL << LOG_SIZE_UNPINNED)
#define REGION_SLOTS		6	// options for unpinned region

static inline void GetNumaNodeBitInfo(uval &bitpos, uval &bitnum)
{
    bitnum = 0;
}

