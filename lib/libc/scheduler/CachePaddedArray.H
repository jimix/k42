#ifndef __CACHE_PADDED_ARRAY_H_
#define __CACHE_PADDED_ARRAY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CachePaddedArray.H,v 1.4 2002/04/26 11:17:06 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    An array which is padded to cache line boundaries.
 * **************************************************************************/

#include <sys/KernelInfo.H>

template <class X> class CachePaddedArray {
    char *mem;
    size_t elem_size;

public:
    /*
     * init() allocates space for the elements but does not initialize it.
     */
    void init(MemoryMgrPrimitive *memory,
	      int numElements) {
	uval cacheLineSize = KernelInfo::SCacheLineSize();
	elem_size = ALIGN_UP(sizeof(X), cacheLineSize);

	uval memu;
	memory->alloc(memu, numElements * elem_size, cacheLineSize);
	mem = (char *)memu;
    }

    void init(int numElements) {
	uval cacheLineSize = KernelInfo::SCacheLineSize();
	elem_size = ALIGN_UP(sizeof(X), cacheLineSize);

	mem = (char *)AllocGlobal::alloc(numElements * elem_size);

	tassert(((unsigned long)mem & (cacheLineSize-1)) == 0,
		err_printf("Allocated memory not cache line aligned.\n"));

	memset((void *)mem, 0, numElements * elem_size);
    }

    X& operator[](int index) {
	return *(X *)(mem + elem_size * index);
    }
};

#endif /* #ifndef __CACHE_PADDED_ARRAY_H_ */
