#ifndef __PTR_SLOCK_H_
#define __PTR_SLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PtrSLock.H,v 1.5 2001/10/05 21:48:21 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: spin locks on a pointer.
 * **************************************************************************/

template <class T>
class PtrSLock {
    BitSLock<PtrBits> ptrbits;
public:
    void acquire(void) {
	ptrbits.acquire();
    }
    void acquire(T* &tp) {
	PtrBits bits;
	ptrbits.acquire(bits);
	tp = (T*)bits.ptr();
    }
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
    PtrSLock<T>(void) { init(); }
    PtrSLock<T>(T* tp) { init(tp); }
};
#endif /* #ifndef __PTR_SLOCK_H_ */
