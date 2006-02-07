#ifndef __PAGE_ALLOCATOR_KERN_H_
#define __PAGE_ALLOCATOR_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKern.H,v 1.11 2001/10/05 21:49:59 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This is the base class for the kernel level
 * page allocators
 * **************************************************************************/

#include <alloc/PageAllocatorDefault.H>
class PageAllocatorKern : public PageAllocatorDefault {
    // we don't really need this class anymore
};
#endif /* #ifndef __PAGE_ALLOCATOR_KERN_H_ */
