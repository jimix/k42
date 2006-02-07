/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sched_yield.C,v 1.11 2004/06/14 20:32:56 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: yield the processor voluntarily
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#define sched_yield __k42_linux_sched_yield
#include <sched.h>

/* FIXME: this yields the currently running thread to another thread
 * in the same process, not the currently running process to another
 * process.  We should look at the POSIX standard to see if this is
 * acceptable behaviour for this function. */
int sched_yield(void)
{
    SYSCALL_ENTER();
    Scheduler::Yield();
    SYSCALL_EXIT();
    return 0;
}
