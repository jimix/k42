/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgrException.C,v 1.37 2004/07/08 17:15:35 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages that are processed at exception level
 * **************************************************************************/
#include <kernIncs.H>
#include "HWInterrupt.H"
#include "ExceptionLocal.H"
#include "MPMsgMgrException.H"
#include <misc/hardware.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <trace/traceException.h>

SysStatus
MPMsgMgrException::processSendQueue()
{
    MsgHeader *list, *hdr;

    tassertSilent( !hardwareInterruptsEnabled(), err_printf("woops\n"));

    list = (MsgHeader *) FetchAndClearVolatile((uval *)(&sendQueue.head));
    while (list != NULL) {
	/*
	 * It is critical to read the "next" field out of the message before
	 * calling the function, because the function may free the message,
	 * allowing it to be reallocated and placed on some other list.
	 */
	hdr = list;
	list = hdr->next;
	TraceOSExceptionExceptionMPMsg(
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
MPMsgMgrException::ProcessMsgs()
{
    exceptionLocal.getExceptionMsgMgr()->processSendQueue();
    exceptionLocal.getExceptionMsgMgr()->processReplyQueue();
}

void
MPMsgMgrException::ClassInit(DispatcherID dspid, MemoryMgrPrimitive *pa)
{
    static MPMsgMgrRegistryRef theRegistryRef = NULL;

    MPMsgMgrException *mgr;
    mgr = new(pa) MPMsgMgrException;
    tassert(mgr != NULL, err_printf("failed to create MPMsgMgrException.\n"));

    mgr->init(dspid, pa, theRegistryRef);
    exceptionLocal.setExceptionMsgMgr(mgr);
    mgr->addToRegistry(dspid);
}
