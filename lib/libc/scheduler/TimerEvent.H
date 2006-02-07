#ifndef __TIMER_EVENT_H_
#define __TIMER_EVENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TimerEvent.H,v 1.14 2003/08/06 22:02:32 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: User interface to timer event support.
 *
 * A TimerEvent can be scheduled.  When the time is reached, the function
 * specified is run disabled.
 *
 * A useful subclass is TimerUnblock, which unblocks a thread at the
 * specified time.
 *
 * Another useful subclass is TimerSleep, which sleeps for a specified
 * interval.
 *
 * N.B. - in the kernel, events must be allocated in pinned memory
 *        since they are manipulated at exception/disabled level in
 *        the scheduler where page faults are not allowed.
 *
 * **************************************************************************/

#include <misc/AutoList.H>

class TimerEvent: public AutoListNode {
    friend class SchedulerTimer;
protected:
    SysTime when;			// time this event will occur
public:
    TimerEvent() : AutoListNode(), when(0) {};
    enum Kind {reset, absolute, relative, queryNow, queryTicksPerSecond};
    virtual void handleEvent() = 0;
    virtual SysTime scheduleEvent(SysTime whenTime, Kind kind);
    virtual SysTime disabledScheduleEvent(SysTime whenTime, Kind kind);
};
#endif /* #ifndef __TIMER_EVENT_H_ */
