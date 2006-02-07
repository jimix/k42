#ifndef __TIMER_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __TIMER_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Timer.H,v 1.7 2001/10/05 21:49:46 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    PwrPC specific definition of a timer class
 *    responsible to set the timer counters etc.
 * **************************************************************************/

class Timer {
public:
    static void init();
    static void setTicks(uval ticks);
    static uval getTicks();
};
