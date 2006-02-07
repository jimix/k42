/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: deadlock.C,v 1.1 2005/09/01 21:27:40 neamtiu Exp $
 *****************************************************************************/

/******************************************************************************
Description:

    Dynamic deadlock detection. 
    GuardedLock is essentially a BLock, but lock acquisition is protected by
    dynamic deadlock check. If trying to acquire the lock closes a cycle in the
    resource allocation graph, a cycle is reported and the program exits.

Compiling:

    add this file and MyGraph.H to kitchsrc/usr,
    add 'deadlock' to USRTARGETS in kitchsrc/usr/Makefile
    and build using 'make deadlock'

Running:
    ./deadlock on a K42 victim. It should print 'Cycle in the lock graph!'

 *****************************************************************************/

#include <sys/sysIncs.H>
#include <io/DiskClientAsync.H>
#include <stdlib.h>
#include <sys/systemAccess.H>
#include <scheduler/Scheduler.H>
#include <stdio.h>

#include "MyGraph.H"

int threadIds[2] = {0,1};
int lockIds[2] = {2,3};
char * names[] = {"Thread 0", "Thread 1", "Lock 0", "Lock 1"};

class GuardedLock {

protected:
  MyGraph * pg;
  BLock bLock;
  int id;
  static int counter;

public:
  GuardedLock(MyGraph * pg_) :
    pg(pg_),
    id(counter++)
  {
    bLock.init();
  }

  void acquire(unsigned threadId)
  {
    int tid = threadIds[threadId];
    int lid = lockIds[id];

    if (pg->existsPath(lid, tid))
      {
        printf("Cycle in the lock graph!\n");
        //exit(1);
      }
    else
      {
        pg->addEdge(tid, lid);
        printf("GuardedLock::acquire() : adding edge %s -> %s\n", names[tid], names[lid]);
        bLock.acquire();
        pg->removeEdge(tid, lid);
        pg->addEdge(lid, tid);
      }
  }
};

int GuardedLock::counter = 0;

GuardedLock *locks[2] = {NULL,NULL};

void
thread(uval id)
{
  printf("Thread %ld starting.\n", id);

  locks[id]->acquire(id);
  printf("Thread %ld acquired lock %ld\n", id, id);

  Scheduler::DelaySecs(5);

  locks[1 - id]->acquire(id);
  printf("Thread %ld acquired lock %ld\n", id, 1 - id);
}

int
main(int argc, char **argv)
{

  NativeProcess();

  MyGraph * pg = new MyGraph();
    
  locks[0] = new GuardedLock(pg);
  locks[1] = new GuardedLock(pg);

  Scheduler::ScheduleFunction(thread, 0);
  Scheduler::ScheduleFunction(thread, 1);

  Scheduler::Block();

  printf("Main done\n");

  return 0;
}
    
