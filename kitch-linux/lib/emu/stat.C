/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: stat.C,v 1.19 2004/07/14 20:45:57 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: stat()  get file status Linux Kernel version
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "linuxEmul.H"

#include "stat.H"
#include <errno.h>

/*
 * convert a 64 bit glibc stat struct, produced by k42, into a linux
 * 64 bit stat structure, as used by the 64 bit linux kernel.  
 *
 *     kstat <-- gstat
 */
void _convert_gstat2kstat(struct stat *kstat, struct _gstat64 *gstat) 
{
    kstat->st_dev	= gstat->st_dev;
    kstat->st_ino	= gstat->st_ino;
    kstat->st_mode	= gstat->st_mode;
    kstat->st_nlink	= gstat->st_nlink;
    kstat->st_uid	= gstat->st_uid;
    kstat->st_gid	= gstat->st_gid;
    kstat->st_rdev	= gstat->st_rdev;
    kstat->st_size	= gstat->st_size;
    kstat->st_blksize	= gstat->st_blksize;
    kstat->st_blocks	= gstat->st_blocks;
    kstat->st_atime	= gstat->st_atim.tv_sec;
    kstat->st_mtime	= gstat->st_mtim.tv_sec;
    kstat->st_ctime	= gstat->st_ctim.tv_sec;
    kstat->__unused1	= 0;
    kstat->__unused2	= 0;
    kstat->__unused3	= 0;
    kstat->__unused4	= 0;
    kstat->__unused5	= 0;
    kstat->__unused6	= 0;

    
    #if 0
	unsigned int i;
	err_printf("struct _gstat64 (glibc version size %ld):",
		sizeof(struct _gstat64));
	for ( i = 0; i < sizeof (struct _gstat64); i += 4)
	    err_printf("%c%08x",
	    	i%(8*4)==0 ? '\n' : ' ',
	    	*(unsigned int *) ( (long)gstat + i));
	err_printf("\nstruct kstat (linux version size %ld):",
		sizeof(struct stat));
	for ( i = 0; i < sizeof (struct stat); i += 4)
	    err_printf("%c%08x",
	    	i%(8*4)==0 ? '\n' : ' ',
	    	*(unsigned int *) ( (long)kstat + i));

	err_printf("\ndev=%lu\tino=%lu\tnlink=%lu\tmode=%x\n",
		kstat->st_dev, kstat->st_ino, kstat->st_nlink, kstat->st_mode);
	err_printf("uid=%d\tgid=%d\trdev=%lu\tsize=%lu\n",
		kstat->st_uid, kstat->st_gid, kstat->st_rdev, kstat->st_size);
	err_printf("blksize=%lu\tblocks=%lu\tatime=%lu\tmtime=%lu\tctime=%lu\n",
		kstat->st_blksize, kstat->st_blocks, kstat->st_atime, 
		kstat->st_ctime, kstat->st_ctime);
    #endif
}

/*
 * convert a 64 bit glibc stat struct, produced by k42, into a linux
 * 32 bit stat structure, as used by the 64 bit linux kernel for a 32
 * bit system call.  
 *
 *     kstat <-- gstat
 */
