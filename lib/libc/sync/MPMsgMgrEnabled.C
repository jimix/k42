/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgrEnabled.C,v 1.25 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages that are processed enabled
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <trace/traceScheduler.h>
#include "MPMsgMgrEnabled.H"

/*static*/ void
MPMsgMgrEnabled::ProcessMsgList(uval listUval)
{
    MsgHeader *list, *hdr;
    SysStatus rc;

    list = (MsgHeader *) listUval;
    do {
	hdr = list;
	list = hdr->next;
	if (list != NULL) {
	    /*
	     * Process the tail of the list on another thread, in case we
	     * block while handling the work item at the head.  If we succeed
	     * in creating a new thread, we no longer have responsibility for
	     * the remainder of the list and must exit the loop after handling
	     * the first item.
	     */
	    rc = Scheduler::ScheduleFunction(ProcessMsgList, uval(list));
	    if (_SUCCESS(rc)) list = NULL;
	}
	TraceOSSchedulerEnabledMPMsg(
			(((uval64 *) GetMessage(hdr))[0]),
			(((uval64 *) GetMessage(hdr))[1]));
	GetMessage(hdr)->handle();
    } while (list != NULL);
}

SysStatus
MPMsgMgrEnabled::processSendQueue()
{
    SysStatus rc;
    MsgHeader *list;

    list = (MsgHeader *) FetchAndClearVolatile((uval *)(&sendQueue.head));
    if (list != NULL) {
	rc = Scheduler::DisabledScheduleFunction(ProcessMsgList, uval(list));
	tassert(_SUCCESS(rc),
		err_printf("should have a reserved thread for this\n"));
    }
    return 0;
}

/*
 * processReplyQueue() is implemented in the superclass.
 */

/*static*/ void
MPMsgMgrEnabled::ProcessSendQueue(SoftIntr::IntrType)
{
    Scheduler::GetEnabledMsgMgr()->processSendQueue();
}

/*static*/ void
MPMsgMgrEnabled::ProcessReplyQueue(SoftIntr::IntrType)
{
    Scheduler::GetEnabledMsgMgr()->processReplyQueue();
}

void
MPMsgMgrEnabled::ClassInit(DispatcherID dspid, MemoryMgrPrimitive *pa)
{
    static MPMsgMgrRegistryRef theRegistryRef = NULL;

    MPMsgMgrEnabled *mgr;
    mgr = new(pa) MPMsgMgrEnabled;
    tassert(mgr != NULL, err_printf("failed to create MPMsgMgrEnabled.\n"));

    mgr->init(dspid, pa, theRegistryRef);
    mgr->setInterruptFunctions(SoftIntr::MP_ENABLED_SEND,  ProcessSendQueue,
			       SoftIntr::MP_ENABLED_REPLY, ProcessReplyQueue);
    Scheduler::SetEnabledMsgMgr(mgr);
    mgr->addToRegistry(dspid);
}

/*static*/ void
MPMsgMgrEnabled::PostFork()
{
    MPMsgMgrEnabled *const mgr = Scheduler::GetEnabledMsgMgr();

    mgr->postFork();
    mgr->setInterruptFunctions(SoftIntr::MP_ENABLED_SEND,  ProcessSendQueue,
			       SoftIntr::MP_ENABLED_REPLY, ProcessReplyQueue);
    mgr->addToRegistry(SysTypes::DSPID(0,0));
}
