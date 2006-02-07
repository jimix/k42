#ifndef __FS_DISK_ENV_VAR_H
#define __FS_DISK_ENV_VAR_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskMountInfoList.H,v 1.1 2004/10/27 19:21:58 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Temporary code to get information about disks on a victim
 * **************************************************************************/

/* FIXME: we have to get rid of this ugly way of learning which disks/file
 * system to mount!! */

#include <misc/ListSimpleLocked.H>

class DiskMountInfo {
    char *dev;
    char *mnt;
    char *fstype;
    char *flags;
public:
    DEFINE_GLOBAL_NEW(DiskMountInfo);
    DiskMountInfo() : dev(NULL), mnt(NULL), fstype(NULL), flags(NULL) {};
    SysStatus init(char *d, char *m, char *t, char *fl);
    char *getDev() { return dev; }
    char *getFSType() { return fstype; }
    char *getMountPoint() {return mnt; }
    char *getFlagSpec() { return flags; }
};

class DiskMountInfoList {
    ListSimpleLocked<DiskMountInfo*, AllocGlobal> list;
    static SysStatus GetMountInfo(char *&buf, char* minfo[4]);
public:
    DEFINE_GLOBAL_NEW(DiskMountInfoList);
    DiskMountInfoList() {}
    SysStatus init();
    // return NULL if there is no next element, if curr is NULL, returns head
    void *next(void *curr, DiskMountInfo* &elem) {
	return list.next(curr, elem);
    }
};


#endif // __FS_DISK_ENV_VAR_H
