#ifndef __STAT_H_
#define __STAT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: stat.H,v 1.1 2004/07/14 20:45:57 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#define __KERNEL__ 1
#include <linux/stat.h>
#undef __KERNEL

struct _gstat64 { /* glibc 2.3.2 64 bits stat */
    __dev_t st_dev;			// long
    __ino_t st_ino;			// long
    __nlink_t st_nlink;			// long
    __mode_t st_mode;			// int
    __uid_t st_uid;			// int
    __gid_t st_gid;			// int 
    int __pad2;				// int
    __dev_t st_rdev;			// long
    __off_t st_size;			// long
    __blksize_t st_blksize;		// long
    __blkcnt_t st_blocks;		// long
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    unsigned long int __unused4;
    unsigned long int __unused5;
    unsigned long int __unused6;
};

struct compat_stat { /* linux 32 bits stat */
	uval32		st_dev;		// int
	uval32		st_ino;		// int 
	mode_t		st_mode;	// int 
	short		st_nlink;	// short 
	uid_t 		st_uid;		// int 
	gid_t 		st_gid;		// int 
	uval32		st_rdev;	// int 
	int		st_size;	// int 
	int	  	st_blksize;	
	int	  	st_blocks;
	int	  	st_atime;
	uval32	  	st_atime_nsec;
	int	  	st_mtime;
	uval32	  	st_mtime_nsec;
	int	  	st_ctime;
	uval32	  	st_ctime_nsec;
	uval32	  	__unused4;
	uval32	  	__unused5;
};

#endif // __STAT_H_
