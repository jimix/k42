/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Memory.C,v 1.22 2005/05/24 20:44:48 rosnbrg Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include <alloc/PageAllocatorDefault.H>

extern "C" {
#define private __C__private
#define typename __C__typename
#define virtual __C__virtual
#define new __C__new
#define class __C__class
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
#include "Memory.H"
#include "LinuxEnv.H"

extern "C" pgd_t swapper_pg_dir[1];
pgd_t swapper_pg_dir[1];

PageAllocatorRef LinuxPageAlloc;
void
setMemoryAllocator(PageAllocatorRef allocRef)
{
    LinuxPageAlloc = allocRef;
}

static inline SysStatus
allocFromPageAllocator(uval & addr, unsigned int order)
{
    SysStatus rc= DREF(LinuxPageAlloc)->allocPages(addr, PAGE_SIZE*(1<<order));
    if (_SUCCESS(rc)) {
	memset((void*)addr, PAGE_SIZE*(1<<order),0);
    }
    return rc;
}

static inline SysStatus
deallocFromPageAllocator(uval addr, unsigned int order)
{
    return DREF(LinuxPageAlloc)->deallocPages(addr, PAGE_SIZE*(1<<order));
}


//static char ___pageHashHolder[sizeof(PageHash)];
char ___pageHashHolder[sizeof(PageHash)];
//static PageHash* const pageHash= (PageHash*)___pageHashHolder;
PageHash* pageHash = (PageHash*)___pageHashHolder;

void
LinuxMemoryInit(VPNum vp, PageAllocatorRef pa)
{
    if (vp) return;
    LinuxPageAlloc = pa;
    new(pageHash) PageHash;
    extern int mem_init_done;
    mem_init_done = 1;
}

extern "C" {

unsigned long
nr_free_pages()
{
    return 256 << 20;
}

struct page*
alloc_page_desc(void* addr, uval numPages);

struct page*
__k42_virt_to_page(unsigned long addr)
{
    SysStatus rc;
    MemoryHashNode* desc = NULL;
    addr = addr & ~(PAGE_SIZE-1);
    LinuxEnvSuspend();
    rc = pageHash->find_node(addr,desc);
    LinuxEnvResume();
    if (_FAILURE(rc)) {
	err_printf("WARNING: %lx not found in pageHash\n", addr);
	return alloc_page_desc((void*)addr, 1);
    }
#if 0
    // maa sanity check
    tassertMsg(uval(addr) == uval(desc->__C_virtual__), "MemoryHash bug0\n");
    if (uval(desc) != uval(desc->index)) {
	// hard test for arrays
	struct page* top;
	top = (struct page*)(desc->index);
	tassertMsg(uval(top) == uval(top->index),"MemoryHash bug1\n");
	tassertMsg((uval(desc)-uval(top))/(sizeof(struct page)) ==
		   (uval(desc->__C_virtual__)-uval(top->__C_virtual__))
		   /PAGE_SIZE,
		   "MemoryHash bug2\n");
    }
#endif
    return desc;
}


struct page*
__alloc_page_desc(void* addr, uval numPages)
{
    struct MemoryHashNode* desc;
    addr = (void*)((uval)addr & ~(PAGE_SIZE-1));

//    err_printf("APD: %p - %p\n", addr, (char*)addr+(numPages-1)*PAGE_SIZE);
    if (numPages == 1) {
	pageHash->alloc_node(desc);
	memset(desc,0,sizeof(struct page));
	INIT_LIST_HEAD(&desc->lru);
	desc->__C__virtual = (void*) ((unsigned long)addr);
	desc->_count.counter = 1;
	desc->index = (unsigned long)desc;
	pageHash->find_or_add_node(desc);
    } else {
	pageHash->alloc_array_node(numPages, desc);
	memset(desc,0,sizeof(struct page) * numPages);
	for (uval i=0; i<numPages; ++i) {
	    INIT_LIST_HEAD(&desc[i].lru);
	    desc[i].__C__virtual =
		(void*) ((unsigned long)addr + i * PAGE_SIZE);
	    desc[i]._count.counter = 1;
	    desc[i].index = (unsigned long)desc;
	}
	pageHash->add_array_node(desc, numPages);
    }
    return desc;
}

struct page*
alloc_page_desc(void* addr, uval numPages)
{
    LinuxEnvSuspend();
    struct page* p = __alloc_page_desc(addr, numPages);
    LinuxEnvResume();
    return p;
}

SysStatus
__free_page_desc(void* addr, unsigned long numPages)
{
    SysStatus rc;
    page *p;

    addr = (void*) ((uval)addr & ~(PAGE_SIZE-1));
    if (numPages == 1) {
	rc = pageHash->remove_node(uval(addr));
    } else {
	rc = pageHash->remove_array_node(uval(addr), numPages);
    }
///    err_printf("FPD: %p - %p\n", addr, (char*)addr+(numPages-1)*PAGE_SIZE);
    return rc;
}

SysStatus
free_page_desc(void* addr, unsigned long numPages)
{
    SysStatus rc;
    LinuxEnvSuspend();
    rc = __free_page_desc(addr, numPages);
    LinuxEnvResume();
    return rc;
}

struct page *
__alloc_pages(unsigned int gfp_mask, unsigned int order, struct zonelist* ign)
{
    SysStatus rc;
    uval addr;
    struct page * page;

    LinuxEnvSuspend();
    rc = allocFromPageAllocator(addr, order);

    tassertMsg(_SUCCESS(rc), "_alloc_pages: allocation failed\n");
    page = __alloc_page_desc((void *) addr, (1<<order));
    LinuxEnvResume();
    return page;
}

unsigned long
__get_free_pages (unsigned int gfp_mask, unsigned int order)
{
    struct page * page;

    page = __alloc_pages(gfp_mask, order, NULL);
    return (unsigned long) page->__C__virtual;
}


unsigned long get_zeroed_page(unsigned int gfp_mask) {
    unsigned long addr = __get_free_pages(gfp_mask,0);
    memset((void*)addr, 0, PAGE_SIZE);
    return addr;
}


void
__free_pages(struct page* p, unsigned int order)
{
    SysStatus rc;
    void* vaddr;

    LinuxEnvSuspend();
    vaddr = p->__C__virtual;
    if (_SUCCESS(__free_page_desc(vaddr, 1<<order))) {
	rc = deallocFromPageAllocator(uval(vaddr), order);
	tassertMsg(_SUCCESS(rc), "__free_pages: deallocation failed\n");
    }
    LinuxEnvResume();
}

void
__page_cache_release(struct page* p) {
    __free_pages(p,0);
}

void
free_pages(unsigned long addr, unsigned int order)
{
    __free_pages(__k42_virt_to_page(addr), order);
}

int
__copy_tofrom_user(void *to, const void *from, unsigned long size)
{
    memcpy(to, from, size);
    return 0;
}

} // extern "C"

#include <misc/HashNonBlocking.I>
template class HashNonBlockingBase<AllocPinnedGlobalPadded, MemoryHashNode, 2>;
