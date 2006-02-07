#ifndef __SCHEDULER_TIMER_H_
#define __SCHEDULER_TIMER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SchedulerTimer.H,v 1.22 2003/09/29 21:17:53 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: manage timer events in scheduler
 *
 * **************************************************************************/
#include <sys/TimerEvent.H>
#include <misc/hardware.H>
#include <alloc/MemoryMgrPrimitive.H>

class SchedulerTimer {
    // for timers 0 - 1<<IMMBITS into the future
    AutoListHead immediate;
    AutoListHead *table;
    TimerEvent* anchor; 		// sorted list of pending events
    SysTime now;			// estimate of current time
    SysTime when;			// time of next interrupt
    void migrate(SysTime start, SysTime end, AutoListHead &runList);
public:
    static SysTime TicksPerSecond();
    virtual ~SchedulerTimer() {};
    struct ForkData {
	DEFINE_NOOP_NEW(ForkData);
	AutoListHead *table;
    };
    DEFINE_NOOP_NEW(SchedulerTimer);

    virtual void init(MemoryMgrPrimitive *memory);
    virtual void getPreForkData(ForkData *fd);
    virtual void initPostFork(ForkData *fd);
    void timerInterrupt(SoftIntr::IntrType);	// process a timer pop
    SysTime disabledScheduleEvent(TimerEvent* timerEvent,
				  SysTime whenTime,
				  TimerEvent::Kind kind);
    static SysTime SysTimeNowInternal();
    static void TimerInterrupt(SoftIntr::IntrType);	// process a timer pop
    // returns current time in system units
    static SysTime SysTimeNow() {
	if (usermodeClock) return getClock();
	return SysTimeNowInternal();
    }
    virtual void printStatus();
    static void PrintStatus();
};


#endif /* #ifndef __SCHEDULER_TIMER_H_ */
