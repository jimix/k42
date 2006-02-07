/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StdIOKern.C,v 1.4 2004/09/30 11:21:54 cyeoh Exp $
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

#if 0
void udbg_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // remove markup identifying debug level
    if (fmt[0]=='<' && fmt[2]=='>' && fmt[1]>='0' && fmt[1]<='9') {
	fmt+=3;
    }

    verr_printf(fmt, ap);
    va_end(ap);
}
#endif

int
printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // remove markup identifying debug level
    if (fmt[0]=='<' && fmt[2]=='>' && fmt[1]>='0' && fmt[1]<='9') {
	fmt+=3;
    }

    verr_printf(fmt, ap);
    va_end(ap);
    return 0;
}

