/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Ext2Conversion.C,v 1.5 2004/09/30 03:07:48 apw Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <sys/types.h>
#include <emu/stat.H>
#include "Ext2Conversion.H"

#define __KERNEL__ 1
#include <linux/dirent.h>
#undef __KERNEL__

struct _gdirent64 { /* glibc 2.3.2 64 bits dirent */
    __ino64_t d_ino;
    __off64_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];		/* We must not include limits.h! */
};

// stat <-- gstat
static void
_convert_stat2gstat(struct _gstat64* gstat, struct stat *kstat)
{
    gstat->st_dev             = kstat->st_dev;
    gstat->st_ino             = kstat->st_ino;
    gstat->st_nlink           = kstat->st_nlink;
    gstat->st_mode            = kstat->st_mode;
    gstat->st_uid             = kstat->st_uid;
    gstat->st_gid             = kstat->st_gid;
    gstat->st_rdev            = kstat->st_rdev;
    gstat->st_size            = kstat->st_size;
    gstat->st_blksize         = kstat->st_blksize;
    gstat->st_blocks          = kstat->st_blocks;
    gstat->st_atim.tv_sec     = kstat->st_atime;
    gstat->st_mtim.tv_sec     = kstat->st_mtime;
    gstat->st_ctim.tv_sec     = kstat->st_ctime;
    gstat->__unused4          = 0;
    gstat->__unused5          = 0;
    gstat->__unused6          = 0;
}

// gdir <-- kdir
/*static*/ void
_convert_kdir2gdir(struct _gdirent64 *gdir, struct linux_dirent64* kdir)
{
    err_printf("Got the following in kdir\n"
	       "\td_ino %lld\n"
	       "\td_off %lld\n"
	       "\td_reclen %d\n"
	       "\td_type %d\n",
	       (uval64)kdir->d_ino, (uval64) kdir->d_off,
	       kdir->d_reclen, (int) kdir->d_type);

    tassertMsg(sizeof(gdir->d_ino) == sizeof(kdir->d_ino)
	       && sizeof(gdir->d_off) == sizeof(kdir->d_off)
	       && sizeof(gdir->d_reclen) == sizeof(kdir->d_reclen)
	       && sizeof(gdir->d_type) == sizeof(kdir->d_type),
	       "look at this\n");

    //gdir->d_ino = kdir->d_ino;
    //gdir->d_off = kdir->d_off;
    //gdir->d_reclen = kdir->d_reclen;
    //gdir->d_type = kdir->d_type;

    tassertMsg(kdir->d_reclen <= sizeof(_gdirent64), "ops");
    memcpy(gdir, kdir, kdir->d_reclen);

    err_printf("Got the following in gdir\n"
	       "\td_ino %lld\n"
	       "\td_off %lld\n"
	       "\td_reclen %d\n"
	       "\td_type %d\n"
	       "\td_name %s\n",
	       (uval64)gdir->d_ino, (uval64) gdir->d_off,
	       gdir->d_reclen, (int) gdir->d_type, gdir->d_name);
}

SysStatus
callStat(void *mnt, void *dentry, void *gstat)
{
    struct stat kstat;
    int ret = k42_get_stat(mnt, dentry, &kstat);

    if (ret < 0) {
	return _SERROR(2816, 0, -ret);
    }
    _convert_stat2gstat((struct _gstat64*) gstat, &kstat);
    return 0;
}

SysStatusUval
callGetDents(void *dentry, uval &cookie, void *dirbuf, uval len)
{
    struct linux_dirent64 *kdir = (struct linux_dirent64*)dirbuf;
    struct _gdirent64 *gdir = (struct _gdirent64*)dirbuf; 
    tassertMsg(sizeof(gdir->d_ino) == sizeof(kdir->d_ino)
	       && sizeof(gdir->d_off) == sizeof(kdir->d_off)
	       && sizeof(gdir->d_reclen) == sizeof(kdir->d_reclen)
	       && sizeof(gdir->d_type) == sizeof(kdir->d_type),
	       "look at this\n");

    int ret = k42_getdents(dentry, (long long*) &cookie, kdir,
			   (unsigned int) len);

    if (ret < 0) {
	return _SERROR(2817, 0, -ret);
    }
    //_convert_kdir2gdir((struct _gdirent64*) dirbuf, &kdir);
    return _SRETUVAL(ret);
}
