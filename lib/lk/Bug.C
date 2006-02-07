/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Bug.C,v 1.1 2004/02/27 17:14:33 mostrows Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
extern "C" {
#include <linux/kernel.h>
}
extern "C" void panic(const char*, ...);

void
panic(const char*, ...)
{
    breakpoint();
}

void
lkBreakpoint(int line, const char* file, const char* fn)
{
    printk("Bug in linux-kernel code: %s at %s:%d\n", fn, file, line);
    breakpoint();
}

void
lkWarning(int line, const char* file, const char* fn)
{
    printk("Warning in linux-kernel code: %s at %s:%d\n", fn, file, line);
}

extern "C" int seq_printf(struct seq_file *m, const char *f, ...);
int seq_printf(struct seq_file *m, const char *fmt, ...)
{
    va_list vap;
    va_start(vap,fmt);
    verr_printf(fmt,vap);
    va_end(vap);
    return 0;
}

extern "C" void udbg_ppcdbg(unsigned long debug_flags, const char *fmt, ...);
void
udbg_ppcdbg(unsigned long debug_flags, const char *fmt, ...)
{
    va_list vap;
    va_start(vap,fmt);
    verr_printf(fmt,vap);
    va_end(vap);
}

extern "C" unsigned long udbg_ifdebug(unsigned long flags);

unsigned long
udbg_ifdebug(unsigned long flags)
{
    return 0;
}

extern "C" void
udbg_console_write(struct console *con, const char *s, unsigned int n);

void
udbg_console_write(struct console *con, const char *s, unsigned int n)
{
    consoleWrite(s,n);
}

