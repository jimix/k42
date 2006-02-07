#ifndef __BARRIER_H_
#define __BARRIER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Barrier.H,v 1.9 2001/10/16 05:31:58 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Barrier
 * **************************************************************************/

/******************************************************************************
 *
 * Barrier locks, that queue all processes until all reach same point
 *
 *      this is the basic data structure that client must allocate
 *      before calling Barrier
 *
 *****************************************************************************/

#include <sync/BLock.H>
#include <sync/SLock.H>

class Barrier {
public:
    virtual void enter() = 0;
    virtual void reinit(uval numWaiters) = 0;
    DEFINE_NOOP_NEW(Barrier);
    virtual ~Barrier() { /* empty body */ }
};

class SpinBarrier : public Barrier {
    BLock lock;
    uval  upcount;
    uval  downcount;
    uval  total;

public:
    DEFINE_GLOBAL_NEW(SpinBarrier);
    SpinBarrier(uval numWaiters);
    virtual void enter();
    virtual void reinit(uval numWaiters); // assume not in use
    virtual ~SpinBarrier();
};

/* eventually should be changed to a tree based barrier */
class SpinOnlyBarrier : public Barrier {
    SLock lock;
    uval  upcount;
    uval  downcount;
    uval  total;

public:
    DEFINE_GLOBAL_NEW(SpinOnlyBarrier);
    SpinOnlyBarrier(uval numWaiters);
    virtual void enter();
    virtual void reinit(uval numWaiters); // assume not in use
    virtual ~SpinOnlyBarrier();
};

class BlockBarrier : public Barrier {
    class BarrierList;
    BLock        lock;
    uval         upcount;
    uval         downcount;
    uval         total;
    BarrierList *list;
public:
    DEFINE_GLOBAL_NEW(BlockBarrier);
    BlockBarrier(uval numWaiters);
    virtual void enter();
    virtual void reinit(uval numWaiters); // assume not in use
    virtual ~BlockBarrier();
};

#endif /* #ifndef __BARRIER_H_ */
