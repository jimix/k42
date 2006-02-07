/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: micro2.C,v 1.16 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <misc/hardware.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "bench.H"

#define COUNT_NUM 5000l

extern "C" int pthread_setconcurrency(int level);

/////////////////////////////////////////////////////////////////////////////
// Test 5: Measure yield times between busy VPs in the same process.
uval test5_done = 0;

void
yield_thread5(uval v_param)
{
    while(!test5_done) {
	Scheduler::YieldProcessor();
    }
}

// Launch a yield thread on the target VP:
struct StartupChildMsg : public MPMsgMgr::MsgSync {
    virtual void handle() {
	passert(Scheduler::IsDisabled(),
		err_printf("Dispatcher not disabled.\n"));

	SysStatus rc =
	    Scheduler::DisabledScheduleFunction(yield_thread5, 0);

	passert(_SUCCESS(rc),
		err_printf("DisabledScheduleFunction failed.\n"));

	reply();
    }
};

void test5()
{
    printf("########################################\n"
	   "# Test 5: Busy VP yield time\n"
	   "#    -> Measures time taken to yield a VP when there are busy\n"
	   "#       VPs in the same process (checks how kernel\n"
	   "#       scheduler overhead scales with the number of VPs.)\n");

    int rc;
    VPNum vp;
    TIMER_DECL(COUNT_NUM);

    sleep(2);

    printf("plot yield_vs_numbvp %ld yellow\n", Scheduler::VPLimit);

    if(DREFGOBJ(TheProcessRef)->ppCount() > 1) {
	printf("WARNING: test results not valid if "
	       "physical processors > 1.\n");
    }

    for(vp = 0; vp < Scheduler::VPLimit; vp++) {
	// Set the number of VPs:
	rc = pthread_setconcurrency(vp + 1);

	if (rc != 0) {
	    printf("Error, pthread_setconcurrency() returned %d\n", rc);
	    break;
	}

	if(vp != 0) {
	    MPMsgMgr::MsgSpace msgSpace;
	    StartupChildMsg *const msg =
		new(Scheduler::GetDisabledMsgMgr(), msgSpace)
		StartupChildMsg;
	    passert(msg != NULL,
		    err_printf("message allocate failed.\n"));

	    SysStatus rc = msg->send(SysTypes::DSPID(0, vp));

	    passert(_SUCCESS(rc), err_printf("message send failed.\n"));
	}

	// Do it once to warm up the cache:
	Scheduler::YieldProcessor();

	START_TIMER;
	Scheduler::YieldProcessor();
	END_TIMER;

	printf("%ld %f %f %f\n", vp + 1, AVG_TIME, MIN_TIME, MAX_TIME);
    }

    // NOTE: at the end of this test case we have many more VPs active.

    test5_done = 1;

    printf("xlabel Number of VPs\n"
	   "ylabel Yield time (us)\n"
	   "title Scheduler::YieldProcessor() time vs. number of busy VPs\n"
	   "# All times averaged over %ld iterations\n", COUNT_NUM);
}

int
main (void)
{
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    printf("###############################################################\n"
	   "# K42 Threading Microbenchmarks Part II\n"
	   "# \n"
	   "# Number of CPUs: %ld\n"
	   "# Ticks per second: %ld\n"
	   "###############################################################\n",
	   DREFGOBJ(TheProcessRef)->ppCount(),
	   (unsigned long)Scheduler::TicksPerSecond());


    test5();
    return 0;
}
