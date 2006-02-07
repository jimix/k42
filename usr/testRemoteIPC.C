/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testRemoteIPC.C,v 1.5 2005/08/11 20:20:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Tests IPC to a server with no local representative.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>

#include "RemoteIPCTst.H"
#include "stub/StubRemoteIPCTst.H"
#include <sys/systemAccess.H>

char *ProgName = NULL;
ThreadID ClientThreadID[Scheduler::VPLimit];

/*static*/ SysStatus
RemoteIPCTst::TestAlive()
{
    err_printf("%s server: TestAlive() called.\n", ProgName);
    return 0;
}

/*static __async*/ SysStatus
RemoteIPCTst::DisableMicrosecs(uval us)
{
    SysTime start, end;

    err_printf("%s server: disabling for %ld microseconds.\n", ProgName, us);

    start = Scheduler::SysTimeNow();
    end = start + ((us * Scheduler::TicksPerSecond()) / 1000000);

    Scheduler::Disable();
    while (Scheduler::SysTimeNow() < end) {
    }
    Scheduler::Enable();

    err_printf("%s server: re-enabled.\n", ProgName);
    return 0;
}

/*static __async*/ SysStatus
RemoteIPCTst::RefuseMicrosecs(uval us)
{
    err_printf("%s server: refusing PPCs for %ld microseconds.\n",
	       ProgName, us);

    Scheduler::DisableEntryPoint(IPC_CALL_ENTRY);
    Scheduler::DelayMicrosecs(us);
    Scheduler::EnableEntryPoint(IPC_CALL_ENTRY);

    err_printf("%s server: accepting PPCs again.\n", ProgName);
    return 0;
}

void
Server()
{
    err_printf("%s server: starting.\n", ProgName);

    // register with type server
    MetaRemoteIPCTst::init();

    err_printf("%s server: blocking.\n", ProgName);

    // block forever
    while (1) {
	Scheduler::Block();
    }
}

