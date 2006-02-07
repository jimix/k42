#ifndef __TIMER_EVENT_BC_H_
#define __TIMER_EVENT_BC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TimerEventBC.H,v 1.6 2005/07/10 16:08:17 dilma Exp $
 *****************************************************************************/

#include <scheduler/Scheduler.H>

class TimerEvent;
class KFSGlobals;

class TimerEventBC : TimerEvent {
public:
#if 1
    static const SysTime DEFAULT_INTERVAL = 30; // 30 secs
#else
    static const SysTime DEFAULT_INTERVAL = 10*60; // 10 minutes
#endif // if 0
    static const SysTime DEFAULT_INTERVAL_STRESSING = 10; // 10 secs

    SysTime interval;
    KFSGlobals *globals;

    // local strict, since timeout occurs on same processor always
    DEFINE_GLOBAL_NEW(TimerEventBC);
    TimerEventBC(KFSGlobals *g)
	: interval (Scheduler::TicksPerSecond()*DEFAULT_INTERVAL),
	  globals(g) {}

    static void Event(uval);
    static void ScheduleEvent(TimerEventBC *te);
    virtual void handleEvent();
};

#endif // #ifdef __TIMER_EVENT_BC_H_
