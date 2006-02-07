/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: tracedStop.C,v 1.3 2005/06/28 19:42:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user program that stops the trace daemon. If the trace
 *                     daemon is not running, nothing happens.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/SystemMiscWrapper.H>
#include <sys/systemAccess.H>

int
main(int argc, char *argv[])
{
    NativeProcess();

    SysStatus rc;

    rc = DREFGOBJ(TheSystemMiscRef)->traceStopTraceDAllProcs();
    if (_FAILURE(rc)) {
	cprintf("Call traceStopTraceDAllProcs failed with rc 0x%lx\n",	rc);
    }
    return 0;
}