SysStatus
ClientThread(uval vpUval)
{
    VPNum vp = vpUval;
    SysStatus rc;

    err_printf("%s client thread %ld: running.\n", ProgName, vp);


    err_printf("%s client thread %ld: calling server.\n", ProgName, vp);
    rc = StubRemoteIPCTst::TestAlive();
    err_printf("%s client thread %ld: server returned %lx.\n",
	       ProgName, vp, rc);

    err_printf("%s client thread %ld: blocking.\n", ProgName, vp);
    ClientThreadID[vp] = Scheduler::GetCurThread();
    while (ClientThreadID[vp] != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    err_printf("%s client thread %ld: released.\n", ProgName, vp);

    err_printf("%s client thread %ld: calling server.\n", ProgName, vp);
    rc = StubRemoteIPCTst::TestAlive();
    err_printf("%s client thread %ld: server returned %lx.\n",
	       ProgName, vp, rc);

    err_printf("%s client thread %ld: blocking.\n", ProgName, vp);
    ClientThreadID[vp] = Scheduler::GetCurThread();
    while (ClientThreadID[vp] != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    err_printf("%s client thread %ld: released.\n", ProgName, vp);

    err_printf("%s client thread %ld: calling server.\n", ProgName, vp);
    rc = StubRemoteIPCTst::TestAlive();
    err_printf("%s client thread %ld: server returned %lx.\n",
	       ProgName, vp, rc);

    err_printf("%s client thread %ld: blocking.\n", ProgName, vp);
    ClientThreadID[vp] = Scheduler::GetCurThread();
    while (ClientThreadID[vp] != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    err_printf("%s client thread %ld: released.\n", ProgName, vp);

    err_printf("%s client thread %ld: exiting.\n",
	       ProgName, vp);

    return 0;
}

void
Client()
{
    SysStatus rc;
    VPNum n, vp;
    ThreadID tmp;

    err_printf("%s client: starting.\n", ProgName);

    while (_FAILURE(StubRemoteIPCTst::TestAlive())) {
	err_printf("%s client: waiting for server.\n", ProgName);
	Scheduler::DelayMicrosecs(100000);
    }

    n = _SGETUVAL(DREFGOBJ(TheProcessRef)->ppCount());

    err_printf("%s client: creating auxiliary VPs.\n", ProgName);

    for (vp = 1; vp < n; vp++) {
	rc = ProgExec::CreateVP(vp);
	passertMsg(_SUCCESS(rc), "%s client: CreateVP failed.\n", ProgName);
	err_printf("%s client: VP %ld created.\n", ProgName, vp);
    }

    err_printf("%s client: creating threads on auxiliary VPs.\n", ProgName);

    for (vp = 1; vp < n; vp++) {
	ClientThreadID[vp] = Scheduler::NullThreadID;
	rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
				     SysTypes::DSPID(0, vp),
				     ClientThread, uval(vp));
	passertMsg(_SUCCESS(rc),
		   "%s client: SendAsyncUval failed.\n", ProgName);
	err_printf("%s client: thread for VP %ld created.\n", ProgName, vp);
    }

    for (vp = 1; vp < n; vp++) {
	while (ClientThreadID[vp] == Scheduler::NullThreadID) {
	    Scheduler::DelayMicrosecs(1000);
	}
    }
    err_printf("%s client: all threads are ready.\n", ProgName);

    err_printf("%s client: asking server to disable.\n", ProgName);

    rc = StubRemoteIPCTst::DisableMicrosecs(2000000);
    passertMsg(_SUCCESS(rc),
	       "%s client: DisableMicrosecs failed.\n", ProgName);

    err_printf("%s client: sleeping.\n", ProgName);
    Scheduler::DelayMicrosecs(1000000);

    err_printf("%s client: releasing threads.\n", ProgName);
    for (vp = 1; vp < n; vp++) {
	tmp = ClientThreadID[vp];
	ClientThreadID[vp] = Scheduler::NullThreadID;
	Scheduler::Unblock(tmp);
    }

    for (vp = 1; vp < n; vp++) {
	while (ClientThreadID[vp] == Scheduler::NullThreadID) {
	    Scheduler::DelayMicrosecs(1000);
	}
    }
    err_printf("%s client: all threads are back.\n", ProgName);

    err_printf("%s client: asking server to refuse PPCs.\n", ProgName);

    rc = StubRemoteIPCTst::RefuseMicrosecs(2000000);
    passertMsg(_SUCCESS(rc),
	       "%s client: RefuseMicrosecs failed.\n", ProgName);

    err_printf("%s client: sleeping.\n", ProgName);
    Scheduler::DelayMicrosecs(1000000);

    err_printf("%s client: releasing threads.\n", ProgName);
    for (vp = 1; vp < n; vp++) {
	tmp = ClientThreadID[vp];
	ClientThreadID[vp] = Scheduler::NullThreadID;
	Scheduler::Unblock(tmp);
    }

    for (vp = 1; vp < n; vp++) {
	while (ClientThreadID[vp] == Scheduler::NullThreadID) {
	    Scheduler::DelayMicrosecs(1000);
	}
    }
    err_printf("%s client: all threads are back.\n", ProgName);

    err_printf("%s client: releasing threads.\n", ProgName);
    for (vp = 1; vp < n; vp++) {
	tmp = ClientThreadID[vp];
	ClientThreadID[vp] = Scheduler::NullThreadID;
	Scheduler::Unblock(tmp);
    }

    err_printf("%s client: sleeping.\n", ProgName);
    Scheduler::DelayMicrosecs(1000000);

    err_printf("%s client: exiting.\n", ProgName);
}

int
main(int argc, char **argv)
{
    NativeProcess();

    SysStatus rc;
    pid_t childLinuxPID;

    ProgName = argv[0];

    if (_FAILURE(StubRemoteIPCTst::TestAlive())) {

	err_printf("%s: creating server.\n", ProgName);
	rc = ProgExec::ForkProcess(childLinuxPID);
	tassertMsg(_SUCCESS(rc), "%s: ForkProcess() failed\n", ProgName);

	if (childLinuxPID == 0) {
	    // Child becomes the server.
	    Server();
	    return 0;
	}
    } else {
	err_printf("%s: server already exists.\n", ProgName);
    }

    // Parent becomes the client.
    Client();
    return 0;
}
