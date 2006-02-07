/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: alarm.C,v 1.1 2005/06/07 00:45:16 awaterl Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: generate a SIGALRM signal after the specified time
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"

extern "C" unsigned int __k42_linux_alarm(unsigned int seconds);

unsigned int
__k42_linux_alarm(unsigned int seconds)
{
    SysStatus rc;

    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->alarm(0, seconds);
    SYSCALL_EXIT();

    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }

    return rc;
}
