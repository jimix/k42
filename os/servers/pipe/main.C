/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.11 2005/06/28 19:48:05 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: rlogin daemon.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <sys/systemAccess.H>

#include "StreamServerPipe.H"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if (n <= 1) {
        err_printf("piped - number of processors %ld\n", n);
        return;
    }

    err_printf("piped - starting secondary processors\n");
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
                err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("piped - vp %ld created\n", vp);
    }
    return;
}

int
main()
{
    NativeProcess();

//    startupSecondaryProcessors();

    // baseServers should launch this on a separate thread
    StreamServerPipe::ClassInit();


    exit(0);
}

