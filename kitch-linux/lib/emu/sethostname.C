/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sethostname.C,v 1.5 2005/06/28 19:43:15 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: sethostname()
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <fcntl.h>
#define write __k42_linux_write
#define close __k42_linux_close
#include <unistd.h>

extern "C" int
__k42_linux_sethostname (char *name, size_t len);

/* This assumes that /kbin/sysctl has bound the /ksys filesystem.  */
int
__k42_linux_sethostname (char *name, size_t len)
{
    int ret;
    int fd = __k42_linux_open("/ksys/hostname", O_WRONLY, 0);
    if ( fd < 0 ) {
	return fd;
    }

    ret = __k42_linux_write(fd ,name, len);

    __k42_linux_close(fd);

    return 0;
}

