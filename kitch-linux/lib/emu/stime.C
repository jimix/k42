/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: stime.C,v 1.3 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current time
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

#define stime __k42_linux_stime
#define settimeofday __k42_linux_settimeofday
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

int
stime(const time_t* t)
{
#undef stime
    if (t == NULL) {
	// Perhaps we should check for all bad addresses?
	return -EFAULT;
    }

    struct timeval tv;
    tv.tv_sec = *t;
    tv.tv_usec = 0;
    return settimeofday(&tv, 0);
}

