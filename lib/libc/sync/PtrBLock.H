#ifndef __PTR_BLOCK_H_
#define __PTR_BLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PtrBLock.H,v 1.8 2001/10/05 21:48:21 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Blocked locks on a pointer.
 * N.B. To create a lock holding a pointer to type T,
 *      use PtrBLock<T>, NOT PtrBLock<T*>
 * **************************************************************************/

template <class T>
class PtrBLock {
    BitBLock<PtrBits> ptrbits;
public:
    void acquire(void) {
	ptrbits.acquire();
    }

    void acquire(T* &tp);

    uval tryAcquire(void) {
	return ptrbits.tryAcquire();
    }
    uval tryAcquire(T* &tp) {
	uval rc;
	PtrBits bits;
	rc = ptrbits.tryAcquire(bits);
	tp = (T*)bits.ptr();
	return rc;
    }
    void release(void) {
	ptrbits.release();
    }
    void release(T* tp) {
	PtrBits bits;
	bits.ptr((uval)tp);
	ptrbits.release(bits);
    }
    uval isLocked(void) {
	return ptrbits.isLocked();
    }
    void init(void) {
	ptrbits.init();
    }
    void init(T* tp) {
	PtrBits bits;
	bits.ptr((uval)tp);
	ptrbits.init(bits);
    }
    void get(T* &tp) {
	PtrBits bits;
	ptrbits.get(bits);
	tp = (T*)bits.ptr();
    }
    void set(T* tp) {
	PtrBits bits;
	bits.ptr((uval)tp);
	ptrbits.set(bits);
    }
    PtrBLock<T>(void) { init(); }
    PtrBLock<T>(T* tp) { init(tp); }
};

#define INLINE_LOCK inline

template <class T>
INLINE_LOCK void
PtrBLock<T>::acquire(T* &tp)
{
    PtrBits bits;
    ptrbits.acquire(bits);
    tp = (T*)bits.ptr();
}

#endif /* #ifndef __PTR_BLOCK_H_ */
