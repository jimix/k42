/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerState.C,v 1.3 2002/10/10 13:08:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for keeping track of server states
 * **************************************************************************/

#include "kernIncs.H"
#include "ServerState.H"
#include <meta/MetaServerState.H>
#include <stub/StubServerState.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>

uval64 ServerState::stateMask=0;
ListSimpleKeyLocked<uval, uval, AllocGlobal> ServerState::list;

/*static */void
ServerState::ClassInit(VPNum vp)
{

    if (vp!=0) return;
    MetaServerState::init();
}

/*static*/ SysStatus
ServerState::waitForNotification(uval bit)
{
    list.acquireLock();
    if (!(stateMask & 1ULL<<bit)) {
	ThreadID thr =Scheduler::GetCurThread();
	uval tid= (uval)&thr;
	list.locked_add(bit, tid);
	list.releaseLock();

	//Wait for server to complete and wake us
	while (thr != Scheduler::NullThreadID) {
	    Scheduler::Block();
	}
    } else {
	list.releaseLock();
    }
    return 0;
}

/*static*/ SysStatus
ServerState::_pollForNotification(uval bit)
{
    SysStatus rc = 0;
    list.acquireLock();
    if (!(stateMask & 1ULL<<bit)) {
	rc = _SERROR(2284, 0, EAGAIN);
    }
    list.releaseLock();
    return rc;
}


/* static */ SysStatus
ServerState::_notifyServerState(uval state)
{
    list.acquireLock();
    stateMask |= 1ULL<<state;
    list.releaseLock();
    uval ptr;
    while (list.remove(state,ptr)) {
	ThreadID thr = *(ThreadID*)ptr;
	*(ThreadID*)ptr = Scheduler::NullThreadID;
	Scheduler::Unblock(thr);
    }
    return 0;
}
