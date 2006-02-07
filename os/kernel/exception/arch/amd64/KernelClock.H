#ifndef __AMD64_KERNEL_CLOCK_H_
#define __AMD64_KERNEL_CLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelClock.H,v 1.4 2001/10/05 21:49:31 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine dependent functions for clock manipulation
 * **************************************************************************/

#include <misc/hardware.H>

class KernelClock {
public:
    SysTime when;
    SysTime lastInterval;
    SysTime maxInterval;
    void debug() {
	SysTime newInterval;
	newInterval = ::getClock() - when;
	if(newInterval > maxInterval) maxInterval = newInterval;
    }

    inline SysTime getTicksPerSecond();

    SysTime getClock() { return ::getClock(); }
    void    setClock(SysTime now) { /*::setClock(now);*/ }
    void    setInterval(SysTime interval) {
	if (interval > ::maxDecrementer()) interval = ::maxDecrementer();
	::setDecrementer(interval);
	// testing
	when = ::getClock();
	lastInterval = interval;
	return;
    }
    void init() {
	tassert(!hardwareInterruptsEnabled(),
		err_printf("must init timer disabled\n"));
	maxInterval = 0;
	imr_enable(0);
    }
};

// definitely check this one pdb XXX
#include <scheduler/Scheduler.H>
SysTime
KernelClock::getTicksPerSecond()
{
    return KernelInfo::OnSim()?300000:300000000;
}
#endif /* #ifndef __AMD64_KERNEL_CLOCK_H_ */
