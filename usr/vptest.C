/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: vptest.C,v 1.33 2005/06/28 19:48:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <sync/MPMsgMgr.H>
#include <sys/systemAccess.H>

struct TestMsg : public MPMsgMgr::MsgAsync {
    uval d1;
    uval d2;

    virtual void handle() {
	// Can't use cprintf disabled.
	err_printf("TestFunction (vp %ld): received d1 %ld, d2 %ld.\n",
		   Scheduler::GetVP(), d1, d2);
	free();
    }
};

int
main()
{
    NativeProcess();

    SysStatus rc;

    cprintf("vptest:  about to create second vp.\n");

    rc = ProgExec::CreateVP(1);

    cprintf("vptest:  ProgExec::CreateVP(1) returned 0x%lx.\n", rc);

    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();
    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();
    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();
    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();

    cprintf("vptest: calling sendDisabledAsync().\n");
    //breakpoint();

    TestMsg *const msg =
	new(Scheduler::GetDisabledMsgMgr()) TestMsg;
    tassert(msg != NULL, err_printf("message allocation failed.\n"));
    msg->d1 = 13;
    msg->d2 = 17;
    rc = msg->send(SysTypes::DSPID(0, 1));
    cprintf("vptest:  send disabled async() returned 0x%lx.\n", rc);

    cprintf("vptest:  sleeping.\n");
    Scheduler::DelaySecs(1);
    cprintf("vptest:  exiting.\n");
}
