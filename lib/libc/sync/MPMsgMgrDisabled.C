/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgrDisabled.C,v 1.26 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages that are processed disabled
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <trace/traceScheduler.h>
#include "MPMsgMgrDisabled.H"

SysStatus
MPMsgMgrDisabled::processSendQueue()
{
    MsgHeader *list, *hdr;

    list = (MsgHeader *) FetchAndClearVolatile((uval *)(&sendQueue.head));
    while (list != NULL) {
	/*
	 * It is critical to read the "next" field out of the message before
	 * calling the function, because the function may free the message,
	 * allowing it to be reallocated and placed on some other list.
	 */
	hdr = list;
	list = hdr->next;
	TraceOSSchedulerDisabledMPMsg(
			(((uval64 *) GetMessage(hdr))[0]),
			(((uval64 *) GetMessage(hdr))[1]));
	GetMessage(hdr)->handle();
    }
    return 0;
}

/*
 * processReplyQueue() is implemented in the superclass.
 */

/*static*/ void
MPMsgMgrDisabled::ProcessSendQueue(SoftIntr::IntrType)
{
    Scheduler::GetDisabledMsgMgr()->processSendQueue();
}

/*static*/ void
MPMsgMgrDisabled::ProcessReplyQueue(SoftIntr::IntrType)
{
    Scheduler::GetDisabledMsgMgr()->processReplyQueue();
}

/*static*/ void
MPMsgMgrDisabled::ClassInit(DispatcherID dspid, MemoryMgrPrimitive *pa)
{
    static MPMsgMgrRegistryRef theRegistryRef = NULL;

    MPMsgMgrDisabled *mgr;
    mgr = new(pa) MPMsgMgrDisabled;
    tassert(mgr != NULL, err_printf("failed to create MPMsgMgrDisabled.\n"));
//    err_printf("disabled ref: %p %p\n", theRegistryRef, &theRegistryRef);

    mgr->init(dspid, pa, theRegistryRef);
    mgr->setInterruptFunctions(SoftIntr::MP_DISABLED_SEND,  ProcessSendQueue,
			       SoftIntr::MP_DISABLED_REPLY, ProcessReplyQueue);
    Scheduler::SetDisabledMsgMgr(mgr);
    mgr->addToRegistry(dspid);
}

/*static*/ void
MPMsgMgrDisabled::PostFork()
{
    MPMsgMgrDisabled *const mgr = Scheduler::GetDisabledMsgMgr();

    mgr->postFork();
    mgr->setInterruptFunctions(SoftIntr::MP_DISABLED_SEND,  ProcessSendQueue,
			       SoftIntr::MP_DISABLED_REPLY, ProcessReplyQueue);
    mgr->addToRegistry(SysTypes::DSPID(0,0));
}
