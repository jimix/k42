#ifndef __MEMORY_MGR_PRIMITIVE_H_
#define __MEMORY_MGR_PRIMITIVE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemoryMgrPrimitive.H,v 1.19 2005/03/15 02:37:53 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Primitive space allocator - used only during processor initialization.
 * **************************************************************************/

class MemoryMgrPrimitive {
protected:
    enum {MAX_SAVED_CHUNKS = 16};

    uval _allocStart;	// virtual address of start of allocation space
    uval _allocEnd;	// virtual address of end of allocation space
    uval _allocNext;	// virtual address of next allocation space

    // array of remembered chunks of memory, chunks that will presumably
    // be made available after initialization is complete
    uval _numSavedChunks;
    struct {uval start; uval end;} _savedChunk[MAX_SAVED_CHUNKS];

    uval minAlign;	// minimum alignment, to prevent false sharing

    void checkAlign(char *name, uval value);
    void recoverSpace(uval start, uval end);

public:
    void init(uval allocstart, uval allocend);
    void init(uval allocstart, uval allocend, uval minalign);
    void rememberChunk(uval start, uval end);
    uval retrieveChunk(uval &start, uval &end);
    void alloc(uval &ptr, uval size, uval align=1, uval offset=0);
    void allocAll(uval &ptr, uval &size, uval align=1, uval offset=0);
    SysStatus allocFromChunks(uval &ptr, uval size, uval align=1, uval offset=0);

    uval allocStart()		 { return _allocStart; }
    uval allocNext()		 { return _allocNext; }
    uval allocEnd()		 { return _allocEnd; }
    uval allocNextPage();

    /*
     * on simulators, it is useful to have all code and data mapped
     * so the simulator debugger can find it
     */
    void touchAllocated();

    /*
     * How much memory, in bytes, is currently "in" this allocator.  
     */
    uval memorySize();
};

#define DEFINE_PRIMITIVE_NEW(CLASS)					\
inline void * operator new(size_t size, MemoryMgrPrimitive *pa)		\
{									\
    uval space;								\
    tassertSilent(size == sizeof(CLASS), BREAKPOINT);				\
    pa->alloc(space, size, sizeof(uval));				\
    return (void *) space;						\
}									\
inline void * operator new[](size_t size, MemoryMgrPrimitive *pa)	\
{									\
    uval space;								\
    pa->alloc(space, size, sizeof(uval));				\
    return (void *) space;						\
}

#endif /* #ifndef __MEMORY_MGR_PRIMITIVE_H_ */
