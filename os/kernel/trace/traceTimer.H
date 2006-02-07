#ifndef __TRACE_TIMER_H_
#define __TRACE_TIMER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceTimer.H,v 1.7 2004/07/08 17:15:39 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/


// trace timer event for entering heartbeat trace events
// must be called from the vp it is running on - no cross vp support
class TraceTimerEvent : protected TimerEvent {
    VPNum myvp;
    SysTime interval;

public:
    DEFINE_PINNEDLOCALSTRICT_NEW(TraceTimerEvent);

    TraceTimerEvent() {
	myvp = Scheduler::GetVP();
	// this should produce slightly more than 4 events per
	//   32 bit clock wrap and gives us a nice multiplier
	//   to check when they're really occuring
	interval = 1000000000; // 0x3b9aca00
	TraceOSControlHeartbeat(Scheduler::SysTimeNow(), myvp);
	disabledScheduleEvent(interval, TimerEvent::relative);
    }

    void handleEvent() {
	TraceOSControlHeartbeat(Scheduler::SysTimeNow(), myvp);
	disabledScheduleEvent(interval, TimerEvent::relative);
	return;
    }
};

#endif /* #ifndef __TRACE_TIMER_H_ */
