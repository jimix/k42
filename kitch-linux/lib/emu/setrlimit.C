/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: setrlimit.C,v 1.4 2005/08/19 12:11:09 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of setrlimit syscall
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

extern "C" int
__k42_linux_setrlimit (int resource, const struct rlimit *rlim)
{
    SYSCALL_ENTER();

    #define VERBOSE_SETRLIMIT
    #ifdef VERBOSE_SETRLIMIT
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	tassertWrn(0, "setrlimit is a stub for now\n");
    }
    #endif // VERBOSE_SETRLIMIT

    SYSCALL_EXIT();

    return 0;
}

struct rlimit_32 {
    sval32 rlim_cur;
    sval32 rlim_max;
};

extern "C" int
__k42_linux_setrlimit_32 (sval32 resource, const struct rlimit_32 *rlim)
{
    return __k42_linux_setrlimit(0, NULL);
}
