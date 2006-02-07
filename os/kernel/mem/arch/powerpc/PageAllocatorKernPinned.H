#ifndef	PAGE_ALLOCATOR_KERN_PINNED_DEFH
<<<< include machine independent file - not this machine dependent file >>>>
#endif
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernPinned.H,v 1.9 2003/05/06 19:38:20 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine-dependent component of page frame array
 * **************************************************************************/

// maximum physical memory size, as a real address
// Note this is the architectural limit, not the actual memory in use

#define MAX_PHYS_MEM_SIZE  (uval(1) << 36)
