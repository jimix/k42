#ifndef __ALLOC_H_
#define __ALLOC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: alloc.H,v 1.30 2003/03/14 23:10:46 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This file provides clients to the Alloc
 * facility with the basic interface operations.
 * **************************************************************************/

#include <defines/mem_debug.H>
#include <alloc/AllocPool.H>

#ifdef DEBUG_CORRUPTMEM
#define MEMSET(RESULT,SIZE) memset(RESULT,0xBF,SIZE)
#else /* #ifdef DEBUG_CORRUPTMEM */
#define MEMSET(RESULT,SIZE)
#endif /* #ifdef DEBUG_CORRUPTMEM */

#ifdef DEBUG_LEAK
#include <sys/LeakProof.H>
extern LeakProof* allocLeakProof;
#define LEAKPROOF_ALLOC(RESULT,SIZE) allocLeakProof->alloc2((uval) RESULT,SIZE)
#define LEAKPROOF_FREE(PTR,SIZE) allocLeakProof->free((uval)PTR,SIZE)
#else /* #ifdef DEBUG_LEAK */
#define LEAKPROOF_ALLOC(RESULT,SIZE)
#define LEAKPROOF_FREE(PTR,SIZE)
#endif /* #ifdef DEBUG_LEAK */

#ifndef NDEBUG
void Alloc_AssertProperCallingContext();
#else
#define Alloc_AssertProperCallingContext()
//static inline void Alloc_AssertProperCallingContext(){};
#endif

