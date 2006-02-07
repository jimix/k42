/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SchedulerTimer.C,v 1.35 2004/11/05 16:23:58 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: manage timer events in scheduler
 *
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <scheduler/DispatcherDefault.H>
#include "SchedulerTimer.H"


extern "C" SysTime _TIMER_REQUEST(SysTime whenTime, TimerEvent::Kind kind);

//#undef tassertMsg
//#define tassertMsg(ARGS...) passertMsg(ARGS)

// Bits in immediate bucket window
static const uval IMMBITS = 22;

static const uval BUCKET_TICKS = (1ULL<<IMMBITS);
// 256 AutoListHead's in a page
static const uval TABLE_SIZE = PAGE_SIZE / sizeof(AutoListHead);
static inline uval bucketStart(SysTime curr) {
    return (curr & ~(BUCKET_TICKS-1));
}
static inline uval bucketEnd(SysTime curr) {
    return (curr & ~(BUCKET_TICKS-1)) + BUCKET_TICKS - 1;
}
static inline uval timeIndex(SysTime t) {
    return (t >> IMMBITS) & (TABLE_SIZE-1);
}



void
SchedulerTimer::init(MemoryMgrPrimitive * memory)
{
    SchedulerTimer tmp;
    // Get vtbl pointer right
    memcpy(this, &tmp, sizeof(tmp));
    anchor = 0;
    now = 0;
    when=SysTime(-1);
    uval ptr;
    memory->alloc(ptr, TABLE_SIZE*sizeof(AutoListHead), PAGE_SIZE);
    table = (AutoListHead*)ptr;

    uval i = 0;

    // We need to legitimately construct this object, and then make
    // copies of it.  This ensures that vtable pointers are set right
    // (we don't "construct" array elements).
    AutoListHead alh;
    alh.init();
    memcpy(&immediate, &alh, sizeof(AutoListHead));
    immediate.init();
    for (; i<TABLE_SIZE; ++i) {
	memcpy(&table[i],&alh, sizeof(AutoListHead));
	table[i].init();
    }
}


/* virtual */ void
SchedulerTimer::getPreForkData(ForkData* fd)
{
    fd->table = table;

#ifndef NDEBUG
    /*
     * Count the active timers and print a warning if there are more than we
     * expect.  We eventually expect there to be no timers active at fork time
     * in the common case, but for now we still have the timer that drives the
     * cleanup daemon.
     */
    uval count = 0;
    TimerEvent *te;
    te = (TimerEvent *) immediate.next();
    while (te != NULL) {
	count++;
	te = (TimerEvent *) te->next();
    }
    for (uval i = 0; i < TABLE_SIZE; i++) {
	te = (TimerEvent *) table[i].next();
	while (te != NULL) {
	    count++;
	    te = (TimerEvent *) te->next();
	}
    }
    // Looking for "== 1" rather than "<= 1" so that the assertion will get
    // noisy when the cleanup daemon is no longer timer-driven, which will
    // remind us to change the comparison to "== 0".
    tassertWrn(count == 1, "Process forking with %ld active timers.\n", count);
#endif // !NDEBUG
}

/*
 * if timer ever uses storage to do a more efficient job of managing
 * the set of timer events, then the PostFork init will differ from
 * the init above
 */
/* virtual */ void
SchedulerTimer::initPostFork(ForkData *fd)
{
    AutoListHead alh;
    alh.init();
    memcpy(&immediate, &alh, sizeof(AutoListHead));
    immediate.init();
    when = SysTime(-1);
    anchor = NULL;
    table = fd->table;
    for (uval i = 0; i < TABLE_SIZE; i++) {
	table[i].init();
    }
}

SysTime
SchedulerTimer::SysTimeNowInternal()
{
    SysTime retvalue;
    retvalue = usermodeClock ?
	getClock() : _TIMER_REQUEST(0, TimerEvent::queryNow);
    return (retvalue);
}

SysTime
SchedulerTimer::TicksPerSecond()
{
    return kernelInfoLocal.systemGlobal.ticksPerSecond;
}

#if 0
SysTime marcLimit = 1000000;
#endif /* #if 0 */

/*
 *
 * runs disabled to protect timer event structures.
 * also, _TIMER_REQUEST must only be called disabled
 */
