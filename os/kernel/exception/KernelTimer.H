#ifndef __KERNEL_TIMER_H_
#define __KERNEL_TIMER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelTimer.H,v 1.21 2002/08/22 14:48:18 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of exception level timer support
 *                     this deals with recording one timer event per
 *                     vp and delivering timer interrupts to the vp
 *
 *                     Note that the kernel also has normal thread level
 *                     timer services, just like any other process
 *
 *                     For now, this looks very similar to Timer.H etc.
 *                     But we ultimately expect different implementations
 *                     so have not tried to abstract common functions.
 * **************************************************************************/

#include <sys/TimerEvent.H>
#include <sys/KernelInfo.H>
#include <misc/hardware.H>
#include __MINC(KernelClock.H)

class ProcessAnnex;

/*
 * we chain these via the process annex's that contain them.
 * this solves the problem of "finding" the process annex
 * associated with a timer event.
 * The alternative of subtracting the offset and casting was
 * to ugly to stomach.
 * The alternative of storing the ProcessAnnex pointer in each event
 * was to silly to stomach.
 */
class KernelTimerEvent {
    friend class KernelTimer;
    SysTime when;			// -1 when not active
    ProcessAnnex* next;
public:
    KernelTimerEvent() {when = (SysTime)-1;}
};

extern "C" SysTime KernelTimer_TimerRequest(SysTime whenTime,
					    TimerEvent::Kind kind);

class KernelTimer {
    friend void genConstants(void);
    friend void run_test(char *tbuf, uval len, uval doDefault);
    friend SysTime KernelTimer_TimerRequest(SysTime whenTime,
					    TimerEvent::Kind kind);
    friend class DispatchQueue;
    friend class CPUDomainAnnex;
    friend class ExceptionLocal;

    ProcessAnnex* anchor;		// annex containing first event
    SysTime when;
    SysTime dispatchTime;
    KernelClock kernelClock;		// machine dependent stuff

public:
    /*
     * Schedule an event for the specified process annex.
     * Must be called disabled.
     */
    SysTime timerRequest(SysTime whenTime, TimerEvent::Kind kind,
			 ProcessAnnex* source);

    /*
     * Detach the specified process annex from the kernel timer queue.
     * Must be called disabled.
     */
    void remove(ProcessAnnex* pa) {
	tassertSilent( !hardwareInterruptsEnabled(), BREAKPOINT);
	(void) timerRequest(0, TimerEvent::reset, pa);
    }
    
    /*
     * Return current timer request time of the specified process annex.
     * Must be called disabled.
     */
    static SysTime TimerRequestTime(ProcessAnnex *pa);

    /*
     * Specify the next time at which the kernel dispatcher should be called.
     * Must be called disabled.
     */
    void setDispatchTime(SysTime now, SysTime quantum);

    void init();

    void initTOD(VPNum vp);

    void setTOD(KernelInfo::SystemGlobal* kip, uval sec, uval usec);

    void print();

    void timerInterrupt();	// process a timer pop
};

#endif /* #ifndef __KERNEL_TIMER_H_ */
