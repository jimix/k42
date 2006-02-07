#ifndef __RW_BLOCK_H_
#define __RW_BLOCK_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: RWLock.H,v 1.2 2003/10/02 18:17:56 dilma Exp $
 **************************************************************************/
/*****************************************************************************
 * Module Description: Reader-Writer locks.
 * **************************************************************************/

#include <sys/BasicTypes.H>
#include <KFSDebug.H>

extern "C" {
    size_t sizeOfLinuxRWSemaphore(void);
    void newLinuxRWSemaphore(void *sem);
    void acquireReadLinuxSemaphore(void *sem);
    void acquireWriteLinuxSemaphore(void *sem);
    void releaseReadLinuxSemaphore(void *sem);
    void releaseWriteLinuxSemaphore(void *sem);
}

class RWBLock {
    void *rwlock;
public:
    void init() {
	newLinuxRWSemaphore(rwlock);
    }

    void acquireR() {
	acquireReadLinuxSemaphore(rwlock);
    }

    void releaseR() {
	releaseReadLinuxSemaphore(rwlock);
    }

    void acquireW() {
	acquireWriteLinuxSemaphore(rwlock);
    }

    void releaseW() {
	releaseWriteLinuxSemaphore(rwlock);
    }

    // This makes it easy to use these locks with AutoLock
    inline void acquire() { acquireW(); };
    inline void release() { releaseW(); };

    RWBLock() {
	rwlock = AllocGlobalPadded::alloc(sizeOfLinuxRWSemaphore());
    }

    ~RWBLock() { 
	AllocGlobalPadded::free(rwlock, sizeOfLinuxRWSemaphore()); 
    }
};

#endif /* #ifndef __RW_BLOCK_H_ */
