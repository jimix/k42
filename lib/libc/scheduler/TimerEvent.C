/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TimerEvent.C,v 1.12 2004/03/04 19:33:01 mostrows Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <scheduler/DispatcherDefault.H>

SysTime
TimerEvent::scheduleEvent(SysTime whenTime, Kind kind)
{
    SysTime rt;
    Scheduler::Disable();
    rt =DISPATCHER->timer.disabledScheduleEvent(this, whenTime, kind);
    Scheduler::Enable();
    return rt;
}

SysTime
TimerEvent::disabledScheduleEvent(SysTime whenTime, Kind kind)
{
    return DISPATCHER->timer.disabledScheduleEvent(this, whenTime, kind);
}