void _convert_gstat2kstat32(struct compat_stat *kstat, struct _gstat64 *gstat)
{
    kstat->st_dev	= gstat->st_dev;
    kstat->st_ino	= gstat->st_ino;
    kstat->st_mode	= gstat->st_mode;
    kstat->st_nlink	= gstat->st_nlink;
    kstat->st_uid	= gstat->st_uid;
    kstat->st_gid	= gstat->st_gid;
    kstat->st_rdev	= gstat->st_rdev;
    kstat->st_size	= gstat->st_size;
    kstat->st_blksize	= gstat->st_blksize;
    kstat->st_blocks	= gstat->st_blocks;
    kstat->st_atime	= gstat->st_atim.tv_sec;
    kstat->st_atime_nsec= gstat->st_atim.tv_nsec;
    kstat->st_mtime	= gstat->st_mtim.tv_sec;
    kstat->st_mtime_nsec= gstat->st_mtim.tv_nsec;
    kstat->st_ctime	= gstat->st_ctim.tv_sec;
    kstat->st_ctime_nsec= gstat->st_ctim.tv_nsec;
    kstat->__unused4	= 0;
    kstat->__unused5	= 0;

    #if 0
    int verbose = 1;
    if (verbose)
    {
	unsigned int i;
	err_printf("struct _gstat64 (glibc version size=%ld):",
		sizeof(struct _gstat64));
	for ( i = 0; i < sizeof (struct _gstat64); i += 4)
	    err_printf("%c%08x",
	    	i%(8*4)==0 ? '\n' : ' ',
	    	*(unsigned int *) ( (long)gstat + i));
	err_printf("\nstruct kstat (linux 32 version size=%ld):",
		sizeof(struct compat_stat));
	for ( i = 0; i < sizeof (struct compat_stat); i += 4)
	    err_printf("%c%08x",
	    	i%(8*4)==0 ? '\n' : ' ',
	    	*(unsigned int *) ( (long)kstat + i));

	err_printf("\nsizeof ndev %ld offset %d\n",
                sizeof (kstat->st_dev),
                (int) ( (uval) &(kstat->st_dev) - (uval) kstat));
	err_printf("\nsizeof ino %ld offset %d\n",
                sizeof (kstat->st_ino),
                (int) ( (uval) &(kstat->st_ino) - (uval) kstat));
	err_printf( "\nsizeof nlink %ld offset %d\n",
                sizeof (kstat->st_nlink),
                (int) ( (uval) &(kstat->st_nlink) - (uval) kstat));
	err_printf( "\nsizeof mode %ld offset %d\n",
                sizeof (kstat->st_mode),
                (int) ( (uval) &(kstat->st_mode) - (uval) kstat));
	err_printf( "\nsizeof uid %ld offset %d\n",
                sizeof (kstat->st_uid),
                (int) ( (uval) &(kstat->st_uid) - (uval) kstat));
	err_printf( "\nsizeof gid %ld offset %d\n",
                sizeof (kstat->st_gid),
                (int) ( (uval) &(kstat->st_gid) - (uval) kstat));
	err_printf( "\nsizeof rdev %ld offset %d\n",
                sizeof (kstat->st_rdev),
                (int) ( (uval) &(kstat->st_rdev) - (uval) kstat));
	err_printf( "\nsizeof size %ld offset %d\n",
                sizeof (kstat->st_size),
                (int) ( (uval) &(kstat->st_size) - (uval) kstat));
	err_printf( "\nsizeof blksize %ld offset %d\n",
                sizeof (kstat->st_blksize),
                (int) ( (uval) &(kstat->st_blksize) - (uval) kstat));
	err_printf( "\nsizeof blocks %ld offset %d\n",
                sizeof (kstat->st_blocks),
                (int) ( (uval) &(kstat->st_blocks) - (uval) kstat));
	err_printf( "\nsizeof atime %ld offset %d\n",
                sizeof (kstat->st_atime),
                (int) ( (uval) &(kstat->st_atime) - (uval) kstat));

	err_printf("\ndev=%ud\tino=%d\tnlink=%d\tmode=%x\n",
		kstat->st_dev, kstat->st_ino, kstat->st_nlink, kstat->st_mode);
	err_printf("uid=%d\tgid=%d\tsize=%ud\n",
		kstat->st_uid, kstat->st_gid, kstat->st_size);
	err_printf("blksize=%d\tblocks=%d\tatime=%d\tmtime=%d\n",
		kstat->st_blksize, kstat->st_blocks, kstat->st_atime,
		kstat->st_mtime);
    }
    #endif
}

/*
 * convert a 64 bit glibc stat struct, produced by k42, into a linux
 * 32 bit stat structure, as used by the 64 bit linux kernel for a 32
 * bit system call.  
 *
 *     out_stat <-- gstat
 */
void _convert_gstat2lgstat64(struct stat64 *out_stat, struct _gstat64 *gstat)
{
    out_stat->st_dev	= gstat->st_dev;
    out_stat->st_ino	= gstat->st_ino;
    out_stat->st_mode	= gstat->st_mode;
    out_stat->st_nlink	= gstat->st_nlink;
    out_stat->st_uid	= gstat->st_uid;
    out_stat->st_gid	= gstat->st_gid;
    out_stat->st_rdev	= gstat->st_rdev;
    out_stat->st_size	= gstat->st_size;
    out_stat->st_blksize= gstat->st_blksize;
    out_stat->st_blocks	= gstat->st_blocks;
    out_stat->st_atime	= gstat->st_atim.tv_sec;
    out_stat->st_mtime	= gstat->st_mtim.tv_sec;
    out_stat->st_ctime	= gstat->st_ctim.tv_sec;
    out_stat->__unused1	= 0;
    out_stat->__unused2	= 0;
    out_stat->__unused3	= 0;
    out_stat->__unused4	= 0;
    out_stat->__unused5	= 0;

    #if 0
    int verbose = 1;
    if (verbose)
    {
	unsigned int i;
	err_printf("struct _gstat64 (glibc version size=%ld):",
		sizeof(struct _gstat64));
	for ( i = 0; i < sizeof (struct _gstat64); i += 4)
	    err_printf("%c%08x",
	    	i%(8*4)==0 ? '\n' : ' ',
	    	*(unsigned int *) ( (long)gstat + i));
	err_printf("\nstruct out_stat (linux stat64 version size=%ld):",
		sizeof(struct stat64));
	for ( i = 0; i < sizeof (struct stat64); i += 4)
	    err_printf("%c%08x",
	    	i%(8*4)==0 ? '\n' : ' ',
	    	*(unsigned int *) ( (long)out_stat + i));

	err_printf("\ndev=%lu\tino=%lu\tnlink=%d\tmode=%x\n",
		out_stat->st_dev, out_stat->st_ino, out_stat->st_nlink,
		out_stat->st_mode);
	err_printf("uid=%d\tgid=%d\tsize=%ld\n",
		out_stat->st_uid, out_stat->st_gid, out_stat->st_size);
	err_printf("atime=%d\tmtime=%d\tctime=%d\n",
		out_stat->st_atime, out_stat->st_mtime, out_stat->st_ctime);
    }
    #endif
}


