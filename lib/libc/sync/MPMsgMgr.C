/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgr.C,v 1.71 2005/06/09 13:11:03 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages sent between virtual
 * processors of the application it is in.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ppccore.H>
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>
#include <sync/BlockedThreadQueues.H>
#include "MPMsgMgr.H"

class MPMsgMgr::MPMsgMgrRegistry : Obj {
public:
    virtual SysStatus lookupMgr(DispatcherID dspid, MPMsgMgr *&mgr) {
	RDNum rd; VPNum vp;
	SysTypes::UNPACK_DSPID(dspid, rd, vp);
	if ((rd >= Scheduler::RDLimit) || (vp >= Scheduler::VPLimit)) {
	    return _SERROR(1353, 0, EINVAL);
	}
	mgr = manager[rd][vp];
	if (mgr == NULL) {
	    BlockedThreadQueues::Element qe;
	    DREFGOBJ(TheBlockedThreadQueuesRef)->
		addCurThreadToQueue(&qe, (void *) this);
	    while ((mgr = manager[rd][vp]) == NULL) {
		Scheduler::Block();
	    }
	    DREFGOBJ(TheBlockedThreadQueuesRef)->
		removeCurThreadFromQueue(&qe, (void *) this);
	}
	return 0;
    }

    virtual SysStatus registerMgr(DispatcherID dspid, MPMsgMgr *mgr) {
	RDNum rd; VPNum vp;
	SysTypes::UNPACK_DSPID(dspid, rd, vp);
	if ((rd < Scheduler::RDLimit) &&
	    (vp < Scheduler::VPLimit) &&
	    (manager[rd][vp] == NULL))
	{
	    manager[rd][vp] = mgr;
	    DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void *) this);
	    return 0;
	} else {
	    return _SERROR(1354, 0, EINVAL);
	}
    }

    static SysStatus Create(MPMsgMgrRegistryRef &ref, MemoryMgrPrimitive *pa) {
	MPMsgMgrRegistry *registry;
	registry = new(pa) MPMsgMgrRegistry;
	if (registry == NULL) {
	    return _SERROR(1355, 0, ENOMEM);
	}
	ref = (MPMsgMgrRegistryRef) ((pa != NULL) ?
		    CObjRootSingleRepPinned::CreatePrimitive(registry, pa) :
		    CObjRootSingleRepPinned::Create(registry));
	if (ref == NULL) {
	    delete registry;
	    return _SERROR(1356, 0, ENOMEM);
	}
	return 0;
    }

    virtual SysStatus postFork() {
	init();
	return 0;
    }

private:
    void init() {
	RDNum rd; VPNum vp;
	for (rd = 0; rd < Scheduler::RDLimit; rd++) {
	    for (vp = 0; vp < Scheduler::VPLimit; vp++) {
		manager[rd][vp] = NULL;
	    }
	}
    }

    MPMsgMgrRegistry() {
	init();
    }

    void * operator new(size_t size, MemoryMgrPrimitive *pa) {
	uval addr;
	if (pa != NULL) {
	    pa->alloc(addr, size, sizeof(uval));
	} else {
	    addr = uval(allocGlobalPadded(size));
	}
	return (void *) addr;
    }

    MPMsgMgr *manager[Scheduler::RDLimit][Scheduler::VPLimit];
};

void *
MPMsgMgr::alloc(uval size)
{
    if (size > MAX_MSG_SIZE) {
	tassertWrn(0, "alloc size too big\n");
	return NULL;
    } else {
	uval msgIdx;
	allocMsgLock.acquire();
	for (;;) {
	    for (msgIdx = nextMsgIdx; msgIdx < NUM_MSGS; msgIdx++) {
		if (!msgHolder[msgIdx].busy) goto found;
	    }
	    for (msgIdx = 0; msgIdx < nextMsgIdx; msgIdx++) {
		if (!msgHolder[msgIdx].busy) goto found;
	    }
	    allocMsgLock.release();
	    Scheduler::Yield();
	    allocMsgLock.acquire();
	}
    found:
	msgHolder[msgIdx].busy = 1;
	nextMsgIdx = msgIdx;	// don't increment; hoping for re-use
	allocMsgLock.release();
	return msgHolder[msgIdx].msgSpace;
    }
}

