/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: multiDispatcher.C,v 1.3 2005/06/28 19:48:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>
#include <misc/simpleThread.H>
#include <sys/systemAccess.H>
#include <scheduler/DispatcherMgr.H>
#include <scheduler/DispatcherDefault.H>

// Size of a unit of work for a thread.
// Threads will be given a % of this amount of work.
#define UNITOFWORK 20000000

int sharedMemory1 = 0;
int sharedMemory2 = 0;

// Global variable for storing number of seconds to sleep.
int delayValue = 5;

SysStatus thread_func(void *arg)
{
    VPNum vp = Scheduler::GetVP();
    ThreadID tid = Scheduler::GetCurThread();
    cprintf("vp %lu: About to do some work. tid 0x%lx \n", vp, tid );

    // Make threads do stuff.
    // Amount of work done is inversely proportional to vp # that
    // it is on.
    for (uval ctr = 0; ctr < UNITOFWORK ; ctr++) {

        // updating the two shared memory items that are not in the same 
	// cache line (padded)

        sharedMemory1++;
        sharedMemory2++;

        if (!(ctr % 10000000)) { 
            cprintf("vp %lu: tid 0x%lx  counter passed %lu\n", vp, tid, ctr);
        }

	for (uval i=0; i < 5; i++);  // some delay 
    }

    cprintf("vp %lu: Finished doing work. tid 0x%lx  \n", vp, tid);
    return (SysStatus)arg;
}


// Hack: Using value of argc as number of seconds to sleep.
int
main(int argc, char *argv[])
{
    NativeProcess();

    VPNum numVP;

    cprintf("main: I am pid=0x%lx(%lu), vp=%lu, tid=0x%lx\n",
	    DREFGOBJ(TheProcessRef)->getPID(),
	    DREFGOBJ(TheProcessRef)->getPID(), Scheduler::GetVP(),
	    Scheduler::GetCurThread());

    // Figure out how many processors there are.
    numVP = DREFGOBJ(TheProcessRef)->ppCount();
    cprintf("There are %lu processors.\n", numVP);

    // Create a virtual processor for each physical processor:
    for (VPNum vp = 1; vp < numVP; vp++) {
	SysStatus rc;
	rc = ProgExec::CreateVP(vp);
	passert(_SUCCESS(rc), {});
    }

    // Create a thread to run on each vp.
    SimpleThread *threads[numVP];

    // Create a thread on each processor.
    for (VPNum vp=0; vp < numVP; vp++) {
	// Create a thread on vp.
	cprintf("Spawning a thread on vp %lu\n", vp);
	threads[vp] = SimpleThread::Create(thread_func, (void *)vp,
					   SysTypes::DSPID(0, vp));
	passert(threads[vp] != 0, err_printf("Thread creation failed\n"));
    }

    cprintf("Done. sleeping for %d seconds.\n", delayValue);
    // Wait for other thread on vp 0 to finish.
    Scheduler::DelaySecs(delayValue);

    return 0;
}
