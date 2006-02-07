/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelClock.H,v 1.4 2001/10/16 05:32:06 okrieg Exp $
 *************************************************************************** */

#ifndef __GENERIC64_KERNEL_CLOCK_H_
#define __GENERIC64_KERNEL_CLOCK_H_

/*****************************************************************************
 * Module Description: machine dependent functions for clock manipulation
 * **************************************************************************/

/* FORWARD */ class KernelClock;

#include <misc/hardware.H>

class KernelClock {
    friend void genConstants(void);
    friend void choose_test(void);

    SysTime ticksPerSecond;

    // these are updated in the lolita timer routine after each timer int
    SysTime now;			// ticks since boot
    uval32  c0CountSnapshot;		// C0_COUNTER last time 'now' updated

    /* Make interval less than what hardware supports so that we can be
     * sure to get timer interrupt before count register wraps past last
     * snapshot.
     */
    enum { MAX_INTERVAL = SysTime(1) << 31 };

    /* Make interval greater than some small amount to ensure that
     * when compare register is set, it is greater than count at that
     * moment.
     */
    enum { MIN_INTERVAL = 0x200 };	// to deal with random cache misses...

public:

    void debug() { /* empty body */ }

    // following two for debugging
    SysTime getNow()			{ return now; }
    uval32  getc0CountSnapshot()	{ return c0CountSnapshot; }

    SysTime getTicksPerSecond()		{ return ticksPerSecond; }

    SysTime getClock() {
      return(now);
    }

    void setClock(SysTime nowarg);
    void setInterval(SysTime interval);
    void init();
};

#endif /* #ifndef __GENERIC64_KERNEL_CLOCK_H_ */
