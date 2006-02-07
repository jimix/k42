/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageFaultNotification.C,v 1.13 2004/07/21 20:06:01 mergen Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Class maintains information for notifying
 * page-fault completion
 * **************************************************************************/

#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include "mem/PageFaultNotification.H"
#include "exception/ProcessAnnex.H"

void
PageFaultNotification::initAsynchronous(ProcessAnnex *pa, uval id)
{
    threadBlocked = 0;
    prefetch = 0;
    processAnnex = pa;
    pageFaultId = id;
}

void
PageFaultNotification::initSynchronous()
{
    threadBlocked = 1;
    _wasWoken = 0;
    thread = Scheduler::GetCurThread();
}

void
PageFaultNotification::initPrefetch()
{
    threadBlocked = 0;
    prefetch = 1;
    pageFaultId = 1;
}

void
PageFaultNotification::doNotification()
{
    if (threadBlocked) {
	_wasWoken = 1;
	Scheduler::Unblock(thread);
    } else if (prefetch) {
	delete this;
    } else {
	processAnnex->notifyFaultCompletion(this);
    }
}
