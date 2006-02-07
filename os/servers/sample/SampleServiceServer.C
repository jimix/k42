/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SampleServiceServer.C,v 1.12 2003/03/25 13:14:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for testing user-level service invocation.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SampleServiceServer.H"
#include <cobj/CObjRootSingleRep.H>
#include <sys/ProcessWrapper.H>
#include <scheduler/Scheduler.H>
#include <alloc/PageAllocatorUser.H>

SysStatus
SampleServiceServer::testRequest()
{
    return 0;
}

SysStatus
SampleServiceServer::testRequestWithIncrement()
{
    (void) FetchAndAddSignedVolatile(&counter, 1);
    return 0;
}

SysStatus
SampleServiceServer::testRequestWithLock()
{
    AutoLock<BLock> al(&lock);
    counter++;
    return 0;
}

SysStatus
SampleServiceServer::testRequestWithFalseSharing()
{
    counterArray[Scheduler::GetVP()]++;
    return 0;
}

/*static*/ SysStatus
SampleService::GetServerPID(__out uval &pid)
{
    pid = DREFGOBJ(TheProcessRef)->getPID();
    return 0;
}

void
SampleService::init()
{
    lock.init();
    counter = 0;
    uval space;
    (void) DREFGOBJ(ThePageAllocatorRef)->allocPages(space, PAGE_SIZE);
    counterArray = (sval *) space;
    for (uval i = 0; i < Scheduler::VPLimit; i++) counterArray[i] = 0;
}


/*static*/ SysStatus
SampleService::Create(ObjectHandle &oh, __CALLER_PID caller)
{
    SampleService *ts;
    ObjRef ref;
    SysStatus rc;

    ts = new SampleServiceServer;
    if (ts == NULL) return _SERROR(1245, 0, ENOSPC);
    ref = (ObjRef) CObjRootSingleRep::Create(ts);
    if (ref == NULL) {
	delete ts;
	return _SERROR(1246, 0, ENOSPC);
    }
    rc = DREF(ref)->giveAccessByServer(oh, caller);

    if (_SUCCESS(rc)) {
	ts->init();
    } else {
	ts->destroy();
    }

    return rc;
}

static ThreadID BlockedThread = Scheduler::NullThreadID;

/*static*/ SysStatus
SampleService::Die()
{
    ThreadID tid;

    while (BlockedThread == Scheduler::NullThreadID) {
	Scheduler::DelayMicrosecs(100000);
    }
    tid = BlockedThread;
    BlockedThread = Scheduler::NullThreadID;
    Scheduler::Unblock(tid);
    return 0;
}

/*static*/ void
SampleServiceServer::Block()
{
    BlockedThread = Scheduler::GetCurThread();
    while (BlockedThread != Scheduler::NullThreadID) {
	// NOTE: this object better not go away while deactivated
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
}

/*static*/ void
SampleServiceServer::ClassInit()
{
    MetaSampleService::init();
}
