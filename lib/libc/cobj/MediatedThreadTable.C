/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MediatedThreadTable.C,v 1.9 2002/10/10 13:08:12 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of the mediator support classes
 * (hash table, condition object)
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "MediatedThreadTable.H"
#include <alloc/alloc.H>

MediatedThreadTable::MediatedThreadTable(uval size)
    : tableSize(size), buckets(new Bucket[size])
{
}

MediatedThreadTable::~MediatedThreadTable()
{
#ifndef NDEBUG
    for (uval i = 0; i < tableSize; ++i) {
	tassert(!buckets[i].itemBucket, err_tprintf("ibuckets not clean!\n"));
	tassert(!buckets[i].btBucket, err_tprintf("bbuckets not clean!\n"));
    }
#endif /* #ifndef NDEBUG */
    delete [] buckets;
}

SysStatus
MediatedThreadTable::pushData(ThreadID tidKey, uval ra, uval nvreg)
{
    uval idx = hashFunc(tidKey);
    err_tprintf("tidKey = %lx => idx = %ld\n", tidKey, idx);
    ThreadItem *item = new ThreadItem(tidKey, ra, nvreg);

    AutoLock<BLock> al(&buckets[idx].itemBucketLock);

    item->next = buckets[idx].itemBucket;
    buckets[idx].itemBucket = item;
    return 0;
}

SysStatus
MediatedThreadTable::popData(ThreadID tidKey, uval &ra, uval &nvreg)
{
    ThreadItem *prev = 0;
    uval idx = hashFunc(tidKey);
    AutoLock<BLock> al(&buckets[idx].itemBucketLock);

    ThreadItem *item = locked_findItem(tidKey, prev, idx);
    tassert(item, err_tprintf("Doh! Couldn't find item!\n"));
    if (!item) return -1;

    if (prev) {
	prev->next = item->next;
    } else {
	buckets[idx].itemBucket = item->next;
    }

    ra = item->retAddr;
    nvreg = item->nonVolatileReg;
    delete item;

    return 0;
}

uval
MediatedThreadTable::queryThreadExists(ThreadID tidKey)
{
    ThreadItem *prev;
    uval idx = hashFunc(tidKey);
    AutoLock<BLock> al(&buckets[idx].itemBucketLock);
    ThreadItem *item = locked_findItem(tidKey, prev, idx);
    return (!!item);
}

inline MediatedThreadTable::ThreadItem *
MediatedThreadTable::locked_findItem(ThreadID tidKey, ThreadItem * &prev,
				       uval idx)
{
    ThreadItem *item;
    for (item = buckets[idx].itemBucket; item; prev = item, item = item->next) {
	if (item->tidKey == tidKey) break;
    }

    return item;
}

SysStatus
MediatedThreadTable::addBlockedThread(ThreadID *ptid)
{
    uval idx = hashFunc(*ptid);
    BlockedThread *bt = new BlockedThread(ptid);

    AutoLock<BLock> al(&buckets[idx].btBucketLock);

    bt->next = buckets[idx].btBucket;
    buckets[idx].btBucket = bt;
    return 0;
}

SysStatus
MediatedThreadTable::removeBlockedThread(ThreadID tid, uval doUnblock)
{
    BlockedThread *bt, *prev = 0;
    uval idx = hashFunc(tid);
    AutoLock<BLock> al(&buckets[idx].btBucketLock);

    for (bt = buckets[idx].btBucket; bt; prev = bt, bt = bt->next) {
	if (*bt->blockedTID == tid) break;
    }

    tassert(bt, err_tprintf("Doh! Couldn't find blocked thread!\n"));
    if (!bt) return -1;

    if (prev) {
	prev->next = bt->next;
    } else {
	buckets[idx].btBucket = bt->next;
    }

    if (doUnblock) {
	bt->unblockThread();
    }

    delete bt;

    return 0;
}

SysStatus
MediatedThreadTable::unblockThreads()
{
    err_tprintf("MTT:unblockThreads()\n");
    for (uval idx = 0; idx < tableSize; ++idx) {
	buckets[idx].btBucketLock.acquire();
	BlockedThread *bt, *next;

	for (bt = buckets[idx].btBucket; bt; bt = next) {
	    bt->unblockThread();
	    next = bt->next;
	    delete bt;
	}
	buckets[idx].btBucket = 0;
	buckets[idx].btBucketLock.release();
    }

    return 0;
}

inline uval
MediatedThreadTable::hashFunc(ThreadID tidKey)
{
    uval retvalue;
#if 1
    retvalue = ((SysTypes::VP_FROM_COMMID(tidKey) << 3) ^
	    SysTypes::PID_FROM_COMMID(tidKey)) % tableSize;
#else /* #if 1 */
    retvalue = SysTypes::VP_FROM_COMMID(tidKey) ^
	   SysTypes::PID_FROM_COMMID(tidKey);
#endif /* #if 1 */
    return(retvalue);
}

void
ConditionObject::addBlockedThread(ThreadID *ptid)
{
    BlockedThread *bt = new BlockedThread(ptid);

    bt->next = threadList;
    threadList = bt;
}

// returns 1 if condition is set already
uval
ConditionObject::registerForCondition(ThreadID &curThread)
{
    AutoLock<BLock> al(listLock);

    if (flag == 1) return 1;

    curThread = Scheduler::GetCurThread();
    addBlockedThread(&curThread);
    return 0;
}

void
ConditionObject::waitForCondition(ThreadID &curThread)
{
    do {
	err_tprintf("ConditionObject::wait: WAITING\n");
	Scheduler::Block();
	err_tprintf("ConditionObject::wait: (%lx) CONTINUES\n", curThread);
    } while (curThread != Scheduler::NullThreadID);
}

void
ConditionObject::conditionSet()
{
    AutoLock<BLock> al(listLock);

    flag = 1;
    BlockedThread *next;
    for (BlockedThread *bt = threadList; bt; bt = next) {
	bt->unblockThread();
	next = bt->next;
	delete bt;
    }
}
