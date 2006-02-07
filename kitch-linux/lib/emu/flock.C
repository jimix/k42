/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: flock.C,v 1.5 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: manipulate file descriptor
 * **************************************************************************/

// Needed in order to support the superset of #defs
#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include "FD.H"
#include <unistd.h>
#include <fcntl.h>

#define flock __k42_linux_flock
#include <sys/file.h>

/*
 * Whole file locks can be implemented using fcntl() locks.  This may
 * be temporary since remote file system may require special
 * attention to flock.
 */

int
flock(int fd, int operation)
{
#undef flock
    int cmd;
    struct flock fLock;
    fLock.l_whence =	SEEK_SET;
    fLock.l_start =	0;
    fLock.l_len =	0; // whole file
    fLock.l_pid =	0; // ignored

    switch (operation & ~LOCK_NB) {
    case LOCK_SH:
      fLock.l_type = F_RDLCK;
      break;
    case LOCK_EX:
      fLock.l_type = F_WRLCK;
      break;
    case LOCK_UN:
      fLock.l_type = F_UNLCK;
      break;
    default:
      return -EINVAL;
      break;
    }

    if (operation & LOCK_NB) {
	cmd = F_SETLK;
    } else {
	cmd = F_SETLKW;
    }

    return __k42_linux_fcntl(fd, cmd, &fLock);
}