void *
MPMsgMgr::alloc(uval size, MsgSpace &space)
{
    if (size > MAX_MSG_SIZE) {
	tassertWrn(0, "alloc size too big\n");
	return NULL;
    } else {
	MsgHolder *const holder = (MsgHolder *) ALIGN_UP(space.holderSpace,
							 MSG_CHUNK_SIZE);
	holder->manager = this;
	return holder->msgSpace;
    }
}

void
MPMsgMgr::addPendingMsg(MsgHeader *hdr, MsgQueue &q)
{
    SysStatus rc;
    MsgHeader *oldHead;

    do {
	oldHead = q.head;
	hdr->next = oldHead;
	/*
	 * FIXME: really just need a sync here to ensure changes to
	 * msg complete before putting on queue, re-write
	 * machine specific assembly, e.g., an enqueue function
	 */
    } while (!CompareAndStoreSynced((uval *)(&q.head),
				    uval(oldHead), uval(hdr)));
    if (oldHead == NULL) {
	rc = DREFGOBJ(TheProcessRef)->sendInterrupt(thisDspID, q.interruptBit);
	tassert(_SUCCESS(rc), err_printf("sendInterrupt failed.\n"));
    }
}

SysStatus
MPMsgMgr::MsgAsync::send(DispatcherID dspid)
{
    MsgHeader *const hdr = GetHeader(this);
    SysStatus rc;
    MPMsgMgr *targetMgr;

    rc = DREF(hdr->manager->getRegistry())->lookupMgr(dspid, targetMgr);
    tassert(_SUCCESS(rc), err_printf("MPMsgAsync: lookupMgr failed.\n"));
    targetMgr->addPendingSend(hdr);
    return 0;
}

void
MPMsgMgr::MsgAsync::free()
{
    MsgHeader *const hdr = GetHeader(this);
    hdr->busy = 0;
}

