/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.7 2005/06/28 19:48:07 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "FileSystemProc.H"
#include <sys/systemAccess.H>
#include <io/FileLinux.H>
#include <usr/ProgExec.H>
extern "C" void _start();
extern char **environ;


#define PROCFS_AS_BACKGROUP_PROCESS

void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if (n <= 1) {
        err_printf("procFS - number of processors %ld\n", n);
        return ;
    }

    if (n > 1) {
	err_printf("procFS - starting secondary processors\n");
    }
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("ProcFS - vp %ld created\n", vp);
    }
}

int
main(int argc, const char *argv[], const char *envp[])
{
    NativeProcess();

    SysStatus rc;

    #ifdef PROCFS_AS_BACKGROUP_PROCESS
    close(0); close(1); close(2);

    setpgid(0,0);
    pid_t pid = fork();
    if (pid) {
	struct stat buf;
        while (stat("/proc/sys/kernel/version",&buf)!=0) {
	    usleep(10000);
	}

	exit(0);
    }

    startupSecondaryProcessors();
    rc = FileSystemProc::ClassInit(0);
    passertRC(rc, "can't create /proc\n");

    // deactivate and block this child's thread forever
    Scheduler::DeactivateSelf();
    while (1) {
        Scheduler::Block();
    }
    return 0;

    #else

    char c;
    rc = FileSystemProc::ClassInit(0);
    passertRC(rc, "can't create /proc\n");
    fprintf(stderr, "waiting for a character\n");
    c = getchar();
    fprintf(stderr, "proc filesystem exiting\n");
    #endif
}