extern int dostat(const char *filename, void *buf);
extern int dofstat(int fd, void *buf);
extern int dolstat(const char *filename, void *buf);

extern "C" int __k42_linux_stat (const char *filename, struct stat *kstat);
int
__k42_linux_stat (const char *filename, struct stat *kstat)
{
    struct _gstat64 gstat;

    int rc = dostat(filename, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2kstat(kstat, &gstat);
    return 0;
}

extern "C" int __k42_linux_fstat (int fd, struct stat *kstat);
int
__k42_linux_fstat (int fd, struct stat *kstat)
{
    //err_printf("fstat on %d(%016lx)\n", fd, (uval)kstat);
    struct _gstat64 gstat;
   
    int rc = dofstat(fd, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2kstat(kstat, &gstat);
    return 0;
}

extern "C" int
__k42_linux_lstat (const char *filename, struct stat *kstat)
{
    struct _gstat64 gstat;
   
    int rc = dolstat(filename, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2kstat(kstat, &gstat);
    return 0;
}

/* 32-bit lstat syscalls are vectored here.  */
extern "C" int
__k42_linux_compat_sys_newlstat (const char *filename, 
				 struct compat_stat *kstat)
{
    struct _gstat64 gstat;

    int rc = dolstat(filename, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2kstat32(kstat, &gstat);	// kstat <- gstat
    return 0;
}

/*
 * 32 bit within 64 bit kernel "stat"
 */
extern "C" int
__k42_linux_compat_sys_newstat (const char *filename, struct compat_stat *kstat)
{
    //err_printf("compat_sys_newstat: on %s(%016lx)\n", filename, (uval)kstat);
    struct _gstat64 gstat;

    int rc = dostat(filename, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2kstat32(kstat, &gstat);	// kstat <- gstat
    return 0;
}

/*
 * 32 bit within 64 bit kernel "fstat"
 */
extern "C" int
__k42_linux_compat_sys_newfstat (int fd, struct compat_stat *kstat)
{
    //err_printf("compat_sys_newfstat: on %d(%016lx)\n", fd, (uval)kstat);
    struct _gstat64 gstat;

    int rc = dofstat(fd, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2kstat32(kstat, &gstat);	// kstat <- gstat
    return 0;
}

/*
 * never invoked from glibc.  Probably a way to do files with large
 * offsets withint a 32 bit linux kernel.  This is invoked by the 32
 * bit glibc.
 */
extern "C" int
__k42_linux_stat64 ( char *filename, struct stat64 *output_stat)
{
    struct _gstat64 gstat;

    int rc = dostat(filename, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2lgstat64(output_stat, &gstat);
    return 0;
}

/*
 * This is _not_ a 64 bit system call.  It's a 32 bit svc that
 * supports large files
 */
extern "C" int
__k42_linux_fstat64 ( int fd, struct stat64 *output_stat)
{
    struct _gstat64 gstat;
   
    int rc = dofstat(fd, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2lgstat64(output_stat, &gstat);
    return 0;
}

/*
 * This is _not_ a 64 bit system call.  It's a 32 bit svc that
 * supports large files
 */
extern "C" int
__k42_linux_lstat64 ( char *filename, struct stat64 *output_stat)
{
    struct _gstat64 gstat;

    int rc = dolstat(filename, (void *) &gstat);
    if (rc < 0) return rc;

    _convert_gstat2lgstat64(output_stat, &gstat);
    return 0;
}
