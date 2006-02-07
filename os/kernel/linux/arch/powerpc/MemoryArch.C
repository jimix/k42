/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemoryArch.C,v 1.4 2005/05/24 20:44:52 rosnbrg Exp $
 *****************************************************************************/
#define __KERNEL__
#include "lkKernIncs.H"
#include <mem/PageAllocatorKernPinned.H>
#include <exception/ExceptionLocal.H>
#include <init/kernel.H>
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
#include <lk/LinuxEnv.H>


extern PageHash* pageHash;


extern "C" void * ioremap(unsigned long addr, unsigned long size);
extern "C" void * __ioremap(unsigned long addr,
			    unsigned long size, unsigned long flags);
extern "C" void iounmap(void *addr);
extern "C" void* reserve_phb_iospace(unsigned long size);
extern "C" void* __ioremap_explicit(unsigned long pa, unsigned long ea,
				    unsigned long size, unsigned long flags);

extern "C" void *eeh_ioremap(unsigned long addr, void *vaddr);
void*
__ioremap(unsigned long addr, unsigned long size, unsigned long flags)
{
    void* vaddr = (void*)ioReserveAndMap(addr, size);
    err_printf("IO remap: %016lx[%lx] %p\n", addr, size, vaddr);
    tassert((uval)vaddr, err_printf("Out of I/O mapping range\n"));
    return eeh_ioremap(addr, vaddr);
}

void*
ioremap(unsigned long addr, unsigned long size)
{
    return __ioremap(addr, size, _PAGE_NO_CACHE);
}

void
iounmap(void *addr)
{
    printk("Unmap request: %016lx\n",uval(addr));
}


void*
reserve_phb_iospace(unsigned long size)
{
    void* vaddr = (void*)ioReserve(size);
    err_printf("Reserve IO: [%lx] %p\n", size, vaddr);
    tassert((uval)vaddr, err_printf("Out of I/O mapping range\n"));
    return vaddr;
}

void* __ioremap_explicit(unsigned long pa, unsigned long ea,
			 unsigned long size, unsigned long flags)
{
    void* vaddr = (void*)ioMap(ea, pa, size);
    err_printf("Explicit IO remap: %016lx[%lx] %p\n", pa, size, vaddr);
    tassert((uval)vaddr, err_printf("Out of allocated I/O mapping range\n"));
    return eeh_ioremap(pa, vaddr);
}


extern "C" {
extern struct page* __alloc_page_desc(void* addr, uval numPages);
extern SysStatus __free_page_desc(void* addr, unsigned long numPages);

static uval bootmem;
static uval bootmem_curr;
static uval bootmem_size;
#define BOOTMEM_SIZE	1024*1024
void
StartBootMem(void)
{
    SysStatus rc;
    uval addr;
    uval size = BOOTMEM_SIZE;
    LinuxEnvSuspend();
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(addr, size);
    tassert(_SUCCESS(rc), err_printf("StartBootMem\n"));
    size += ( size% PAGE_SIZE ? PAGE_SIZE: 0);
    __alloc_page_desc((void*)addr, size/PAGE_SIZE);
    LinuxEnvResume();
    bootmem = bootmem_curr = addr;
    bootmem_size = size;
}

void
CleanUpBootMem(void)
{
    //FIXME: now way to free boot mem, because we can't just release
    //the page descriptors for the back end of the region we created
    //above.

#if 0
    // For debugging, for now
    uval freeStart = PAGE_ROUND_UP(bootmem_curr);
    uval freeEnd = bootmem_size + bootmem;
    memset((void*)freeStart, 0xbf, freeEnd - freeStart);
    LinuxEnvSuspend();
    __free_page_desc((void*)bootmem_curr,bootmem_size/PAGE_SIZE);
    LinuxEnvResume();
#endif
}

void *
__alloc_bootmem (unsigned long size,
		 unsigned long align,
		 unsigned long goal)
{
    uval addr = ALIGN_UP(bootmem_curr,align);
    bootmem_curr = addr + size;
    tassertMsg(bootmem_curr <= bootmem_size + bootmem,
	       "Out of bootmem\n");
    memset((void*)addr, 0, size);
    return (void*)addr;
}

char* getBootMem(int size)
{
    uval addr = 0;
    LinuxEnvSuspend();
    DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(addr,
						     PAGE_ROUND_UP(size));
    LinuxEnvResume();
    return (char*)addr;
}

void freeBootMem(char* ptr, int size)
{
    LinuxEnvSuspend();
    DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(PAGE_ROUND_UP((uval)ptr),
						       PAGE_ROUND_DOWN(size));
    LinuxEnvResume();
}

} // extern "C"
