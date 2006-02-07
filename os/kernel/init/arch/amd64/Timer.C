/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Timer.C,v 1.2 2002/05/01 15:44:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    PwrPC specific definition of a timer class
 *    responsible to set the timer counters etc.
 * **************************************************************************/

#include "kernIncs.H"
#include "exception/ExceptionLocal.H"
#include "Timer.H"

void Timer::init()
{
    /* FIXME -- X86-64 */
    // initialize the TimerBase to all-zero
    asm("li r3,0; mttbl r3; mttbu r3; mttbl r3");
}

void Timer::setTicks(uval ticks)
{
    /* FIXME -- X86-64 */
    asm volatile("mtdec %0" : : "r" (ticks));
}

uval Timer::getTicks()
{
    /* FIXME -- X86-64 */
    uval ticks;
    asm volatile("mfdec %0" : "=r" (ticks));
    return ticks;
}

/* currently we are resetting to 0x11111 which is at an assumed
 * decrement rate of 40MHz 600 interrupts per second all we do right
 * now is to note that we got an interrupt, reset and return
 */

const uval timerSetValue = 0x11111;

void testTimerInt()
{
    /* FIXME -- X86-64 */
    uval count = 0;
    uval num_dec;
    Timer::init();

    cprintf("Spinning for 10 timer interrupts\n");

    num_dec = exceptionLocal.num_dec;
    do {
	num_dec = exceptionLocal.num_dec;
	Timer::setTicks(timerSetValue);
	do {
	    if (exceptionLocal.num_dec != num_dec) {
		count++;
		break;
	    } else {
		uval ticks = Timer::getTicks();
		cprintf("dec-reg=<0x%lx> #-ints=%ld\n",
			ticks,exceptionLocal.num_dec);
	    }
	} while (1);
    } while (count < 10);
}
