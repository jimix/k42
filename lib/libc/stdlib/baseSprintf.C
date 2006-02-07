/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: baseSprintf.C,v 1.6 2004/08/20 17:30:43 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "io/printfBuf.H"

extern "C" int sprintf(char *buf, const char *fmt, ...) __attribute__((weak));

int
sprintf(char *buf, const char *fmt, ...)
{
    sval len;
    va_list vap;

    va_start(vap, fmt);
    // sigh, no idea of length of buffer
    len = printfBuf(fmt, vap, buf, 4096);
    va_end(vap);
    return len;

}

sval
vsprintf(char *str, const char *fmt, va_list ap)
{
    sval len;
    len = printfBuf(fmt, ap, str, 32767);
    str[len] = 0;
    return (len);
}

sval
baseSprintf(char *str, const char *fmt, ...)
{
    sval len;
    va_list ap;

    va_start(ap, fmt);

    // sigh, no idea of length of buffer
    len = printfBuf(fmt, ap, str, 4096);
    va_end(ap);
    str[len] = 0;
    return (len);
}
