/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.15 2005/08/11 20:20:53 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <usr/ProgExec.H>
#include <stdio.h>
#include <unistd.h>
#include <scheduler/Scheduler.H>
#include <FileSystemKFS.H>
#include <stub/StubFileSystemKFS.H>
#include <sys/ProcessSet.H>
#include <sys/systemAccess.H>

#include <fslib/fs_defines.H> /* currently defines value for default device
			       * HDRW_DISK_NAME */

static void usage(char *prog)
{
    printf("Usage: %s <mount_point> [device] [--coverable yes|no]\n"
	   "\t default value for device is %s\n",
	   "\t default is not coverable, i.e, mounts above\n"
	   "\t <mount_point> in the tree do not hide <mount_point>"
	   " (PARAMETER BEING IGNORED FOR NOW)\n"
	   "\t Examples: %s /kfs\n",
	   "\t           %s /kfs /dev/sda1\n",
	   prog, HDRW_DISK_NAME, prog);
}

void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if (n <= 1) {
        err_printf("kfs - number of processors %ld\n", n);
        return ;
    }

    if (n > 1) {
	err_printf("kfs - starting secondary processors\n");
    }
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("kfs - vp %ld created\n", vp);
    }
}

#include <cobj/sys/COSMgrObject.H>
int
main(int argc, char **argv)
{
    NativeProcess();

    pid_t childLinuxPID;
    SysStatus rc;
    char *device = HDRW_DISK_NAME;

    if (argc < 2 || argc >= 6) {
	usage(argv[0]);
	return 1;
    }
    if (argc > 2) {
	// either device or coverable option has been provided
	if (strncmp(argv[2], "--", 2) != 0) {
	    device = argv[2];
	}
    }

    // FIXME: Spin longer for all BLocks.
    extern uval FairBLockSpinCount, BitBLockSpinCount;
    FairBLockSpinCount = 100000;
    BitBLockSpinCount = 100000;

//    close(0); close(1); close(2);

    rc = ProgExec::ForkProcess(childLinuxPID);
    tassertMsg(_SUCCESS(rc), "%s: ForkProcess() failed rc 0x%lx\n", argv[0],
	       rc);

    if (childLinuxPID == 0) {
	// I'm the child

	// Set the cleanup delay to 100ms. so the token will circulate
	// reasonably.
	((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);

	err_printf("kfs file system - starting (device %s, mount point"
	    " %s)\n", argv[1], argv[2]);

	/*
	 * to improve cold perfomance and thus make benchmarks easier
	 * to run, force the process set to "adapt" to supporting a
	 * large number of pids.
	 */
	DREFGOBJ(TheProcessSetRef)->numberOfPids(1024);
	startupSecondaryProcessors();
	// FIXME: not coverable by default
	rc = FileSystemKFS:reate(device, argv[1]);
	tassertMsg(_SUCCESS(rc), "ops rc 0x%lx\n", rc);
	err_printf("kfs - file system started  (device %s, path %s)\n",
		   argv[1], argv[2]);

	// FIXME: shouldn't block forever
	FileSystemKFS::Block();
    }

    // this is for the parent: wait for file system initialization to finish

    do {
#ifdef MAMBO
	Scheduler::DelayMicrosecs(10000);
#else
 	Scheduler::DelaySecs(1);
#endif

	rc = StubFileSystemKFS::_TestAlive(argv[2], strlen(argv[2]));
	if (_FAILURE(rc)) {
	    err_printf("%s: waiting for KFS initialization\n", argv[0]);
	    //err_printf("\trc is (%ld,%ld, %ld) \n", argv[0], _SERRCD(rc),
	    //	       _SCLSCD(rc), _SGENCD(rc));
	}
    } while (_FAILURE(rc));

    return 0;
}

