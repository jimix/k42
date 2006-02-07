/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.29 2005/06/28 19:48:02 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <usr/ProgExec.H>
#include <scheduler/Scheduler.H>
#include "NFSExport.H"
#include "FileSystemNFS.H"
#include <sys/systemAccess.H>
#include <errno.h>

void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if ( n<=1) {
        err_printf("NFS - number of processors %ld\n", n);
        return ;
    }

    err_printf("NFS - starting secondary processors\n");
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("NFS - vp %ld created\n", vp);
    }
}

void usage(char* argv0) {
    fprintf(stderr, "Usage: %s <host> <path> <mountpoint>\n",argv0);
    exit(-EINVAL);
}


int
main(int argc, char** argv)
{
    NativeProcess();

    char* host;
    char* path;
    char* mntPoint;
    if (argc<4) {
	usage(argv[0]);
    }

    host= argv[1];
    path= argv[2];
    mntPoint = argv[3];

    err_printf("NFS - file system starting\n");

    startupSecondaryProcessors();
    FileSystemNFS::ClassInit(0); // initialize the file system

    FileSystemRef fs;
    FileSystemNFS::Create(fs, host, path, mntPoint);

    err_printf("NFS - file system started\n");

    FileSystemNFS::Block();
}
