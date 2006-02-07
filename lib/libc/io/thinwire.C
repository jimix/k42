/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: thinwire.C,v 1.40 2005/06/28 19:44:25 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * FIXME: get rid of tihs!!!!
 * Module Description: server implementation of thin-wire services
 * uses Wire object from kernel to communicate
 * over a multiplexed thin-wire connection
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/thinwire.H>
#include <sys/ProcessLinuxClient.H>
#include "stub/StubWire.H"
#include <string.h>

// FIXME: put this here temporarily, since need a a file
// that is not in the kernel

#include <scheduler/Scheduler.H>

void
verr_printf(const char *fmt, va_list ap)
{
    uval isDisabled;
    isDisabled = Scheduler::IsDisabled();
    if (!isDisabled) {
	Scheduler::Disable();
    }

    ALLOW_PRIMITIVE_PPC();
    vcprintf(fmt, ap);
    UN_ALLOW_PRIMITIVE_PPC();

    if (!isDisabled) {
	Scheduler::Enable();
    }
}

sval
err_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    verr_printf(fmt, ap);
    va_end(ap);

    return 0;
}
