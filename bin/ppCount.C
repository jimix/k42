/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ppCount.C,v 1.3 2004/07/08 17:15:29 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: prints the number of processors in the system to stdout
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <io/FileLinux.H>
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <sync/Barrier.H>
#include <stdio.h>

//#define MAX_CPUS 32

#if 0
IORef traceIORef[MAX_CPUS];
FileLinuxRef traceFileRef[MAX_CPUS];

void
childMain(VPNum vp, char *str, Barrier *bar)
{
    bar->enter();
    traceUserStr(TRACE_USER_STR, 1, 1, (uval64)vp, str);
    bar->enter();
    err_printf("Worker thread exit.\n");
}

struct LaunchChildMsg : public MPMsgMgr::MsgAsync {
    VPNum vp;
    Barrier *bar;
    char *eventStr;

    virtual void handle() {
	uval const myVP = vp;
	char *myStr = eventStr;
	Barrier *myBar = bar;
	free();
	childMain(myVP, myStr, myBar);
    }
};
#endif

int
main(int argc, char *argv[])
{
    VPNum numbVPs;
    //, vp;
    //SysStatus rc;
    //char *str;

    numbVPs = DREFGOBJ(TheProcessRef)->ppCount();
    //passert((numbVPs < MAX_CPUS), err_printf("too many vps\n"));

#if 0
    BlockBarrier bar(numbVPs);

    if (argc < 2) {
	str = "User-level DEFAULT event";
    } else {
	str = argv[1];
    }

    err_printf("Inserting event [%s] on all VPs...\n", str);

    // create vps
    for (vp = 1; vp < numbVPs; vp++) {
	rc = ProgExec::CreateVP(vp);
	passert(_SUCCESS(rc), err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
    }
    // start vps
    for (vp = 1; vp < numbVPs; vp++) {
	LaunchChildMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr()) LaunchChildMsg;
	msg->vp = vp;
	msg->bar = &bar;
	msg->eventStr = str;
	rc = msg->send(SysTypes::DSPID(0, vp));
	tassert(_SUCCESS(rc), err_printf("send failed\n"));
    }

    // I'm vp 0
    childMain(0, str, &bar);
#endif
    printf("%ld\n", numbVPs);
}
