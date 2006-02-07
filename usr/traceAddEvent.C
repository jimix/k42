/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceAddEvent.C,v 1.12 2005/06/28 19:48:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user program that emits a user-specified string to
 * 		       the tracing buffer (for each processor of the system)
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <io/FileLinux.H>
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include <usr/ProgExec.H>
#include <stdio.h>
#include <sync/Barrier.H>
#include <sys/SystemMiscWrapper.H>
#include <sys/systemAccess.H>

#define MAX_CPUS 32

#define DO_PRINTF	printf

IORef traceIORef[MAX_CPUS];
FileLinuxRef traceFileRef[MAX_CPUS];

struct PFStat {
    PFStat() {
	//for (int i = 0; i < TraceAutoCount::MAX; i++) {
	for (int i = 0; i < 0; i++) {
	    time[i] = 0;
	    num[i] = 0;
	}
    }
    void Dump() {
	const char *countName[] = {
	    "FCMDefault::mapPage()                     ",
	    "FCMDefault::getPageInternal()             ",
	    "FCMComputation::mapPage()                 ",
	    "FCMComputation::getPageInternal()         ",
	    "FCMComputation::getPageInternal_PARENT    ",
	    "FCMComputation::getPageInternal_CHILD     ",
	    "FCMComputation::forkCopy()                ",
	    "FCMComputation::forkCollapse()            ",
	    "FCMDefaultMultiRep::mapPage()             ",
	    "FCMDefaultMultiRep::getPageInternal()     ",
	    "FCMCRW::mapPage()                         ",
	    "ExceptionLocal_PgfltHandler()             ",
	    "FCMCommon LOCK CONTENTION                 ",
	    "NFS_RPC_ACQUIRE                           ",
	    "NFS_RPC_BLOCKED                           ",
	    "NFS_RPC_OP				       ",
	    "Linux syscall			       ",
            "FCMDefault::startFillPage()               ",
            "FCMDefault::startFillPage():ZeroFill      ",
	    "FCMDefault::findOrAllocatePageAndLock     ",
	    "FCMDefault::findOrAllocatePageAndLock:allc",

	    "---ERR-THIS-IS-MAX---"
	};

	//for (int i = 0; i < TraceAutoCount::MAX; i++) {
	for (int i = 0; i < 0; i++) {
	    DO_PRINTF("%s", countName[i]);
	    DO_PRINTF("time=%lld, num=%lld\n", time[i], num[i]);
	}
    }
    //uval64 time[TraceAutoCount::MAX];
    //uval64 num[TraceAutoCount::MAX];

    uval64 time[17];
    uval64 num[17];
};

void
childMain(VPNum vp, char *str, Barrier *bar, uval turnTraceOn, PFStat *stats)
{
    bar->enter();
    if (turnTraceOn == 1) {
	(void)DREFGOBJ(TheSystemMiscRef)->traceInitCounters();
    }
    if (turnTraceOn == 0) {

    }
    TraceOSUserStr((uval64)vp, str);
    //err_printf("Worker thread exit.\n");
    bar->enter();
}

struct LaunchChildMsg : public MPMsgMgr::MsgAsync {
    VPNum vp;
    Barrier *bar;
    char *eventStr;
    uval turnTraceOn;
    PFStat *stat;

    virtual void handle() {
	uval const myVP = vp;
	char *myStr = eventStr;
	Barrier *myBar = bar;
	uval myturnTraceOn = turnTraceOn;
	PFStat *myStat = stat;
	free();
	childMain(myVP, myStr, myBar, myturnTraceOn, myStat);
    }
};

int
main(int argc, char *argv[])
{
    NativeProcess();

    VPNum numbVPs, vp;
    SysStatus rc;
    char *str;
    uval turnTraceOn = 2; // Default do nothing to the trace Mask

    numbVPs = DREFGOBJ(TheProcessRef)->ppCount();
    passert((numbVPs < MAX_CPUS), err_printf("too many vps\n"));

    BlockBarrier bar(numbVPs);

    if (argc < 2) {
	str = "User-level DEFAULT event [0|1]";
    } else {
	str = argv[1];
    }
    if (argc == 3) {
	if (argv[2][0] == '0') turnTraceOn = 0;
	if (argv[2][0] == '1') turnTraceOn = 1;
    }

    err_printf("Inserting event [%s] on all VPs...\n", str);

    // create vps
    for (vp = 1; vp < numbVPs; vp++) {
	rc = ProgExec::CreateVP(vp);
	passert(_SUCCESS(rc), err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
    }

    PFStat pfStats[numbVPs];

    // start vps
    for (vp = 1; vp < numbVPs; vp++) {
	LaunchChildMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr()) LaunchChildMsg;
	msg->vp = vp;
	msg->bar = &bar;
	msg->eventStr = str;
	msg->turnTraceOn = turnTraceOn;
	msg->stat = pfStats;
	rc = msg->send(SysTypes::DSPID(0, vp));
	tassert(_SUCCESS(rc), err_printf("send failed\n"));
    }

    // I'm vp 0
    childMain(0, str, &bar, turnTraceOn, pfStats);

    if (turnTraceOn == 0) {
	for (vp = 0; vp < numbVPs; vp++) {
	    DO_PRINTF("VP%ld:\n-----------------\n", vp);
	    pfStats[vp].Dump();
	}
    }
}
