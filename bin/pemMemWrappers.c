/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pemMemWrappers.c,v 1.8 2004/11/23 04:17:15 cascaval Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: wrappers for malloc and free functions with pem events
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <trace/traceK42.h>
#include <trace/traceLibC.h>

/* ------------------------ frame definition --------------- */

typedef struct _PwrPCFrame {
    struct _PwrPCFrame*	backchain;
    uval	cr;
    uval	lr;
    uval	compilerReserved;
    uval	linkerReserved;
    uval	toc;
    uval	param[8];
} PwrPCFrame;

#define MAP_LARGEPAGE 0x1000000

static void    initMemWrappers(void);
static void *  (*_libc_malloc)(size_t);
static void    (*_libc_free)(void *);
static __ptr_t (*_libc_mmap)(__ptr_t addr, size_t len, int prot, 
			     int flags, int fd, __off_t offset);
static int     (*_libc_munmap)(__ptr_t addr, size_t len);

/* ---------------------------- wrappers ------------------------ */
/**
 * malloc wrapper
 */
void *malloc(size_t size) {
    PwrPCFrame* frame;
    void *ptr;
    uval64 lr = 0; 

    asm volatile("mr %0,1" : "=r" (frame));
    /*
     * Walk back one frame to look up the return address in the caller
     */
    if (frame != NULL) 
      frame = frame->backchain;
    if (frame != NULL)
      lr = (uval64) frame->lr;

    if(!_libc_malloc) initMemWrappers();

    ptr = (*_libc_malloc)(size);
#if DEBUG
    printf("malloc wrapper: %p %ld (call 0x%ld) \n", ptr, size, lr);
#else
    TraceLIBLibCMalloc ((uval64)ptr, size, lr);
#endif
 
    return ptr;
}

/**
 * free wrapper
 */
void free(void *ptr) {
    if(!_libc_free) initMemWrappers();

#if DEBUG
    printf("free wrapper: %p\n", ptr);
#else
    TraceLIBLibCFree ((uval64)ptr);
#endif
    (*_libc_free)(ptr);
}

/**
 * mmap wrapper
 */
__ptr_t mmap(__ptr_t addr, size_t len, int prot, int flags, int fd, 
	     __off_t offset) 
{
    PwrPCFrame* frame;
    __ptr_t ptr;
    char *k42_large_page;
    uval64 lr = 0; 

    asm volatile("mr %0,1" : "=r" (frame));
    /*
     * Walk back one frame to look up the return address in the caller
     */
    if (frame != NULL) { 
      frame = frame->backchain;
    }
    if (frame != NULL) {
      lr = (uval64) frame->lr;
    }
    
    if(!_libc_mmap) initMemWrappers();
    
    k42_large_page = getenv("K42_MMAP_LARGE_PAGE");
    if (k42_large_page != NULL) {
        ptr = (*_libc_mmap)(addr, len, prot, (flags | MAP_LARGEPAGE), fd, offset);
    } else {
        ptr = (*_libc_mmap)(addr, len, prot, flags, fd, offset);
    }

#if DEBUG
    printf("mmap wrapper: %p %ld (call 0x%ld)\n", ptr, len, lr);
#else
    TraceLIBLibCMmap ((uval64)ptr, len, lr, offset, prot, flags, fd);
#endif
    return ptr;
}

/**
 * munmap wrapper
 */
int mumnap(__ptr_t addr, size_t len) {

    if(!_libc_munmap) initMemWrappers();

#if DEBUG
    printf("munmap wrapper: %p %ld\n", addr, len);
#else
    TraceLIBLibCMunmap ((uval64)addr, len);
#endif
    return (*_libc_munmap)(addr, len);
}


/* ---------------------- dynamic library stuff --------------------- */

static void initMemWrappers(void)
{
    char *error;
    static int memWrappersInitialized = 0;
    
    if(memWrappersInitialized) {
      fprintf(stderr, "initMemWrappers: initialization already failed\n");
      exit(1);
    }
    dlerror();
    
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT      ((void *) 0)
#endif

#ifndef RTLD_NEXT
#define RTLD_NEXT      ((void *) -1LL)
#endif

    _libc_malloc = dlsym(RTLD_NEXT, "malloc");
    if ((error = dlerror()) != NULL || !_libc_malloc) {
        perror("_libc_malloc");
	exit(1);
    }
    _libc_free = dlsym(RTLD_NEXT, "free");
    if ((error = dlerror()) != NULL || !_libc_free) {
        perror("_libc_free");
	exit(1);
    }
    _libc_mmap = dlsym(RTLD_NEXT, "mmap");
    if ((error = dlerror()) != NULL || !_libc_mmap) {
        perror("_libc_mmap");
	exit(1);
    }
    _libc_munmap = dlsym(RTLD_NEXT, "munmap");
    if ((error = dlerror()) != NULL || !_libc_munmap) {
        perror("_libc_munmap");
	exit(1);
    }
    
    memWrappersInitialized = 1;
}


