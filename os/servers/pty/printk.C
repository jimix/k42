/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: printk.C,v 1.4 2004/09/30 11:21:55 cyeoh Exp $
 *****************************************************************************/


#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
extern "C" {
//Don't need kernel features
#undef __KERNEL__
#include <linux/ctype.h>

#include <netinet/in.h>
}

#include <stdio.h>

extern "C" void udbg_printf(const char *fmt, ...);
extern "C" int printk(const char *fmt, ...);

void udbg_printf(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    ProcessID pid = DREFGOBJ(TheProcessRef)->getPID();
    tassertMsg(strlen(fmt)<250, "print string is too long\n");
    va_start(ap, fmt);
    snprintf(buf, 256, " %ld: %s", pid, fmt);
    verr_printf(buf, ap);
    va_end(ap);
}

int
printk(const char *fmt, ...)
{
    va_list ap;
    char buf[256];
    ProcessID pid = DREFGOBJ(TheProcessRef)->getPID();
    va_start(ap, fmt);
    snprintf(buf, 256, " %ld: %s", pid, fmt);
    verr_printf(buf, ap);
    va_end(ap);
    return 0;
}

