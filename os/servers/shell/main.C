/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.20 2005/06/28 19:48:12 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include "Shell.H"
#include <sys/systemAccess.H>
#include <io/FileLinux.H>
extern "C" void _start();
extern char **environ;

Shell shell;

#define DEFAULT_PATH "/bin"

// should be automatically grabbed form stdio.h
//extern char *getenv(const char *);
//extern sval setenv(register const char *, register const char *, int);



FileLinuxRef tty;


int
main(int argc, const char *argv[], const char *envp[])
{
    NativeProcess();

    SysStatus rc;

    rc = FileLinux::Create(tty, "/dev/tty", O_RDWR, 0666);
    tassertMsg(_SUCCESS(rc), "can't open /dev/tty: %lx\n",rc);
    if (argc == 1) {
	printf("\n\n\nwelcome to k42 v0.0.2\n\n");
    }
    shell.shell(argc, argv);

    DREF(tty)->destroy();

}
