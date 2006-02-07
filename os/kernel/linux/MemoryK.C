/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemoryK.C,v 1.3 2005/05/24 20:44:50 rosnbrg Exp $
 *****************************************************************************/
#define __KERNEL__
#include "lkKernIncs.H"
#include <mem/PageAllocatorKernPinned.H>
#include <exception/ExceptionLocal.H>
extern "C" {
#define private __C__private
#define typename __C__typename
#define new __C__new
#define class __C__class
#define virtual __C__virtual
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/slab.h>
#undef private
#undef typename
#undef new
#undef class
#undef virtual
}
#include <lk/Memory.H>


//static char ___pageHashHolder[sizeof(PageHash)];
//static PageHash* const pageHash= (PageHash*)___pageHashHolder;
extern PageHash* pageHash;


extern "C" {

unsigned long
__k42_pa(unsigned long v)
{
    return (unsigned long) PageAllocatorKernPinned::virtToReal(v);
}

unsigned long
__k42_va(unsigned long p)
{
    return (unsigned long) PageAllocatorKernPinned::realToVirt(p);
}

extern void * kmalloc (size_t size, int flags);


} // extern "C"
