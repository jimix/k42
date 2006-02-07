/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ptrace.C,v 1.3 2004/07/01 21:14:21 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: ptrace()
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

//FIXME HUGE HACK!!
#include "../../../lib/libc/usr/GDBIO.H"

#define ptrace __k42_linux_ptrace
#include <sys/ptrace.h>
#include <errno.h>

long int
ptrace(enum __ptrace_request request, ...)
{
#undef ptrace
    SysStatus rc = 0;

    switch (request) {
    case PTRACE_TRACEME:
	// debug the system default way
	(void) GDBIO::SetDebugMe(GDBIO::DEBUG);
	break;
    case PTRACE_ATTACH:
	(void) GDBIO::SetDebugMe(GDBIO::USER_DEBUG);
	break;
    case PTRACE_DETACH:
	(void) GDBIO::SetDebugMe(GDBIO::NO_DEBUG);
	break;
    default:
	rc = (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, ENOSYS));
	break;
    }
    return rc;
}
