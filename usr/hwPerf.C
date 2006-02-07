/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: hwPerf.C,v 1.1 2005/08/22 21:49:00 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/sysIncs.H>
#include <sys/SystemMiscWrapper.H>
#include <sys/systemAccess.H>

#define PMUOP(name, ...) \
{ \
rc = DREFGOBJ(TheSystemMiscRef)->hwPerf##name##AllProcs (__VA_ARGS__); \
if (_FAILURE(rc)) { \
   cprintf("PMU operation %s failed with rc 0x%lx\n", #name,	rc); \
   exit(rc); \
}\
}

#define TRCOP(name, ...) \
{ \
rc = DREFGOBJ(TheSystemMiscRef)->trace##name (__VA_ARGS__); \
if (_FAILURE(rc)) { \
   cprintf("TRACE operation %s failed with rc 0x%lx\n", #name,	rc); \
   exit(rc); \
}\
}

#define TRCOPA(name, ...) \
{ \
rc = DREFGOBJ(TheSystemMiscRef)->trace##name##AllProcs (__VA_ARGS__); \
if (_FAILURE(rc)) { \
   cprintf("TRACE operation %s failed with rc 0x%lx\n", #name,	rc); \
   exit(rc); \
}\
}


#define ARRAY_SIZE 10000000
#define MUX_ROUND 100000
#define LOG_PERIOD 10000000
int
main()
{
    SysStatus rc;
    char array[ARRAY_SIZE ];

    NativeProcess();

    PMUOP(SetCountingMode,2);
    PMUOP(SetMultiplexingRound,MUX_ROUND);
    PMUOP(SetLogPeriod,LOG_PERIOD);
    PMUOP(AddGroup,1,8,0);
    PMUOP(AddGroup,3,8,0);
    PMUOP(AddGroup,4,8,0);
    PMUOP(AddGroup,5,8,0);
    TRCOPA(SetMask,0x20000);
    TRCOPA(Reset);
    PMUOP(StartSampling,0);
    TRCOP(StartTraceD,1);   // I'm assuming "1" means all processes

    PMUOP(StartWatch);
    for (int i = 1; i < 100000000; i++) {
       array[i%ARRAY_SIZE] = array[(i-1)%ARRAY_SIZE];
       if (!(i % 50000)) {
           PMUOP(LogAndResetWatch);	
       }
    }
    PMUOP(StopWatch);
   
    TRCOPA(StopTraceD);
    PMUOP(StopSampling);
    return 0;
}
