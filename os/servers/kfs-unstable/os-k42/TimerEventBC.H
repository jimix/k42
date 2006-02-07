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
 * $Id: TimerEventBC.H,v 1.1 2004/02/11 23:03:58 lbsoares Exp $
 *****************************************************************************/

#include <scheduler/Scheduler.H>

class TimerEvent;
class BlockCacheK42;

class TimerEventBC : TimerEvent {
public:
    static const SysTime DEFAULT_INTERVAL = 30; // 30 secs
    static const SysTime DEFAULT_INTERVAL_STRESSING = 10; // 10 secs

    SysTime interval;
    BlockCacheK42 *blkCache;

    // local strict, since timeout occurs on same processor always
    DEFINE_GLOBAL_NEW(TimerEventBC);
    TimerEventBC(BlockCacheK42 *bc)
	: interval (Scheduler::TicksPerSecond()*DEFAULT_INTERVAL),
	  blkCache(bc) {}

    static void Event(uval);
    static void ScheduleEvent(TimerEventBC *te);
    virtual void handleEvent();
};

#endif // #ifdef __TIMER_EVENT_BC_H_
