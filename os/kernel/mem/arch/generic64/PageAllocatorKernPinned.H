#ifndef __GENERIC64_PAGE_ALLOCATOR_KERN_PINNED_H_
#define __GENERIC64_PAGE_ALLOCATOR_KERN_PINNED_H_
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernPinned.H,v 1.2 2001/10/05 21:50:09 peterson Exp $
 *************************************************************************** */

#define MAX_PHYS_MEM_SIZE  (uval(1) << 36)

static inline void GetNumaNodeBitInfo(uval &bitpos, uval &bitnum)
{
    bitnum = 0;
}
#endif /* #ifndef ... */
