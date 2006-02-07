/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: chown.C,v 1.14 2005/05/12 20:30:55 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  change ownership of a file, follows sym links.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define chown __k42_linux_chown
#include <sys/types.h>
#include <unistd.h>

int
chown(const char *path, uid_t owner, gid_t group)
{
#undef chown
    SysStatus rc;

    SYSCALL_ENTER();

    if (((uid_t)-1 == owner) || ((gid_t)-1 == group)) {
        struct stat mystat;
        rc = FileLinux::GetStatus(path,
                         FileLinux::Stat::FromStruc(&mystat));
	if (_FAILURE(rc)) {
	    SYSCALL_EXIT();
	    return -_SGENCD(rc);
	}
	if ((uid_t)-1 == owner ) 
	    owner = mystat.st_uid;
	if ((gid_t)-1 == group ) 
	    group = mystat.st_gid;
    }

    rc = FileLinux::Chown(path, owner, group);
    SYSCALL_EXIT();

    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    } else {
	return (rc);
    }
}
