/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: printk.C,v 1.2 2004/10/19 17:58:28 butrico Exp $
 *
 * FIXME FIXME HACK ALERT: this file is a copy of os/servers/pty/printk.C.
 * We probably should move this to some place where it can be used by both
 * servers (e.g. lib/lk?)
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

