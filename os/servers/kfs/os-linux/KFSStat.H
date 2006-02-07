#ifndef __K42_STAT_H_
#define __K42_STAT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KFSStat.H,v 1.7 2004/02/07 03:51:38 lbsoares Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: format for retrieving stat information from a 
 * Linux file system
 ****************************************************************************/

#include <io/FileLinux.H>
#include <Byteorder.H>

#ifdef KFS_TOOLS
#define _LARGEFILE64_SOURCE
#include <sys/stat.h>
#else
#include <linux/stat.h>
#endif

typedef struct KFSStat : stat64 {
public:
    void copyTo(FileLinux::Stat *fls) {
        *fls = *this;
    }
#define ctime st_ctime
#define atime st_atime
#define mtime st_mtime

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
};

#endif /* #ifndef __K42_STAT_H_ */
