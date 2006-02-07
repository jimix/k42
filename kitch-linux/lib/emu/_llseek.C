/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: _llseek.C,v 1.16 2004/06/16 19:46:42 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: seek from a file descriptor
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <unistd.h>

long
__k42_linux__llseek (int fd, unsigned long offset_high,
                     unsigned long offset_low, loff_t *result, int whence)
{
    SYSCALL_ENTER();
    uval offset = (offset_high<<32ULL)|offset_low;
    FileLinuxRef fileRef = _FD::GetFD(fd);
    FileLinux::At at;

    if (!fileRef) {
	SYSCALL_EXIT();
	return -EBADF;
    }

    // Convert Linux coding to K42
    switch (whence) {
    case SEEK_SET:
	at = FileLinux::ABSOLUTE; break;
    case SEEK_CUR:
	at = FileLinux::RELATIVE; break;
    case SEEK_END:
	at = FileLinux::APPEND; break;
    default:
	SYSCALL_EXIT();
	return -EINVAL;
    }

    SysStatusUval rc=DREF(fileRef)->setFilePosition(offset, at);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    *result = _SGETUVAL(rc);
    SYSCALL_EXIT();
    return 0;
}
