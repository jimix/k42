#ifndef __CACHE_SYNC_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __CACHE_SYNC_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CacheSync.H,v 1.9 2002/04/15 18:23:20 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: deal with icache syncronization for power
 * **************************************************************************/

#include <misc/hardware.H>
#include "mem/PageAllocatorKernPinned.H"
#include <sys/KernelInfo.H>

template <uval CLL>
static __inline void
DCacheStorePage(uval vaddr) 
{
    uval i;
    for(i=0;i<PAGE_SIZE;i+=CLL) {
	DCacheStoreLine(vaddr, i);
    }
    __asm(" sync");
} 

template <uval CLL>
static __inline void
ICacheInvalidatePage(uval vaddr)
{
    uval i;
    for(i=0;i<PAGE_SIZE;i+=CLL) {
	ICacheInvalidateLine(vaddr, i);
    }
    __asm(" isync");
}


// This is inlined.  For architectures which are hardware
// synchonized, use a complete empty procedure -
// DO NOT even set pg->cacheSynced.
static __inline__ void
CacheSync(PageDesc* pg)
{
    uval vaddr;
    vaddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
    if (KernelInfo::PCacheLineSize() == 128) {
	DCacheStorePage<128>(vaddr);
	ICacheInvalidatePage<128>(vaddr);
    } else {
	passert(0, err_printf("need case to handle cache line size of %d\n", 
			      KernelInfo::PCacheLineSize()));
    }
    pg->cacheSynced = PageDesc::SET;
}
