#ifndef __FAIR_BLOCK_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __FAIR_BLOCK_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FairBLock.H,v 1.3 2001/10/05 21:48:24 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine specific component of FairBLock.H
 * **************************************************************************/

// from x86 seems all right

inline void
FairBLock::acquire()
{
    if (!CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock))) {
	_acquire();
    }
}

inline void
FairBLock::release()
{
    if(!CompareAndStoreSynced((uval*)(&lock.tail), (uval)(&lock), 0)) {
	_release();
    }
}

inline uval
FairBLock::tryAcquire(void)
{
    return CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock));
}