SysTime
SchedulerTimer::disabledScheduleEvent(TimerEvent* timerEvent,
				      SysTime whenTime, TimerEvent::Kind kind)
{
#if 0
    // kludge which can be used to catch unpinned timer events in the
    // kernel
    passertMsg((uval)timerEvent >= 0xc000000000000000ul ||
	       (uval)timerEvent < 0x8000000000000000ul, "opps\n");
#endif
    AutoListNode *list = NULL;
    SysTime old = when;
    switch (kind) {
    default:
	return 0;

    case TimerEvent::queryTicksPerSecond:
	return kernelInfoLocal.systemGlobal.ticksPerSecond;

    case TimerEvent::queryNow:
	if (usermodeClock) {
	    now = getClock();
	} else {
	    now = _TIMER_REQUEST(0, TimerEvent::queryNow);
	}
	break;

    case TimerEvent::reset:

	list = timerEvent->head();
	if (!list) {
	    break;
	}

	timerEvent->lockedDetach();
	if (likely(timerEvent != anchor &&
		   // timerEvent bucket is non-empty
		   !(when == bucketStart(timerEvent->when)
		     && !list->next()))) {
	    break;
	}

	if (timerEvent == anchor) {
	    anchor = NULL;
	    list = immediate.next();
	    when = SysTime(-1);
	    while (list) {
		TimerEvent* te = (TimerEvent*)list;
		list = list->next();
		if (!anchor || anchor->when > te->when) {
		    anchor = te;
		    when = anchor->when;
		}
	    }
	}
	if (!anchor) {
	    SysTime currSlot = bucketStart(timerEvent->when + BUCKET_TICKS);
	    while (timeIndex(currSlot)!= timeIndex(timerEvent->when)
		   && !table[timeIndex(currSlot)].next()) {
		currSlot += BUCKET_TICKS;
	    }

	    if (timeIndex(currSlot) == timeIndex(timerEvent->when)) {
		when = SysTime(-1);
	    } else {
		when = currSlot;
	    }
	}
	if ( old != when ) {
	    if (when == SysTime(-1)) {
		now = _TIMER_REQUEST(when, TimerEvent::reset);
	    } else {
		now = _TIMER_REQUEST(when, TimerEvent::absolute);
	    }
	}
	tassertMsg(when == SysTime(-1) ||
		   (!anchor && (when == bucketStart(when))) ||
		   (anchor && when == anchor->when), "bad timer setting\n");

	break;

    case TimerEvent::relative:
	tassertMsg(timerEvent->head() == NULL, "event on list\n");
	if (unlikely(whenTime==0)) {
	    now = _TIMER_REQUEST(0, TimerEvent::queryNow);
	    timerEvent->when = now;
	    timerEvent->lockedDetach();
	    timerEvent->handleEvent();
	    break;
	}
	if (!usermodeClock) {
	    // N.B. this call changes the event time only if the
	    // new event is sooner than the current event if any
	    now = _TIMER_REQUEST(whenTime, TimerEvent::relative);
	    whenTime += now;
	} else {
	    // we can read the clock cheaply
	    now = getClock();
	    whenTime+=now;
	}

	// fall through to absolute case


    case TimerEvent::absolute:
#if 0
	//code to catch someone setting small timeouts
	if (
	    whenTime-now<marcLimit) {
	    // don't complain about what is most likely the
	    // dispatcher two minute warning
//	    if (!(DREFGOBJ(TheProcessRef)->getPID() == 0 &&
//		  whenTime-now == 20000)) {
		err_printf("small whenTime %lld pid %ld\n",
			   whenTime-now, DREFGOBJ(TheProcessRef)->getPID());
	}
#endif /* #if 0 */

	tassertMsg(timerEvent->head() == NULL, "event on list\n");

	if (unlikely(whenTime <= now)) {
	    now = _TIMER_REQUEST(0, TimerEvent::queryNow);
	    timerEvent->when = now;
	    timerEvent->lockedDetach();
	    timerEvent->handleEvent();
	    break;
	}

	timerEvent->when = whenTime;


	if (timerEvent->when <= bucketEnd(now)) {
	    // Event is in immediate window
	    if (!anchor || anchor->when > timerEvent->when) {
		// Event is next to go off --- requires timer adjustment
		tassertMsg(!anchor ||
			   timerEvent->when <= bucketEnd(anchor->when),
			   "event not in immediate bucket\n");
		immediate.append(timerEvent);
		anchor = timerEvent;
		when = whenTime;

		// We may have already set the timer above
		if (usermodeClock) {
		    if (old != when) {
			now = _TIMER_REQUEST(when, TimerEvent::absolute);
		    }
		}
		tassertMsg(when == SysTime(-1) ||
			   (!anchor && (when == bucketStart(when))) ||
			   (anchor && when == anchor->when),
			   "bad timer setting\n");
		break;
	    }
	    tassertMsg(!anchor ||
		       timerEvent->when <= bucketEnd(now),
		       "event not in immediate bucket\n");
	    immediate.prepend(timerEvent);
	} else {
	    // Dump event into the right bucket
	    table[timeIndex(timerEvent->when)].append(timerEvent);
	    if (timerEvent->when < when) {

		// Set timer for bucket of this event
		when = bucketStart(timerEvent->when);
		anchor = NULL;
		if (old != when) {
		    now = _TIMER_REQUEST(when, TimerEvent::absolute);
		}
		tassertMsg(when == SysTime(-1) ||
			   (!anchor && (when == bucketStart(when))) ||
			   (anchor && when == anchor->when),
			   "bad timer setting\n");

	    }
	}
	tassertMsg(when <= timerEvent->when,
		   "No timer before scheduled event: %p %p\n",
		   this, timerEvent);
	tassertMsg(when == SysTime(-1) ||
		   (!anchor && (when == bucketStart(when))) ||
		   (anchor && when == anchor->when), "bad timer setting\n");
    }

#if 0
    {
	SchedulerTimer *st = this;
	for (uval i = 0; i<TABLE_SIZE; ++i) {
	    TimerEvent *t = (TimerEvent*)st->table[i].next();
	    while (t) {
		tassertMsg(timeIndex(t->when) != timeIndex(now) ||
			   t->when > bucketEnd(now),
			   "event not in immediate: %p %lx %lx %lx %lx\n",
			   timerEvent,
			   uval(now), uval(now), uval(t->when), uval(when));
		tassertMsg(t->when > now, "stale event in table %p\n", t);

		t = (TimerEvent*)t->next();

	    }
	}
    }
#endif
    return now;
}



