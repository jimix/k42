/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: truncate.C,v 1.14 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: truncate a file to a specified length
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define truncate __k42_linux_truncate
#include <unistd.h>
int
truncate(const char *path, off_t length)
{
    SYSCALL_ENTER();
    //FIXME:
    // truncate(2) set's size to _at most_ length bytes in size.
    // Don't know what the ramifications of this with the IOMapped stuff is
    SysStatus rc = FileLinux::Truncate(path, length);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}
