/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: spin.C,v 1.12 2005/06/28 19:44:15 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <stdio.h>
#include <stdlib.h>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>

uval Iteration()
{
    uval i, sum;
    sum = 0;
    for (i = 0; i < 100000; i++) sum += i;
    return sum;
}


static int preemptNow = 0;

class CPUPreempt : public TimerEvent {    
public:
    DEFINE_GLOBAL_NEW(CPUPreempt);
    
    virtual void handleEvent() {
	preemptNow = 1;
    }
};

// EP
int setPreempt (uval ticks)
{
    static CPUPreempt *p = new CPUPreempt;

    printf("Scheduling event for %ld\n",ticks);
    p->scheduleEvent(ticks, TimerEvent::relative);
    printf("Done scheduling event\n");
    return 0;
}


int
main(int argc, char **argv)
{
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    if (argc < 2) {
	printf("Usage: %s <iters>\n", argv[0]);
	return -1;
    }

    uval const iters = atoi(argv[1]);

    setPreempt(uval(10000));
    
    printf("spin(%ld) start.preemptNow=%d\n", iters, preemptNow);
    for (uval i = 0; i < iters; i++) (void) Iteration();
    printf("spin(%ld) end.preemptNow=%d\n", iters, preemptNow);

    Scheduler::DelayUntil(Scheduler::TicksPerSecond(), TimerEvent::relative);
    printf("Done\n");
    
    return 0;
}