//
// Look through table, starting with the beginning of bucket for
// "start", through to the end of the bucket for "end" and put any
// expired timers onto the "runList", and any events due to expire
// before bucketEnd(end) on the immediaste list
//
void
SchedulerTimer::migrate(SysTime start, SysTime end, AutoListHead &runList)
{
    TimerEvent *cur;
    while (bucketEnd(start) <= bucketEnd(end)) {
	// No timers in immediate bucket, fill it
	cur = (TimerEvent*)table[timeIndex(start)].next();
	while (cur) {
	    TimerEvent *te = cur;
	    cur = (TimerEvent*)cur->next();
	    if (te->when <= now) {
		// Run expired timers
		te->lockedDetach();
		runList.prepend(te);
	    } else if (te->when <= bucketEnd(end)) {
		// Dump into current bucket
		te->lockedDetach();
		tassertMsg(!anchor || te->when <= bucketEnd(anchor->when),
			   "event not in immediate bucket\n");
		immediate.append(te);

		// Mark soonest expiring timer as anchor
		if (!anchor ||
		    te->when <= anchor->when) {
		    anchor = te;
		    when = te->when;
		}
	    }
	}
	start += BUCKET_TICKS;
    }
}

void
SchedulerTimer::PrintStatus() {
    Scheduler::Disable();
    DISPATCHER->timer.printStatus();
    Scheduler::Enable();
}

void
SchedulerTimer::printStatus() {
    SysTime curr = now;
    err_printf("Timer status at [%lx] <- %lx[%lx] -> [%lx] next %lx[%lx]\n",
	       bucketStart(curr), uval(curr), timeIndex(curr), bucketEnd(curr),
	       uval(when), timeIndex(when));

    for (uval i = 0; i<TABLE_SIZE; ++i) {
	TimerEvent *t = (TimerEvent*)table[i].next();
	if (t) {
	    err_printf("Bucket index: %lx\n",i);
	}
	while (t) {
	    err_printf("\tevent: %p when: %lx[%lx] vptr %p\n",
		       t, uval(t->when), timeIndex(t->when), *((uval**)t));
	    t = (TimerEvent*)t->next();

	}
    }

}

/*
 * called from interrupt handler in scheduler
 * disabled, NOT running on a thread
 */
void
SchedulerTimer::TimerInterrupt(SoftIntr::IntrType it) {
    DISPATCHER->timer.timerInterrupt(it);
}

