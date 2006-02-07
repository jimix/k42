/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: semTest.C,v 1.4 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for semaphores.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/syscalls.H>
#include <scheduler/Scheduler.H>
#include <sync/Sem.H>
#include <sys/systemAccess.H>

class MySem {
private:
    BLock lock;
    BaseSemaphore<BLock> sem;

public:

    MySem() {
        lock.init();
        sem.init(0);
    }

    uval tryZ() {
        uval rc;

        lock.acquire();
        rc = sem.tryZ();
        lock.release();

        return rc;
    }

    sval Z() {
        sval rc;

        lock.acquire();
        rc = sem.Z(&lock);
        lock.release();

        return rc;
    }

    uval tryP(sval c = 1) {
        uval rc;

        lock.acquire();
        rc = sem.tryP(c);
        lock.release();

        return rc;
    }

    uval P(sval c = 1) {
        uval rc;

        lock.acquire();
        cprintf("%ld before P\n", sem.get());
        rc = sem.P(&lock, c);
        cprintf("%ld after P\n", sem.get());
        lock.release();

        return rc;
    }

    void V(sval c = 1) {
        lock.acquire();
        cprintf("%ld before V\n", sem.get());
        sem.V(c);
        cprintf("%ld after V\n", sem.get());
        lock.release();
    }

    sval get(void) {
        sval rc;

        lock.acquire();
        rc = sem.get();
        lock.release();

        return rc;
    }
};

// use a semaphore to block until all "threads" complete
MySem blocker;

struct MyThreadID {
    uval id;
    MySem *sem;
};

void
producer(uval id)
{
    MyThreadID *tid = (MyThreadID *)id;

    for (uval i = 0; i < 20; i++) {
        cprintf("%lu attempt V\n", tid->id);
        tid->sem->V(1);
        Scheduler::DelayMicrosecs(100000);
    }

    cprintf("producer (%lu) done [%ld]\n", tid->id, blocker.get());
    blocker.V();
}

void
consumer(uval id)
{
    MyThreadID *tid = (MyThreadID *)id;

    for (uval i = 0; i < 5; i++) {
        cprintf("%lu attempt P\n", tid->id);
        tid->sem->P(1);
    }

    cprintf("consumer (%lu) done [%ld]\n", tid->id, blocker.get());
    blocker.V();
}

void
slow_consumer(uval id)
{
    MyThreadID *tid = (MyThreadID *)id;

    for (uval i = 0; i < 10; i++) {
        cprintf("%lu attempt P\n", tid->id);
        tid->sem->P(1);
        Scheduler::DelayMicrosecs(1000000);
    }

    cprintf("slow consumer (%lu) done [%ld]\n", tid->id, blocker.get());
    blocker.V();
}

void
waiter(uval id)
{
    MyThreadID *tid = (MyThreadID *)id;

    for (uval i = 0; i < 4; i++) {
        cprintf("%lu attempt Z\n", tid->id);
        tid->sem->Z();
    }

    cprintf("waiter (%lu) complete [%ld]\n", tid->id, blocker.get());
    blocker.V();
}

/* The semaphore variables are really local to the following main
   program.  It was moved out to avoid an internal compiler error
   with gcc:

   kitchsrc/usr/semTest.C:203:
   *** Error ***: Internal compiler error
   in eliminate_regs, at reload1.c:2593
*/

// create a semaphore
MySem sem;

int
main(int argc, char *argv[])
{
    NativeProcess();

    MyThreadID id[15];
    uval i, j = 0;

    // setup the thread ids
    for (i = 0; i < 15; i++) {
        id[i].id = i;
        id[i].sem = &sem;
    }

    // create the consumers
    for (i = 0; i < 4; i++, j++) {
        (void) Scheduler::ScheduleFunction(consumer, (uval)&(id[j]));
    }

    // create a producer
    (void) Scheduler::ScheduleFunction(producer, (uval)&(id[j]));
    j++;

    // create waiters
    for (i = 0; i < 4; i++, j++) {
        (void) Scheduler::ScheduleFunction(waiter, (uval)&(id[j]));
    }

    blocker.P(9);
    cprintf("All threads complete (%ld, %ld)\n", blocker.get(), sem.get());

    // jump the semaphore high
    sem.V(10);

    // create waiters
    for (i = 0; i < 4; i++, j++) {
        (void) Scheduler::ScheduleFunction(waiter, (uval)&(id[j]));
    }

    // create a slow consumer
    (void) Scheduler::ScheduleFunction(slow_consumer, (uval)&(id[j]));

    blocker.P(5);
}
