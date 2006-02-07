#ifndef __KFS_STAT_H_
#define __KFS_STAT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSStat.H,v 1.2 2004/02/24 20:32:48 lbsoares Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: 
 ****************************************************************************/

#include <sys/types.H>

#ifndef KFS_TOOLS
#include <io/FileLinux.H>
#endif // #ifndef KFS_TOOLS

class KFSStat {
public:
    uval64 st_dev;		/* Device.  */
    uval64 st_ino;		/* File serial number.	*/
    uval64 st_mode;	 	/* File mode.  */
    uval64 st_nlink;		/* Link count.  */
    uval32 st_uid;		/* User ID of the file's owner.	*/
    uval32 st_gid;		/* Group ID of the file's group.*/
    uval64 st_rdev;		/* Device number, if device.  */
    uval64 st_size;		/* Size of file, in bytes.  */
    uval32 st_blksize;          /* Optimal block size for I/O.  */
    uval32 st_blocks;	        /* Number 512-byte blocks allocated. */
    uval64 atime;		/* Time of last access.  */
    uval64 mtime;		/* Time of last modification.  */
    uval64 ctime;		/* Time of last status change.  */

    uval64 getDev() { return TE64_TO_CPU(st_dev); }
    uval64 getIno() { return TE64_TO_CPU(st_ino); }
    uval64 getMode() { return TE64_TO_CPU(st_mode); }
    uval64 getNlink() { return TE64_TO_CPU(st_nlink); }
    uval32 getUid() { return TE32_TO_CPU(st_uid); }
    uval32 getGid() { return TE32_TO_CPU(st_gid); }
    uval64 getRdev() { return TE64_TO_CPU(st_rdev); }
    uval64 getSize() { return TE64_TO_CPU(st_size); }
    uval64 getBlksize() { return TE64_TO_CPU(st_blksize); }
    uval64 getBlocks() { return TE64_TO_CPU(st_blocks); }
    uval64 getAtime() { return TE64_TO_CPU(atime); }
    uval64 getMtime() { return TE64_TO_CPU(mtime); }
    uval64 getCtime() { return TE64_TO_CPU(ctime); }

    void setDev(uval64 d) { st_dev = CPU_TO_TE64(d); }
    void setIno(uval64 v) { st_ino = CPU_TO_TE64(v); }
    void setMode(uval64 v) { st_mode= CPU_TO_TE64(v); }
    void setNlink(uval64 v) { st_nlink = CPU_TO_TE64(v); }
    void setUid(uval32 v) { st_uid = CPU_TO_TE32(v); }
    void setGid(uval32 v) { st_gid = CPU_TO_TE32(v); }
    void setRdev(uval64 v) { st_rdev = CPU_TO_TE64(v); }
    void setSize(uval32 v) { st_size = CPU_TO_TE64(v); }
    void setBlksize(uval32 v) { st_blksize = CPU_TO_TE32(v); }
    void setBlocks(uval64 v) { st_blocks = CPU_TO_TE32(v); }
    void setAtime(uval64 v) { atime = CPU_TO_TE64(v); }
    void setMtime(uval64 v) { mtime = CPU_TO_TE64(v); }
    void setCtime(uval64 v) { ctime = CPU_TO_TE64(v); }

#ifndef KFS_TOOLS
    void copyTo(FileLinux::Stat *fls) {
	fls->st_dev = (dev_t) st_dev;
	fls->st_ino = (ino_t) st_ino;
	fls->st_mode = (mode_t) st_mode;
	fls->st_nlink = (nlink_t) st_nlink;
	fls->st_uid = (uid_t) st_uid;
	fls->st_gid = (gid_t) st_gid;
	fls->st_rdev = (dev_t) st_rdev;
	fls->st_size = (off_t) st_size;
	fls->st_blksize = (unsigned long) st_blksize;
	fls->st_blocks = (unsigned long) st_blocks;
	fls->st_atime = (time_t) atime;
	fls->st_mtime = (time_t) mtime;
	fls->st_ctime = (time_t) ctime;
    }
#else
    void copyTo(FileLinux::Stat *fls) {
    }    
#endif // #ifndef KFS_TOOLS
};

#if 0 // not being used anywhere! But it may be useful later
typedef enum {K42_ABSOLUTE_WHENCE, K42_RELATIVE_WHENCE,
	      K42_APPEND_WHENCE} Whence;
#endif

#endif /* #ifndef __KFS_STAT_H_ */
