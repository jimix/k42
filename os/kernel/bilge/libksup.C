/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: libksup.C,v 1.32 2005/02/09 18:45:42 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: support routines needed by libk.
 * **************************************************************************/

#include "kernIncs.H"
#include "sys/thinwire.H"
#include "bilge/libksup.H"
#include "exception/ExceptionLocal.H"
#include "defines/inout.H"
#include <misc/hardware.H>
#include <sys/hcall.h>

#include "LocalConsole.H"

/*
 * machine specific file should define:
 * void baseAbort(void)
 * void breakpoint(void)
 */

#include __MINC(libksup.C)

extern sval
printfBuf(const char *fmt0, va_list argp, char *buf, sval buflen);


void
verr_printf(const char *fmt, va_list ap)
{
    char buf[256];
    uval len = printfBuf(fmt, ap, buf, sizeof(buf));

    if (SysConsole) {
    SysConsole->raw->write(buf, len, 1);
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

void
init_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verr_printf(fmt, ap);
    va_end(ap);

}

void EnterDebugger()
{
    ExceptionLocal::EnterDebugger();
}

void ExitDebugger()
{
    ExceptionLocal::ExitDebugger();
}

uval InDebugger()
{
    return ExceptionLocal::InDebugger();
}
