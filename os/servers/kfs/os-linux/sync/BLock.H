#ifndef __BLOCK_H_
#define __BLOCK_H_

/***************************************************************************
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: BLock.H,v 1.6 2003/08/03 19:11:35 dilma Exp $
 **************************************************************************/
#include <sys/BasicTypes.H>
#include <KFSDebug.H>

extern "C" {
    size_t sizeOfLinuxSemaphore(void);
    void newLinuxSemaphore(void *sem);
    void acquireLinuxSemaphore(void *sem);
    void releaseLinuxSemaphore(void *sem);
    int tryAcquireLinuxSemaphore(void *sem) ;
    int isLockedLinuxSemaphore(void *sem);
}

class BLock {
    void *lock;
public:
    inline void acquire(){
	acquireLinuxSemaphore(lock);
    }
    
    inline void release(){
	releaseLinuxSemaphore(lock);
    }
    
    inline uval tryAcquire() {		// returns 1 if lock gotten, 0 if not
	return tryAcquireLinuxSemaphore(lock);
    }

    inline uval isLocked() {
	return isLockedLinuxSemaphore(lock);
    }
    
    inline void init() {
	newLinuxSemaphore(lock);
    }
    
    BLock() {
	lock = AllocGlobalPadded::alloc(sizeOfLinuxSemaphore());
    }

    ~BLock() { 
	AllocGlobalPadded::free(lock, sizeOfLinuxSemaphore()); 
    }
};

#endif /* #ifndef __BLOCK_H_ */
