/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockedThreadQueues.C,v 1.18 2004/07/11 21:59:25 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains queues of threads blocked on
 * specified addresses.
 * **************************************************************************/
/* Implementation Notes: In the future, will have a hash table per-processor,
 * use some part of the virtual address (same as the memory allocator) to
 * specify the hash table that will handle the request.  Hence will
 * automatically distribute the requests, and queue heads will be local
 * to where the lock was allocated from, which is as good a guess as any.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sync/BlockedThreadQueues.H>
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>

/* virtual */ SysStatus
BlockedThreadQueues::addCurThreadToQueue(Element *qe, void * on)
{
    uval index = hash(uval(on));
    table[index].lock.acquire();
    qe->threadID = Scheduler::GetCurThread();
    qe->next = 0;
    qe->blockedOn = on;
    if (!table[index].tail) {
	table[index].head = table[index].tail = qe;
    } else {
	table[index].tail->next = qe;
	table[index].tail = qe;
    }
    table[index].lock.release();
    return 0;
}

/* virtual */ SysStatus
BlockedThreadQueues::removeCurThreadFromQueue(Element *qe, void */*on*/)
{
    uval index = hash(uval(qe->blockedOn));
    table[index].lock.acquire();
    qe->blockedOn = 0;
    tassert(table[index].head, err_printf("bogus remove from queue\n"));
    if (table[index].head == qe) {
	table[index].head = qe->next;
	if (table[index].tail == qe) {
	    table[index].tail = 0;
	}
	table[index].lock.release();
	return 0;
    }
    Element *prev = table[index].head;
    while (prev->next != qe) {
	tassert(prev->next, err_printf("bogus remove from queue\n"));
	prev = prev->next;
    }
    prev->next = qe->next;
    if (table[index].tail == qe) {
	table[index].tail = prev;
    }
    table[index].lock.release();
    return 0;
}

/* virtual */ SysStatus
BlockedThreadQueues::wakeupFirst(void * on)
{
    uval index = hash(uval(on));
    table[index].lock.acquire();
    Element *ptr = table[index].head;
    while (ptr) {
	if (ptr->blockedOn == on) {
	    Scheduler::Unblock(ptr->threadID);
	    table[index].lock.release();
	    return 0;
	}
	ptr = ptr->next;
    };
    table[index].lock.release();
    return 0;
}

/* virtual */ SysStatus
BlockedThreadQueues::wakeupAll(void * on)
{
    uval index = hash(uval(on));
    table[index].lock.acquire();
    Element *ptr = table[index].head;
    while (ptr) {
	if (ptr->blockedOn == on) {
	    Scheduler::Unblock(ptr->threadID);
	}
	ptr = ptr->next;
    };
    table[index].lock.release();
    return 0;
}

/* static */ void
BlockedThreadQueues::ClassInit(VPNum vp, MemoryMgrPrimitive *memory)
{
    if (vp==0) {
	BlockedThreadQueues *ptr = new(memory) BlockedThreadQueues();
	ptr->init();
	new(memory) CObjRootSingleRepPinned(
	    ptr, (RepRef)GOBJ(TheBlockedThreadQueuesRef));
    }
}

/* virtual */ SysStatus
BlockedThreadQueues::postFork()
{
    init();
    return 0;
}

void
BlockedThreadQueues::init()
{
    uval i;
    for(i=0; i<hashSize; i++) {
	table[i].lock.init();
	table[i].head = table[i].tail = 0;
    }
}
