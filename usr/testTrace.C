/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testTrace.C,v 1.7 2004/10/11 20:46:50 cascaval Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user program that emits a user-specified string to
 * 		       the tracing buffer (for each processor of the system)
 * **************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <sys/sysIncs.H>
#include <trace/trace.H>
#include <trace/traceDbg.h>

int loop1()
{
    uval i, sum;
    sum = 0;
    for (i=0;i<1000000;i++) {
	sum+=i;
    }
    return sum;
}

int loop2()
{
    int i, sum;
    sum = 0;
    for (i=0;i<2000000;i++) {
	sum+=i;
    }
    return sum;
}

int main()
{
  unsigned long long i;
  unsigned long long int sum = 0;
  unsigned long long int count;


  count = 4;

  TraceOSDbgEvent2Start(1, 2);
  for (i = 0; i < count; i++) {
      TraceOSDbgd1A(1);
      sum+=loop1();
      TraceOSDbgd2A(2, 3);
      sum+=loop2();
      TraceOSDbgd3A(4, 5, 6);
  }
  TraceOSDbgEvent2End(3, 4);

  return 0;
}

