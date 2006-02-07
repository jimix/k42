/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testSupport.C,v 1.17 2003/11/24 12:33:36 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Support for starting and joinging threads for testing
 ***************************************************************************/

#include <sys/sysIncs.H>
#include "testSupport.H"
#include <usr/ProgExec.H>

// used to trigger simos events

void tstEvent_starttest() { /* empty body */ }
void tstEvent_endtest() { /* empty body */ }
void tstEvent_startworker() { /* empty body */ }
void tstEvent_endworker() { /* empty body */ }

// --------------------------------------------------------------------------

uval8 ProcsCreated[Scheduler::VPLimit];	// bool for which vps created
					// vp 0 is always assumed created

void
MakeMP(VPNum numVP)
{
    SysStatus rc;

    passert(numVP <= Scheduler::VPLimit, {});

    for (VPNum vp = 1; vp < numVP; vp++) {
	if (!ProcsCreated[vp]) {
	    rc = ProgExec::CreateVP(vp);
	    passert(_SUCCESS(rc), {});
	    ProcsCreated[vp] = 1;
	}
    }
}

// This function deactivates the calling thread and potentially blocks on
// calls to SimpleThread::join calls.  Please be sure that the calling
// thread is safe to be deactivated.
void
DoConcTest(VPNum numWorkers, SimpleThread::function func, TestStructure *p)
{
    VPNum vp;
    SimpleThread *threads[numWorkers];

    MakeMP(numWorkers);
    for (vp=0; vp < numWorkers; vp++) {
	threads[vp] = SimpleThread::Create(func,&p[vp],SysTypes::DSPID(0, vp));
	passert(threads[vp]!=0, err_printf("Thread create failed\n"));
    }

    tassert(Scheduler::GetCurThreadPtr()->isActive(),
	    err_printf("DoConcTest thread is already deative\n"));

    Scheduler::DeactivateSelf();
    for (vp=0; vp < numWorkers; vp++) {
	SimpleThread::Join(threads[vp]);
    }
    Scheduler::ActivateSelf();
}
