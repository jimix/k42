/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: get file system statistics
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#include <sys/statfs.h>

// there is both a statfs function and struct so we must mangle this way
extern "C" int __k42_linux_statfs(const char *, struct statfs *);

int
__k42_linux_statfs(const char *path, struct statfs *buf)
{

    SYSCALL_ENTER();

    SysStatus rc = FileLinux::Statfs(path, buf);

#ifndef DEVFS_SUPER_MAGIC
#define DEVFS_SUPER_MAGIC     0x1373
#endif

    if (_FAILURE(rc)) {
	/* Fake Devfs */
	if (strncmp("/dev/", path, 5)==0) {
	    buf->f_type = DEVFS_SUPER_MAGIC;
	    rc = 0;
	}
    }


    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}

/* Define a 32-bit compatibility structure.  */
typedef struct { sval32 __val[2]; } __fsid_t32;

/* Define a 32-bit compatibility structure.  */
struct statfs32 {
    sval32 f_type;
    sval32 f_bsize;
    uval32 f_blocks;
    uval32 f_bfree;
    uval32 f_bavail;
    uval32 f_files;
    uval32 f_ffree;
    __fsid_t32 f_fsid;
    sval32 f_namelen;
    sval32 f_frsize;
    sval32 f_spare[5];
};

/* The 32-bit syscall vector points here for the statfs syscall.  Here we
 * pass a local 64-bit structure to the real statfs syscall, then copy the
 * data into the pointer provided by 32-bit userspace.
 */
extern "C" int
__k42_linux_statfs_32(const char *path, struct statfs32 *buf) 
{
    int ret;
    struct statfs buf64;

    /* Fire off the real syscall with 64-bit arguments.  */
    ret = __k42_linux_statfs(path, &buf64);

    /* If real syscall succeeded, copy 64-bit data to our 32-bit data.
     * To be perfectly correct, we should check for overflows, but we
     * will not, since statfs64 should be called on relevant filesystems.
     */
    if (ret == 0) {
	buf->f_type = buf64.f_type;
	buf->f_bsize = buf64.f_bsize;
	buf->f_blocks = buf64.f_blocks;
	buf->f_bfree = buf64.f_bfree;
	buf->f_bavail = buf64.f_bavail;
	buf->f_files = buf64.f_files;
	buf->f_ffree = buf64.f_ffree;
	buf->f_fsid.__val[0] = buf64.f_fsid.__val[0];
	buf->f_fsid.__val[1] = buf64.f_fsid.__val[1];
	buf->f_namelen = buf64.f_namelen;
	buf->f_frsize = buf64.f_frsize;
	buf->f_spare[0] = buf64.f_spare[0];
	buf->f_spare[1] = buf64.f_spare[1];
	buf->f_spare[2] = buf64.f_spare[2];
	buf->f_spare[3] = buf64.f_spare[3];
	buf->f_spare[4] = buf64.f_spare[4];
    }

    return ret;
}
