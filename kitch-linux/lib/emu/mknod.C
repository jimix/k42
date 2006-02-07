/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mknod.C,v 1.13 2005/05/06 15:28:07 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: mknod - create a directory or special or ordinary file
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/FileLinux.H>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int
__k42_linux_mknod(const char *pathname, uval mode, uval dev)
{
    SysStatus rc;

    #define VERBOSE_MKNOD
    #ifdef VERBOSE_MKNOD
    err_printf("%s: pathname %s  mode=0x%lx  dev=0x%lx\n",
	__PRETTY_FUNCTION__,
	pathname, mode, dev);
    #endif	// VERBOSE_MKNOD

    rc = FileLinux::Mknod(pathname, mode, dev);
    if (_FAILURE(rc)) {
        #ifdef VERBOSE_MKNOD
	_SERROR_EPRINT(rc);
        #endif	// VERBOSE_MKNOD
	return -_SGENCD(rc);
    }
    return 0;
}
