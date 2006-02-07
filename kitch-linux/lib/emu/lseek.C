/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lseek.C,v 1.4 2005/08/13 03:24:10 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: seek from a file descriptor
 *
 * FIXME: glibc should be modified to redirect this function there not here
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <unistd.h>

off_t
__k42_linux_lseek(int fd, off_t offset, int whence)
{
    loff_t result;
    int ret = __k42_linux__llseek(fd, 0, offset, &result,whence);
    if (ret<0) {
	return ret;
    }
    return (off_t)result;
}