void
SchedulerTimer::timerInterrupt(SoftIntr::IntrType)
{
    // use when as estimate of time unless its cheap to read
    // the clock
    // FIXME:  This usermodeClock trick assumes that interrupts won't be
    //         delivered earlier than expected, an assumption that is
    //         violated when a dispatcher is migrated from one physical
    //         processor to another.  An early interrupt will cause us to
    //         invoke event handlers before their expected times.  To fix
    //         this problem, we should pass the current time up to the
    //         dispatcher when we generate a soft timer interrupt for it.
    SysTime old = when;
    SysTime start = old;
    AutoListHead runList;

    runList.init();
    now = usermodeClock?getClock():when;

    TimerEvent* cur;
    if (unlikely(now < when)) {
	if (when == SysTime(-1)) {
	    now = _TIMER_REQUEST(when, TimerEvent::reset);
	} else {
	    now = _TIMER_REQUEST(when,TimerEvent::absolute);
	}
    }
    while (now > when) {

	cur = anchor;
	// Run the current timer
	anchor = NULL;
	if (cur) {
	    cur->lockedDetach();
//	    tassertWrn(old <= cur->when, "stale event: %lx %lx %lx\n",
//		       uval(old), uval(cur->when), uval(now));
	    runList.prepend(cur);
	}

	cur = (TimerEvent*)immediate.next();
	// Scan immediate timer list, looking for expired timers,
	// first timer to go off next.
	while (cur) {
	    TimerEvent *te = cur;
	    cur = (TimerEvent*)cur->next();
	    tassertMsg(bucketEnd(now)>=te->when,
		       "Bad event on immediate list: %lx %lx %p\n",
		       uval(now), uval(te->when), te);

	    if (te->when <= now) {
		te->lockedDetach();
		tassertWrn(old <= te->when, "stale event3: %lx %lx %lx\n",
			   uval(old), uval(te->when), uval(now));
		runList.prepend(te);
	    } else if (!anchor ||
		       te->when <= anchor->when) {
		anchor = te;
		when = te->when;
	    }
	}

	// Move all events for buckets containing  "start" to "now"
	// to immediate list
	migrate(start, now, runList);


	if (!anchor) {
	    // No "anchor" timer to go off next, look for the next bucket
	    // with timers.

	    // we start in current slot, then increment
	    SysTime nextSlot = bucketStart(now + BUCKET_TICKS);
	    while (timeIndex(old) != timeIndex(nextSlot)
		   && !table[timeIndex(nextSlot)].next()) {
		nextSlot += BUCKET_TICKS;
	    }

	    if (timeIndex(now) == timeIndex(nextSlot)
		&& !table[timeIndex(nextSlot)].next()) {

		when = SysTime(-1);
	    } else {
		when = nextSlot;
	    }
	}

#if 0

	if (when==SysTime(-1)) {
	    SchedulerTimer *st = &DISPATCHER->timer;
	    for (uval i = 0; i<TABLE_SIZE; ++i) {
		TimerEvent *t = (TimerEvent*)st->table[i].next();
		tassertMsg(t==NULL,
			   "Existing event, no timer: %lx %p %p %p %p\n",
			   i,t,st,DISPATCHER,this);
	    }
	}
	{
	    SchedulerTimer *st = &DISPATCHER->timer;
	    for (uval i = 0; i<TABLE_SIZE; ++i) {
		TimerEvent *t = (TimerEvent*)st->table[i].next();
		while (t) {
		    tassertMsg(timeIndex(t->when) != timeIndex(now) ||
			       t->when > bucketEnd(now),
			       "event not in immediate: %p %lx %lx %lx\n",t,
			       uval(now), uval(t->when), uval(when));

		    tassertMsg(t->when > now, "stale event in table %p\n", t);

		    t = (TimerEvent*)t->next();

		}
	    }
	}
#endif
	// adjust timer only if it has changed
	if (when != old) {
	    if (when == SysTime(-1)) {
		now = _TIMER_REQUEST(when, TimerEvent::reset);
	    } else {
		now = _TIMER_REQUEST(when,TimerEvent::absolute);
	    }
	}
    }

    // Run expired timers We do this after all of the processing abov,
    // rather than as we encounter these events because handleEvent()
    // may lead to SchedulerTimer::disabledScheduleEvent, and we don't
    // want that code to run while the timer object's state is in the
    // midst of the computations above -- we want that code to run on
    // a clean slate. Only now can we run these callbacks while
    // ensuring that SchedulerTimer is fully cleaned up.
    cur = (TimerEvent*)runList.next();
    while (cur) {
	TimerEvent *te = (TimerEvent*)cur;
	cur = (TimerEvent*)cur->next();
	te->lockedDetach();
	te->handleEvent();
    }
//    err_printf("next timer: %p %lx\n", DISPATCHER, uval(when));
}
