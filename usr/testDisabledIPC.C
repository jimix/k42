/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testDisabledIPC.C,v 1.14 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Tests an IPC to a disabled process, temporarily
 * gross cludge where the kernel is doing the IPC.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "UsrTst.H"
#include <sys/BaseProcess.H>
#include <scheduler/Scheduler.H>
#include <stdio.h>
#include <sys/systemAccess.H>

int
main()
{
    NativeProcess();

    SysStatus rc;
    ObjectHandle oh;

    UsrTstRef ref = UsrTst::Create();

    rc = DREF(ref)->giveAccessByServer(oh, _KERNEL_PID);
    tassert(_SUCCESS(rc), err_printf("woops\n"));
    printf("testDisabledIPC: calling kernel with oh %lx %lx\n",
	   oh.commID(), oh.xhandle());
    Scheduler::Disable();
    ALLOW_PRIMITIVE_PPC();
    DREFGOBJ(TheProcessRef)->testUserIPC(oh);
    UN_ALLOW_PRIMITIVE_PPC();
    Scheduler::Enable();
    printf("testDisabledIPC: done calling kernel\n");
    for (uval i = 0; i < 20; i++) {
	printf("testDisabledIPC: sleeping (%ld)\n", i);
	Scheduler::DelayMicrosecs(5000);
    }
    printf("testDisabledIPC: terminating\n");
    return 0;
}
