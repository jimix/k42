/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sample.C,v 1.9 2005/06/28 19:48:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Tests an IPC to a disabled process, temporarily
 * gross cludge where the kernel is doing the IPC.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SampleServiceServer.H"
#include <usr/ProgExec.H>
#include <scheduler/Scheduler.H>
#include <stdio.h>
#include <sys/systemAccess.H>

int
main()
{
    NativeProcess();

    SysStatus rc;
    VPNum n, vp;

    printf("testServer:  creating SampleService\n");
    SampleServiceServer::ClassInit();

    n = DREFGOBJ(TheProcessRef)->ppCount();

    printf("testServer:  creating vps\n");
    for (vp = 1; vp < n; vp++) {
	rc = ProgExec::CreateVP(vp);
	passert(_SUCCESS(rc), err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
	printf("testServer:  vp %ld created\n", vp);
    }

    printf("testServer: blocking\n");
    SampleServiceServer::Block();
    printf("testServer: terminating\n");
    return 0;
}
