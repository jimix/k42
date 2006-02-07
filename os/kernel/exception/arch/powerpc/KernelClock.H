#ifndef __POWERPC_KERNEL_CLOCK_H_
#define __POWERPC_KERNEL_CLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelClock.H,v 1.15 2001/10/16 05:32:07 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine dependent functions for clock manipulation
 * **************************************************************************/

#include <misc/hardware.H>
#include "bilge/arch/powerpc/BootInfo.H"

class KernelClock {
public:
    void debug() { /* empty body */ }
    SysTime getTicksPerSecond() { return _BootInfo->timebase_frequency; }
    SysTime getClock() {
	//some machines (not power) require disabled access
	//so this interface requires disabled calls
	tassert(!hardwareInterruptsEnabled(),
		err_printf("interrupts enabled accessing clock\n"));
	return ::getClock(); }
    void    setClock(SysTime now) {
	tassert(!hardwareInterruptsEnabled(),
		err_printf("interrupts enabled accessing clock\n"));
	::setClock(now); }
    void    setInterval(SysTime interval) {
	tassert(!hardwareInterruptsEnabled(),
		err_printf("interrupts enabled accessing clock\n"));
	if (interval > ::maxDecrementer()) interval = ::maxDecrementer();
	// powerpc 630 bug - don't set decrementor to 0 - it may checkstop
	if (interval == 0) {
	    interval = 1;
//	    err_printf("630 bug fix setting interval to 1\n");
	}
	::setDecrementer(interval);
	return;
    }
    void init() { /* empty body */ }
};

#endif /* #ifndef __POWERPC_KERNEL_CLOCK_H_ */
