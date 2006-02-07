/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.12 2005/08/11 20:20:52 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <usr/ProgExec.H>
#include "FileSystemK42RamFS.H"
#include <stdio.h>
#include <unistd.h>
#include <scheduler/Scheduler.H>
#include <stub/StubFileSystemK42RamFS.H>
#include <sys/ProcessSet.H>
#include <sys/systemAccess.H>

static void usage(char *prog)
{
    printf("Usage: %s <mount_point> [--coverable yes|no]\n"
	   "\t default is not coverable, i.e, mounts above\n"
	   "\t <mount_point> in the tree does not cover (hide) <mount_point>\n" ,
	   prog);
}

void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if (n <= 1) {
        err_printf("k42Ramfs - number of processors %ld\n", n);
        return ;
    }

    if (n > 1) {
	err_printf("k42Ramfs - starting secondary processors\n");
    }
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("k42Ramfs - vp %ld created\n", vp);
    }
}

#include <cobj/sys/COSMgrObject.H>
int
main(int argc, char **argv)
{
    NativeProcess();

    pid_t childLinuxPID;
    SysStatus rc;

    if (argc == 1) {
	usage(argv[0]);
	return 1;
    }

    // FIXME: Spin longer for all BLocks.
    extern uval FairBLockSpinCount, BitBLockSpinCount;
    FairBLockSpinCount = 100000;
    BitBLockSpinCount = 100000;

    close(0); close(1); close(2);

    rc = ProgExec::ForkProcess(childLinuxPID);
    tassertMsg(_SUCCESS(rc), "%s: ForkProcess() failed\n", argv[0]);

    if (childLinuxPID == 0) {
	// I'm the child

	// Set the cleanup delay to 100ms. so the token will circulate
	// reasonably.
	((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);

	err_printf("ramFS file system - starting (argv[1] is %s)\n", argv[1]);
	/*
	 * to improve cold perfomance and thus make benchmarks easier
	 * to run, force the process set to "adapt" to supporting a
	 * large number of pids.
	 */
	DREFGOBJ(TheProcessSetRef)->numberOfPids(1024);
	startupSecondaryProcessors();
	FileSystemK42RamFS::Create(0, argv[1], 0);
	//err_printf("ramFS - file system started on %s\n", argv[1]);

	FileSystemK42RamFS::Block();
    }

    do {
 	Scheduler::DelaySecs(1);

	rc = StubFileSystemK42RamFS::_TestAlive(argv[1], strlen(argv[1]));
	if (_FAILURE(rc)) {
	    err_printf("%s: waiting for ramfs initialization\n", argv[0]);
	    //err_printf("\trc is (%ld,%ld, %ld) \n", argv[0], _SERRCD(rc),
	    //	       _SCLSCD(rc), _SGENCD(rc));
	}
    } while (_FAILURE(rc));

    return 0;
}