inline void *
allocLocalStrict(uval size)
{
    void * result;
    result = allocLocal[AllocPool::DEFAULT].allocLocalStrict(size);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void *
allocLocalStrict(uval size, uval &realSize)
{
    void * result;
    result = allocLocal[AllocPool::DEFAULT].allocLocalStrict(size, realSize);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void
freeLocalStrict(void *ptr, uval size)
{
    LEAKPROOF_FREE(ptr, size);
    Alloc_AssertProperCallingContext();
    allocLocal[AllocPool::DEFAULT].freeLocalStrict(ptr, size);
}


inline void *
allocGlobal(uval size)
{
    void * result;
    result = allocLocal[AllocPool::DEFAULT].allocGlobal(size);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void *
allocGlobal(uval size, uval &realSize)
{
    void * result;
    result = allocLocal[AllocPool::DEFAULT].allocGlobal(size, realSize);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void
freeGlobal(void *ptr, uval size)
{
    LEAKPROOF_FREE(ptr, size);
    Alloc_AssertProperCallingContext();
    allocLocal[AllocPool::DEFAULT].freeGlobal(ptr, size);
}

inline void *
allocGlobalPadded(uval size)
{
    void * result;
    result = allocLocal[AllocPool::DEFAULT].allocGlobalPadded(size);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void *
allocGlobalPadded(uval size, uval &realSize)
{
    void * result;
    result = allocLocal[AllocPool::DEFAULT].allocGlobalPadded(size, realSize);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void
freeGlobalPadded(void *ptr, uval size)
{
    LEAKPROOF_FREE(ptr, size);
    Alloc_AssertProperCallingContext();
    allocLocal[AllocPool::DEFAULT].freeGlobalPadded(ptr, size);
}

inline void *
allocPinnedLocalStrict(uval size)
{
    void * result;
    result = allocLocal[AllocPool::PINNED].allocLocalStrict(size);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void *
allocPinnedLocalStrict(uval size, uval &realSize)
{
    void * result;
    result = allocLocal[AllocPool::PINNED].allocLocalStrict(size, realSize);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void
freePinnedLocalStrict(void *ptr, uval size)
{
    LEAKPROOF_FREE(ptr, size);
    Alloc_AssertProperCallingContext();
    allocLocal[AllocPool::PINNED].freeLocalStrict(ptr, size);
}


inline void *
allocPinnedGlobal(uval size)
{
    void * result;
    result = allocLocal[AllocPool::PINNED].allocGlobal(size);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void *
allocPinnedGlobal(uval size, uval &realSize)
{
    void * result;
    result = allocLocal[AllocPool::PINNED].allocGlobal(size, realSize);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void
freePinnedGlobal(void *ptr, uval size)
{
    LEAKPROOF_FREE(ptr, size);
    Alloc_AssertProperCallingContext();
    allocLocal[AllocPool::PINNED].freeGlobal(ptr, size);
}


inline void *
allocPinnedGlobalPadded(uval size)
{
    void * result;
    result = allocLocal[AllocPool::PINNED].allocGlobalPadded(size);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void *
allocPinnedGlobalPadded(uval size, uval &realSize)
{
    void * result;
    result = allocLocal[AllocPool::PINNED].allocGlobalPadded(size, realSize);
    MEMSET(result, size);
    LEAKPROOF_ALLOC(result, size);
    Alloc_AssertProperCallingContext();
    return result;
}
inline void
freePinnedGlobalPadded(void *ptr, uval size)
{
    LEAKPROOF_FREE(ptr, size);
    Alloc_AssertProperCallingContext();
    allocLocal[AllocPool::PINNED].freeGlobalPadded(ptr, size);
}


// classes for allocation that can be used to paramterized template class
class AllocLocalStrict {
public:
    static void *alloc(uval size)
	{ return allocLocalStrict(size); }
    static void *alloc(uval size, uval &rsize)
	{ return allocLocalStrict(size,rsize);}
    static void  free(void *ptr, uval size)
	{ freeLocalStrict(ptr, size); }
};
class AllocGlobal {
public:
    static void *alloc(uval size)
	{ return allocGlobal(size); }
    static void *alloc(uval size, uval &rsize)
	{ return allocGlobal(size,rsize);}
    static void  free(void *ptr, uval size)
	{ freeGlobal(ptr, size); }
};
class AllocGlobalPadded {
public:
    static void *alloc(uval size)
	{ return allocGlobalPadded(size); }
    static void *alloc(uval size, uval &rsize)
	{return allocGlobalPadded(size,rsize);}
    static void  free(void *ptr, uval size)
	{ freeGlobalPadded(ptr, size); }
};
class AllocPinnedLocalStrict {
public:
    static void *alloc(uval size)
	{ return allocPinnedLocalStrict(size); }
    static void *alloc(uval size, uval &rsize)
	{return allocPinnedLocalStrict(size,rsize);}
    static void  free(void *ptr, uval size)
	{ freePinnedLocalStrict(ptr, size); }
};
class AllocPinnedGlobal {
public:
    static void *alloc(uval size)
	{ return allocPinnedGlobal(size); }
    static void *alloc(uval size, uval &rsize)
	{return allocPinnedGlobal(size,rsize);}
    static void  free(void *ptr, uval size)
	{ freePinnedGlobal(ptr, size); }
};
class AllocPinnedGlobalPadded {
public:
    static void *alloc(uval size)
	{ return allocPinnedGlobalPadded(size); }
    static void *alloc(uval size, uval &rsize)
	{return allocPinnedGlobalPadded(size,rsize);}
    static void  free(void *ptr, uval size)
	{ freePinnedGlobalPadded(ptr, size); }
};
class AllocChoose {
public:
    enum {
	LOCALSTRICT,
	GLOBAL,
	GLOBALPADDED,
	PINNEDLOCALSTRICT,
	PINNEDGLOBAL,
	PINNEDGLOBALPADDED,
    };
    static void *alloc(uval size, uval8 type)
	{
	    switch (type) {
	    case LOCALSTRICT:		return allocLocalStrict(size);
	    case GLOBAL:		return allocGlobal(size);
	    case GLOBALPADDED:		return allocGlobalPadded(size);
	    case PINNEDLOCALSTRICT:	return allocPinnedLocalStrict(size);
	    case PINNEDGLOBAL:		return allocPinnedGlobal(size);
	    case PINNEDGLOBALPADDED:	return allocPinnedGlobalPadded(size);
	    }
	    return 0;
	}
    static void *alloc(uval sz, uval &rsz, uval8 type)
	{
	    switch (type) {
	    case LOCALSTRICT:		return allocLocalStrict(sz,rsz);
	    case GLOBAL:		return allocGlobal(sz,rsz);
	    case GLOBALPADDED:		return allocGlobalPadded(sz,rsz);
	    case PINNEDLOCALSTRICT:	return allocPinnedLocalStrict(sz,rsz);
	    case PINNEDGLOBAL:		return allocPinnedGlobal(sz,rsz);
	    case PINNEDGLOBALPADDED:	return allocPinnedGlobalPadded(sz,rsz);
	    }
	    return 0;
	}
    static void free(void *p, uval sz, uval8 type)
	{
	    switch (type) {
	    case LOCALSTRICT:		freeLocalStrict(p,sz);		return;
	    case GLOBAL:		freeGlobal(p,sz);		return;
	    case GLOBALPADDED:		freeGlobalPadded(p,sz);		return;
	    case PINNEDLOCALSTRICT:	freePinnedLocalStrict(p,sz);	return;
	    case PINNEDGLOBAL:		freePinnedGlobal(p,sz);		return;
	    case PINNEDGLOBALPADDED:	freePinnedGlobalPadded(p,sz);	return;
	    }
	}
};

extern void alloc_init();

#endif /* #ifndef __ALLOC_H_ */
