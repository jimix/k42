#ifndef __MEMORY_MGR_PRIMITIVE_REAL_MODE_H_
#define __MEMORY_MGR_PRIMITIVE_REAL_MODE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemoryMgrPrimitiveRealMode.H,v 1.1 2005/07/18 21:49:17 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Primitive space allocator - used for memory accessible in real-mode,
 *         on machines for which not all memory is real-mode accessible.
 * **************************************************************************/

#include <alloc/MemoryMgrPrimitive.H>

class MemoryMgrPrimitiveRealMode : public MemoryMgrPrimitive {
private:
    static const uval MAX_NUM_SIZES = 4;
    FairBLock lock;
    uval numSizes;
    struct {
	uval size;
	uval align;
	uval head;
    } freeList[MAX_NUM_SIZES];

public:
    void init(uval allocstart, uval allocend)
    {
	tassertMsg(0, "Not ready for prime time.\n");
	// FIXME: alloc() must be made virtual before this can work.
	MemoryMgrPrimitive::init(allocstart, allocend, sizeof(uval));
	numSizes = 0;
	lock.init();
    }

    void addSize(uval size, uval align) {
	tassertMsg(numSizes < MAX_NUM_SIZES, "Too many sizes.\n");
	// Keep set sorted, smallest to largest.
	sval i;
	for (i = numSizes-1; i >= 0; i--) {
	    if (size >= freeList[i].size) {
		tassertMsg(size != freeList[i].size, "Duplicate size.\n");
		break;
	    }
	    freeList[i+1] = freeList[i];
	}
	freeList[i+1].size = size;
	freeList[i+1].align = align;
	freeList[i+1].head = 0;
	numSizes++;
    }

    void alloc(uval &ptr, uval size, uval align, uval offset=0) {
	tassertMsg(offset == 0,
		   "Real-mode allocator can't handle non-zero offsets.\n");

	AutoLock<FairBLock> al(&lock);

	for (uval i = 0; i < numSizes; i++) {
	    if ((freeList[i].size == size) && (freeList[i].align >= align)) {
		ptr = freeList[i].head;
		if (ptr != 0) {
		    freeList[i].head = *((uval *) ptr);
		    return;
		}
		break;
	    }
	}

	align = align < minAlign ? minAlign : align;
	ptr = (_allocNext + (align - 1)) & ~(align - 1);
	size = ALIGN_UP(size, minAlign);
	if (ptr + size > _allocEnd) {
	    ptr = 0;
	    return;
	}

	/*
	 * Add anything we're losing to alignment to the free lists, searching
	 * the larger sizes first.
	 */
	for (sval i = numSizes-1; i >= 0; i--) {
	    for (;;) {
		uval next = (_allocNext + (freeList[i].align - 1)) &
						~(freeList[i].align - 1);
		if ((ptr - next) < freeList[i].size) break;
		*((uval *) next) = freeList[i].head;
		freeList[i].head = next;
		_allocNext = next + freeList[i].size;
	    }
	}

	_allocNext = ptr + size;
    }

    void dealloc(uval ptr, uval size) {
	AutoLock<FairBLock> al(&lock);
	for (uval i = 0; i < numSizes; i++) {
	    if (freeList[i].size == size) {
		tassertMsg((ptr & (freeList[i].align - 1)) == 0,
			   "Deallocated block not adequately aligned.\n");
		*((uval *) ptr) = freeList[i].head;
		freeList[i].head = ptr;
		return;
	    }
	}
	tassertMsg(0, "No free list for size 0x%lx.\n", size);
    }
};

#endif /* #ifndef __MEMORY_MGR_PRIMITIVE_REAL_MODE_H_ */
