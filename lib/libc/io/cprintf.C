/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: cprintf.C,v 1.17 2004/08/20 17:30:43 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implements printing to console
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdarg.h>
#include "printfBuf.H"

void
vcprintf(const char *fmt, va_list ap)
{
    char buf[CONSOLE_BUF_MAX];
    uval len = printfBuf(fmt, ap, buf, CONSOLE_BUF_MAX);
    consoleWrite(buf, len);
}

void
cprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcprintf(fmt, ap);
    va_end(ap);
}
