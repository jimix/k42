/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: poll.C,v 1.11 2004/06/14 20:32:55 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of poll by calling select
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <unistd.h>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#define poll __k42_linux_poll
#include <sys/poll.h>
#undef poll

int
__k42_linux_poll(struct pollfd *fds, unsigned long int nfds, int timeout)
{
    SysStatus rc;
    SYSCALL_ENTER();
    // poll() syscall uses milliseconds, FD::poll uses microseconds.
    if (timeout>0) timeout *= 1000;
    sval t = timeout;  //to clean warnings
    rc = _FD::Poll(fds, nfds, t);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return rc;

}
