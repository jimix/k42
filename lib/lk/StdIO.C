/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StdIO.C,v 1.2 2004/10/19 17:52:49 butrico Exp $
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
#include <sys/TAssert.H>

#include <stdio.h>

extern "C" void udbg_printf(const char *fmt, ...);
extern "C" int printk(const char *fmt, ...);

void udbg_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

int
printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return 0;
}

