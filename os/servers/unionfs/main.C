/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.5 2005/08/11 20:20:57 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <usr/ProgExec.H>
#include "FileSystemUnion.H"
#include <stdio.h>
#include <unistd.h>
#include <scheduler/Scheduler.H>
#include <stub/StubFileSystemK42RamFS.H>
#include <sys/ProcessSet.H>
#include <sys/systemAccess.H>

static void usage(char *prog)
{
    printf("Usage: %s <primPath> <secPath> <mountPointPath> "
	   "[--coverable yes|no]\n"
	   "\t default is not coverable, i.e, mounts above\n"
	   "\t <mount_point> in the tree does not cover (hide) <mount_point>\n"
	   "\t Example: %s /ram /nfs /union\n", prog, prog);
}

void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if (n <= 1) {
        err_printf("union fs - number of processors %ld\n", n);
        return ;
    }

    if (n > 1) {
	err_printf("union fs - starting secondary processors\n");
    }
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("union fs - vp %ld created\n", vp);
    }
}

#include <cobj/sys/COSMgrObject.H>
int
main(int argc, char **argv)
{
    NativeProcess();

    pid_t childLinuxPID;
    SysStatus rc;
    char *primPath, *secPath, *mPath;
    uval isCoverable = 0;

    if (argc != 4 && argc != 5) {
	usage(argv[0]);
	return 1;
    } else {
	primPath = argv[1];
	secPath = argv[2];
	mPath = argv[3];
	// FIXME: get coverable argument, for now always making it not coverable
    }

    close(0); close(1); close(2);

    rc = ProgExec::ForkProcess(childLinuxPID);
    tassertMsg(_SUCCESS(rc), "%s: ForkProcess() failed\n", argv[0]);

    if (childLinuxPID == 0) {
	// I'm the child

	err_printf("union file system - starting with primPath %s, "
		   "secPath %s, mountPath %s\n", primPath, secPath, mPath);

	startupSecondaryProcessors();

	FileSystemUnion::ClassInit(0, primPath, secPath, mPath, isCoverable);

	FileSystemUnion::Block();
    }

    return 0;
}

