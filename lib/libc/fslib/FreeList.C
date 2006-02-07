/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FreeList.C,v 1.2 2004/07/08 17:15:30 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <trace/traceFS.h>
#include <fslib/ServerFile.H>
#include "FreeList.H"

/* virtual */ SysStatus
FreeList::freeServerFile(FSFile::FreeServerFileNode *n)
{
#ifdef DESTRUCTION_DEAMON_ON
    if (te != NULL) {
	n->timeFreed = Scheduler::SysTimeNow();
	freeSF.prepend(n);

	tassertMsg(te != NULL, "ops\n");

	// adjust how often the daemon is invoked
#ifdef DESTRUCTION_DAEMON_STRESS_TESTING
	te->interval = Scheduler::TicksPerSecond()*
	    TimerEventFS::DEFAULT_MIN_INTERVAL_STRESSING;
#else
	te->interval = Scheduler::TicksPerSecond()*
	    TimerEventFS::DEFAULT_MIN_INTERVAL;
#endif /* #ifdef DESTRUCTION_DAEMON_STRESS_TESTING */
    }
#endif /* #ifdef DESTRUCTION_DEAMON_ON */

    return 0;
}


/* virtual */ SysStatus
FreeList::unFreeServerFile(FSFile::FreeServerFileNode *n)
{
    freeSF.lock();
    n->lockedDetach();
    freeSF.unlock();
    return 0;
}


/* This goes through freeSF list trying to destroy files that have been in
 * the list for more than "ticksecs" ticks.
 * Also, the method returns 1 if the freeSF list has elements on it, and
 * 0 if it's currently empty
 */

/* virtual */ SysStatusUval
FreeList::tryDestroyFreeFiles(uval ticksecs)
{
    tassertMsg(te != NULL && te != (TimerEventFS*) 1, " ops\n");

    TraceOSFSTryDestroyFReeFilesStarted((uval) this);

    SysTime now = Scheduler::SysTimeNow();
    SysStatus rc = 0;

    while (1) {
	FSFile::FreeServerFileNode *nd;
	freeSF.lock();
	nd = (FSFile::FreeServerFileNode *)freeSF.next();
	// if nothing free, or head is newer than timeout done
	if ((nd == NULL) || (nd->timeFreed + ticksecs > now) ) {
	    if (nd != NULL) {
		rc = 1;
	    }
	    freeSF.unlock();
	    TraceOSFSTryDestroyFReeFilesFinished((uval)this,
		     (uval)rc);
	    return rc;
	}

	// indicates that list is not empty
	rc = 1;

	/* FIXME: comment below is saying that ServerFile should
	 * get out of the list ... and I think we should take it out here
	 */
	/*
	 * Cannot invoke ServerFile while holding lock, since it may
	 * detach from list while holding its own lock.  So, we cache
	 * info, release lock, and invoke ServerFile.  This could
	 * return error if Server file has gone away, if so ignore.
	 * ServerFile should remove from list and either deal with it or
	 * re-enqueue.
	 */
	ServerFileRef r = nd->ref;
	freeSF.unlock();
	DREF(r)->tryToDestroy();
    }
}

class TimerEventFS:TimerEvent {
public:
    // default values
    static const SysTime DEFAULT_MIN_INTERVAL = 120; // 2 minutes
    static const SysTime DEFAULT_MIN_INTERVAL_STRESSING = 30; // 30 secs
    static const SysTime DEFAULT_MAX_INTERVAL = 60 * 10; // 10 minutes
    static const SysTime DEFAULT_MAX_INTERVAL_STRESSING = 60 * 2; // 2 minutes
    static const SysTime DEFAULT_CALL_ARG = 120; // 5 minutes
    static const SysTime DEFAULT_CALL_ARG_STRESSING = 30; // 30 secs
    static const SysTime DEFAULT_INTERVAL_INCREASE = 30; // 30 secs;

    SysTime interval;
    SysTime callArg;
    FreeList *freeList;

    // local strict, since timeout occurs on same processor always
    DEFINE_LOCALSTRICT_NEW(TimerEventFS);
    static void Event(uval);
    static void ScheduleEvent(TimerEventFS *te);
    virtual void handleEvent();
};

/* static */ void
TimerEventFS::Event(uval arg)
{
    TimerEventFS *te = (TimerEventFS *)arg;
    SysStatusUval rc = te->freeList->tryDestroyFreeFiles(te->callArg);
    tassertMsg(_SUCCESS(rc), "ops\n");

    /* adjust how often this event should be scheduled: if the freeSF list
     * is currently empty, we increase the interval up to a maximum
     */
    if (_SGETUVAL(rc) == 0) {
	te->interval += Scheduler::TicksPerSecond()*DEFAULT_INTERVAL_INCREASE;
	if (te->interval > Scheduler::TicksPerSecond()*DEFAULT_MAX_INTERVAL) {
	    te->interval = Scheduler::TicksPerSecond()*DEFAULT_MAX_INTERVAL;
	}
    }

    // re-schedule event here, rather than disabled, in case this took a
    // long time to run
    TimerEventFS::ScheduleEvent(te);
}

/* static */ void
TimerEventFS::ScheduleEvent(TimerEventFS *te)
{
    te->scheduleEvent(te->interval, TimerEvent::relative);
}

/* virtual */ void
TimerEventFS::handleEvent()
{
    SysStatus rc = Scheduler::DisabledScheduleFunction(TimerEventFS::Event,
						       (uval)this);
    tassertSilent(_SUCCESS(rc), BREAKPOINT);
}


/* virtual */ SysStatus
FreeList::setupFileDestruction(uval secsWake, uval secsCall)
{
#ifdef DESTRUCTION_DEAMON_ON
    te = new TimerEventFS;

    te->freeList = this;
    if (secsWake == 0) {
	// use default values
#ifdef DESTRUCTION_DAEMON_STRESS_TESTING
	te->interval = Scheduler::TicksPerSecond()
	    *TimerEventFS::DEFAULT_MAX_INTERVAL_STRESSING;
#else
	te->interval =
	    Scheduler::TicksPerSecond()*TimerEventFS::DEFAULT_MAX_INTERVAL;
#endif /* #ifdef DESTRUCTION_DAEMON_STRESS_TESTING */
    } else {
	te->interval = Scheduler::TicksPerSecond()*secsWake;
    }

    if (secsCall == 0) {
	// use default values
#ifdef DESTRUCTION_DAEMON_STRESS_TESTING
	te->callArg = Scheduler::TicksPerSecond()*
	    TimerEventFS::DEFAULT_CALL_ARG_STRESSING;
#else
	te->callArg = Scheduler::TicksPerSecond()*TimerEventFS::DEFAULT_CALL_ARG;
#endif /* #ifdef DESTRUCTION_DAEMON_STRESS_TESTING */
    } else {
	te->callArg = Scheduler::TicksPerSecond()*secsCall;
    }

    TimerEventFS::ScheduleEvent(te);
#endif /* #ifdef DESTRUCTION_DEAMON_ON */

    return 0;
}
