/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: small_deadlock.cc,v 1.1 2005/09/01 21:27:35 neamtiu Exp $
 *****************************************************************************/

/******************************************************************************
Description:
       Simple test client for graph_internal.cc : create two threads
       that acquire locks circularly and end up in deadlock.  

Compiling:
       g++ -o small_deadlock -c small_deadlock.cc graph_internal.o

Running:
       ./small_deadlock        

 *****************************************************************************/
#include <string>
#include <sstream>
#include <iostream>
#include "litegraph.h"

class Lock {

protected:
  static unsigned counter;

protected:
  LiteGraph * lg;
  unsigned id;
  std::string lockName;


  std::string int2string(unsigned n)
  {
    std::stringstream sstr;
    sstr << n;
    return sstr.str();
  }

  std::string makeThreadName(unsigned threadId)
  {
    return (std::string)"Thread_" + int2string(threadId);
  }

public:
  Lock(LiteGraph * lg_) :
    lg(lg_),
    id(counter++),
    lockName((std::string)"Lock_" + int2string(id))
  {
    std::cout << "Lock::Lock: " << lockName << std::endl;
  }

  void acquire(unsigned threadId)
  {
    std::string threadName = makeThreadName(threadId);
    
    lg->addEdge(lockName, threadName);

    std::cout << "Lock::acquire: adding edge " << lockName << " -> " << threadName << std::endl;
  }

  void tryAcquire(unsigned threadId)
  {
    std::string threadName = makeThreadName(threadId);
    
    //lg->printGraph(lockName);

    if (lg->causesCycle(threadName, lockName))
      {
        std::cout << "Cycle when trying to add edge " << threadName << " -> " << lockName << std::endl;
        lg->printPath(lockName, threadName);
      }
    else
      {
        lg->addEdge(threadName, lockName);
        std::cout << "Lock::tryAcquire: adding edge " << threadName << " -> " << lockName << std::endl;
      }
  }
};

unsigned Lock::counter = 0;

int main(int argc, char **argv)
{
  unsigned t1 = 1, t2 = 2;
  LiteGraph * lg = new LiteGraph();
  Lock l1(lg),l2(lg);

  
  l1.acquire(t1);
  l2.acquire(t2);
  
  l1.tryAcquire(t2);
  l2.tryAcquire(t1);
}
