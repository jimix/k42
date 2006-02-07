#ifndef __BLOCKED_THREAD_QUEUES_H_
#define __BLOCKED_THREAD_QUEUES_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockedThreadQueues.H,v 1.19 2002/05/09 19:39:40 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains queues of threads blocked on
 * specified addresses.
 * **************************************************************************/
#include <scheduler/Thread.H>
#include <alloc/MemoryMgrPrimitive.H>

class BlockedThreadQueues : Obj {
public:
    struct Element {
	ThreadID threadID;
	Element *next;
	void *blockedOn;
    };
protected:
    BlockedThreadQueues() { /* empty body */ }
    DEFINE_GLOBAL_NEW(BlockedThreadQueues);
    DEFINE_PRIMITIVE_NEW(BlockedThreadQueues);
    // FIXME: look at implementation notes in .C, make into per-proc hash
    // first stab is to use a hash table with per bucket lock.
    // fixed size - its hard to extend
    enum {hashSize=1024, logHashSize = 10, hashMask = 1023};
    struct {
	BLock  lock;
	Element *head;
	Element *tail;
    } table[hashSize];

    void init();

    uval hash(uval key) {
	// we'd like a universal hash but I'm lazy and universal
	// hashes can be slow - so make something up
	key = key * 0x9e3779b99e3779b9ull;	// arbitrary value
	key = key >> (64-10);  // unsigned shift
	return key;
    }
    
public:

    // add requesting thread to queue on address
    virtual SysStatus addCurThreadToQueue(Element *qe, void * on);

    // remove requesting thread from queue on address
    virtual SysStatus removeCurThreadFromQueue(Element *qe, void *on);

    // wakeup first thread queued on address
    virtual SysStatus wakeupFirst(void * on);

    // wakeup all threads queued on address
    virtual SysStatus wakeupAll(void * on);

    static void ClassInit(VPNum vp, MemoryMgrPrimitive *memory);

    virtual SysStatus postFork();

};

#endif /* #ifndef __BLOCKED_THREAD_QUEUES_H_ */
