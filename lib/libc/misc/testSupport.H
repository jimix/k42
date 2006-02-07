#ifndef __TEST_SUPPORT_H_
#define __TEST_SUPPORT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testSupport.H,v 1.10 2002/10/10 13:08:20 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Support for starting and joinging threads for testing
 ***************************************************************************/

// Test support code

#include <sync/Barrier.H>
#include "simpleThread.H"
#include <scheduler/Scheduler.H>

/*====================================================================*/

// used to trigger simos events

extern "C" {
    void tstEvent_startworker();
    void tstEvent_endworker();
    void tstEvent_starttest();
    void tstEvent_endtest();
}


class TestStructure {
public:
    Barrier     *bar;
    uval         workers;
    SysTime      time1, time2, time3, time4, time5;
    uval         instr1, instr2;
    uval         size;
    uval         iters;
    uval         test;
    uval         misc;
    void        *ptr;

    DEFINE_GLOBAL_NEW(TestStructure);

    void init(uval w, uval s, uval i, uval t, uval m, void *p, Barrier *b) {
	workers=w; size=s; iters=i; test=t; misc=m; ptr=p; bar=b;
	time1 = time2 = 0;
    }
    static TestStructure *Create(uval w, uval s, uval i, uval t, uval m,
				 void *p, Barrier *b) {
	TestStructure *ts = new TestStructure[w];
	uval j;
	for (j = 0; j < w; j++) {
	    ts[j].init(w,s,i,t,m,p,b);
	}
	return ts;
    }
};

extern void MakeMP(VPNum numVP);
extern void DoConcTest(VPNum numWorkers, SimpleThread::function func,
		       TestStructure *p);


#endif /* #ifndef __TEST_SUPPORT_H_ */
