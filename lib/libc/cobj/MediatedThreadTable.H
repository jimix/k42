#ifndef __MEDIATED_THREAD_TABLE_H_
#define __MEDIATED_THREAD_TABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MediatedThreadTable.H,v 1.12 2002/10/10 13:08:12 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definition of the mediator support classes
 * (hash table, condition object)
 * **************************************************************************/

#include <sync/Lock.H>
#include <scheduler/Scheduler.H>

//FIXME get rid of this
extern sval err_tprintf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

// FIXME: Allow dynamic sizing of the table depending on number of processors
#define TABLE_SIZE	32

struct BlockedThread {
    DEFINE_GLOBAL_NEW(BlockedThread);
    BlockedThread(ThreadID *ptid) : blockedTID(ptid) { /* empty body */ }
    BlockedThread *next;
    ThreadID *blockedTID;
    void unblockThread()
    {
	ThreadID tid = *blockedTID;
	err_tprintf("BlockedThread::unblockThread(%lx)\n", tid);
	*blockedTID = Scheduler::NullThreadID;
	Scheduler::Unblock(tid);
    }
};

class MediatedThreadTable {
public:
    DEFINE_GLOBAL_NEW(MediatedThreadTable);
    MediatedThreadTable(uval size = TABLE_SIZE);
    ~MediatedThreadTable();

    SysStatus pushData(ThreadID tidKey, uval ra, uval nvreg);
    SysStatus popData(ThreadID tidKey, uval &ra, uval &nvreg);
    uval queryThreadExists(ThreadID tidKey);

    SysStatus addBlockedThread(ThreadID *ptid);
    SysStatus removeBlockedThread(ThreadID tid, uval doUnblock = 0);
    SysStatus unblockThreads();
protected:
    struct ThreadItem {
	DEFINE_GLOBAL_NEW(ThreadItem);
	ThreadItem(ThreadID tid, uval ra, uval nvreg)
	    : tidKey(tid), retAddr(ra), nonVolatileReg(nvreg) {
	    /* empty body */
	}
	ThreadItem *next;
	ThreadID tidKey;
	uval retAddr;
	uval nonVolatileReg;
    };

    // Each bucket holds 2 lists, 1 for data and 1 for blocked threads
    struct Bucket {
	DEFINE_GLOBAL_NEW(Bucket);
	Bucket() : itemBucket(0), btBucket(0) {
	    /* empty body */
	}
	ThreadItem *itemBucket;
	BLock itemBucketLock;
	BlockedThread *btBucket;
	BLock btBucketLock;
    };

    uval tableSize;
    Bucket *buckets;
    uval hashFunc(ThreadID tidKey);
    ThreadItem *locked_findItem(ThreadID tidKey, ThreadItem * &prev, uval idx);
};

class ConditionObject {
public:
    ConditionObject() : flag(0), threadList(0) {
	/* empty body */
    }
    DEFINE_NOOP_NEW(CondtionObject);
    ~ConditionObject() {
	tassert(threadList == 0,
		err_printf("There are blocked threads still?\n"));
    }

    uval registerForCondition(ThreadID &curThread);
    void waitForCondition(ThreadID &curThread);
    void conditionSet();

protected:
    uval flag;
    BlockedThread *threadList;
    BLock listLock;

    void addBlockedThread(ThreadID *ptid);
};

#endif /* #ifndef __MEDIATED_THREAD_TABLE_H_ */
