/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ftruncate.C,v 1.7 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: truncate  a  file  to a specified length
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
int
__k42_linux_ftruncate(int fd, off_t length)
{
    SYSCALL_ENTER();

    SysStatus rc;

    FileLinuxRef fileRef = _FD::GetFD(fd);
    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    /* we have verified during initializetion that these two types
     * have the same mapping*/

    rc = ((FileLinux*)(DREF(fileRef)))->ftruncate(length);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return 0;
}
