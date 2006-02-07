/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: main.C,v 1.10 2005/06/28 19:48:08 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: rlogin daemon.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "LinuxPTY.H"

int
main()
{
    NativeProcess();

    err_printf("Pty/Rlogin daemon starting\n");

    // baseServers should launch this on a separate thread
    StreamServerPty::ClassInit();

    exit(0);
}

