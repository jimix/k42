/****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVMessagesClient.C,v 1.1 2003/11/12 02:27:21 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: client (user space) packet ring class
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRoot.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerTimer.H>
#include <sync/BlockedThreadQueues.H>
#include <sync/atomic.h>
#include <stub/StubSysVMessages.H>
#include <meta/MetaSysVMessages.H>
#include "SysVMessagesClient.H"


/* static */SysVMessagesClient* SysVMessagesClient::obj = 0;

/* static*/ SysStatus
SysVMessagesClient::ClassInit()
{
    if(obj) return 0;
    obj = new SysVMessagesClient;
    obj->init();
    CObjRootSingleRep::Create(obj);
    return 0;
}

void
SysVMessagesClient::init()
{
    callBackOH.init();
}

SysStatus
SysVMessagesClient::GetCallBackOH(ObjectHandle& oh, void*& blockKey)
{
    ClassInit();
    return obj->getCallBackOH(oh, blockKey);
}

SysStatus
SysVMessagesClient::getCallBackOH(ObjectHandle& oh, void*& blockKey)
{
    ProcessID pid;
    SysStatus rc;

    pid = DREFGOBJ(TheProcessRef)->getPID();
    if (pid != callBackOH.pid()) {
	/* either we've never initialized or
	 * this is a fork child and we need
	 * a new call back oh.
	 */
	ProcessID serverPID;
	ObjectHandle serverOH;
	StubSysVMessages::__Get__metaoh(serverOH);
	passertMsg(!serverOH.invalid(), "No SysVMessages server\n");
	serverPID=serverOH.pid();
	/*
	 * make oh to this object for async callback
	 */
	rc = giveAccessByServer(callBackOH, serverPID);
	passertMsg(_SUCCESS(rc), "giveAccessByServer failed %ld\n", rc);
    }
    oh = callBackOH;
    blockKey = (void*)obj;
    return 0;
}

SysStatus
SysVMessagesClient::_notify()
{
    DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void *)obj);
    return 0;
}
