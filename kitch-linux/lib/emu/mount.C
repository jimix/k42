/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mount.C,v 1.1 2005/01/28 06:20:19 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  mount/umount filesystems
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/KernelInfo.H>
#include <sys/ProcessLinux.H>
#include "linuxEmul.H"

extern "C" int __k42_linux_mount(char *dev_name, char *dir_name, char *type, 
				 unsigned long flags, void *data);

int __k42_linux_mount(char *dev_name, char *dir_name, char *, unsigned long,
		      void *)
{
    SYSCALL_ENTER();

    err_printf("NYI: mount syscall. Request to mount %s on %s ignored\n",
	       dev_name, dir_name);

    SYSCALL_EXIT();
    return 0;
}

extern "C" int __k42_linux_umount(char *user, int flags);
int __k42_linux_umount(char *user, int )
{
    SYSCALL_ENTER();

    err_printf("NYI: umount syscall. Request to unmount %s ignored\n", user);

    SYSCALL_EXIT();
    return 0;
}