SysStatus
MPMsgMgr::MsgSync::send(DispatcherID dspid)
{
    MsgHeader *const hdr = GetHeader(this);
    SysStatus rc;
    MPMsgMgr *targetMgr;

    rc = DREF(hdr->manager->getRegistry())->lookupMgr(dspid, targetMgr);
    tassert(_SUCCESS(rc), err_printf("MPMsgAsync: lookupMgr failed.\n"));
    hdr->sender = Scheduler::GetCurThread();
    targetMgr->addPendingSend(hdr);

    /*
     * This is the trivial implementation, where we immediately block when
     * sending a cross processor message.  We have a more sophisticated
     * algorithm which attempts to spin for a while and then blocks if
     * information in the reply queue is not for us.  FIXME, implement this
     */
    while (hdr->sender != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    /*
     * With the message queue implementation, message returned is the same
     * as message sent.
     */
    return 0;
}

void
MPMsgMgr::MsgSync::reply()
{
    // reply may be called disabled, so we must allow primitive ppc
    // we do it here rather than in addPendingReply to avoid other
    // cases which cannot be disabled.
    uval isDisabled = Scheduler::IsDisabled();
    MsgHeader *const hdr = GetHeader(this);
    if (isDisabled) {
	ALLOW_PRIMITIVE_PPC();
	hdr->manager->addPendingReply(hdr);
	UN_ALLOW_PRIMITIVE_PPC();
    } else {
	hdr->manager->addPendingReply(hdr);
    }
}

SysStatus
MPMsgMgr::processReplyQueue()
{
    MsgHeader *list;
    ThreadID thid;

    list = (MsgHeader *) FetchAndClearVolatile((uval *)(&replyQueue.head));
    while (list != NULL) {
	thid = list->sender;
	list->sender = Scheduler::NullThreadID;
	list = list->next;
	Scheduler::DisabledUnblockOnThisDispatcher(thid);
    }
    return 0;
}

/*
 * some convenient packaged versions - add what you need
 */
class MPMsgMgr::MsgAsyncUval : public MPMsgMgr::MsgAsync {
public:
    SysStatusFunctionUval function;
    uval value;

    virtual void handle() {
	const SysStatusFunctionUval func = function;
	const uval val = value;
	free();
	(void) func(val);
    }
};

/*static*/ SysStatus
MPMsgMgr::SendAsyncUval(MPMsgMgr *mgr, DispatcherID dspid,
			SysStatusFunctionUval func, uval val)
{
    SysStatus rc;
    MsgAsyncUval *const msg = new(mgr) MsgAsyncUval;
    tassert(msg != NULL, err_printf("message allocate failed.\n"));

    msg->function = func;
    msg->value = val;

    rc = msg->send(dspid);

    return rc;
}

class MPMsgMgr::MsgSyncUval : public MPMsgMgr::MsgSync {
public:
    SysStatusFunctionUval function;
    uval value;
    SysStatus rc;

    virtual void handle() {
	rc = function(value);
	reply();
    }
};

/*static*/ SysStatus
MPMsgMgr::SendSyncUval(MPMsgMgr *mgr, DispatcherID dspid,
		       SysStatusFunctionUval func, uval val,
		       SysStatus &funcRC)
{
    SysStatus rc;
    MsgSpace msgSpace;
    MsgSyncUval *const msg = new(mgr, msgSpace) MsgSyncUval;

    msg->function = func;
    msg->value = val;
    msg->rc = -1;	// in case the send fails

    rc = msg->send(dspid);
    funcRC = msg->rc;

    return rc;
}

void
MPMsgMgr::MsgQueue::init()
{
    head = NULL;
    interruptBit = SoftIntr::MAX_INTERRUPTS;	// out of bounds
}

void
MPMsgMgr::MsgQueue::setInterruptFunction(SoftIntr::IntrType bit,
					   SoftIntr::IntrFunc func)
{
    interruptBit = bit;
    Scheduler::SetSoftIntrFunction(bit, func);
}

void
MPMsgMgr::init(DispatcherID dspid, MemoryMgrPrimitive *pa,
	       MPMsgMgrRegistryRef &registry)
{
    SysStatus rc;

    sendQueue.init();
    replyQueue.init();

    allocMsgLock.init();
    thisDspID = dspid;

    // allocate array of buffers
    tassert((sizeof(MsgHolder) == MSG_HOLDER_SIZE), err_printf("oops\n"));
    const uval amt = NUM_MSGS * sizeof(MsgHolder);
    uval space;
    if (pa != NULL) {
	pa->alloc(space, amt, MSG_CHUNK_SIZE);
    } else {
	space = uval(allocGlobalPadded(amt));
    }
    tassert(space != 0, err_printf("couldn't allocate msg buffers\n"));
    msgHolder = (MsgHolder *) space;

    uval i;
    for (i = 0; i < NUM_MSGS; i++) {
	msgHolder[i].manager = this;
	msgHolder[i].busy = 0;
    }
    nextMsgIdx = 0;

    // Create the registry, but don't register ourselves yet because our
    // interrupt handlers haven't been installed.
    if (dspid == SysTypes::DSPID(0,0)) {
	if (registry!=NULL) {
	    uval* y = (uval*)PAGE_ROUND_UP((uval)&registry);
	    uval* x = (uval*)PAGE_ROUND_DOWN((uval)&registry);
	    while (x < y) {
		if (*x) {
		    err_printf("%p: %lx\n",x,*x);
		}
		++x;
	    }
	}

	passertMsg(registry == NULL,"MPMsgMgr already initialized %p\n",
		   registry);
	rc = MPMsgMgrRegistry::Create(registry, pa);
	tassert(_SUCCESS(rc), err_printf("MPMsgMgrRegistry::Create failed\n"));
    }
    registryRef = registry;
}

void
MPMsgMgr::postFork()
{
    SysStatus rc;

    sendQueue.init();
    replyQueue.init();

    allocMsgLock.init();

    // reinitialize buffers
    uval i;
    for (i = 0; i < NUM_MSGS; i++) {
	msgHolder[i].busy = 0;
    }
    nextMsgIdx = 0;

    // clear the registry
    rc = DREF(registryRef)->postFork();
    tassert(_SUCCESS(rc), err_printf("registry postFork() failed.\n"));
}

void
MPMsgMgr::setInterruptFunctions(
		    SoftIntr::IntrType sendBit,  SoftIntr::IntrFunc sendFunc,
		    SoftIntr::IntrType replyBit, SoftIntr::IntrFunc replyFunc)
{
    sendQueue.setInterruptFunction(sendBit, sendFunc);
    replyQueue.setInterruptFunction(replyBit, replyFunc);
}

void
MPMsgMgr::addToRegistry(DispatcherID dspid)
{
    SysStatus rc;

    rc = DREF(registryRef)->registerMgr(dspid, this);
    tassert(_SUCCESS(rc), err_printf("register MPMsgMgr failed.\n"));
}

void *
MPMsgMgr::operator new(size_t size, MemoryMgrPrimitive *pa)
{
    uval space;
    if (pa != NULL) {
	pa->alloc(space, size, sizeof(uval));
    } else {
	space = uval(allocGlobalPadded(size));
    }
    return (void *) space;
}
