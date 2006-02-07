#ifndef __MOUNT_POINT_MGR_IMP_H_
#define __MOUNT_POINT_MGR_IMP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgrImp.H,v 1.20 2005/01/13 22:33:50 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to service for registering and
 * looking up mount points.
 *
 * A single component of a mount point
 * For now, am not bothering making a clustered object, eventually, since
 * these are read-mostly, we probably want a distributed implementation
 * **************************************************************************/

#include "MountPointMgr.H"
#include <sys/MountPointMgrCommon.H>
#include <misc/ListSimple.H>
#include <sys/KernelInfo.H>

class MountPointMgrImp : public MountPointMgr {
    friend class MountPointMgr;
    // internal state MountPointMgrImp
    typedef BLock LockType;
    LockType   lock;

    MountPointMgrCommon mountPointCommon;
    MountPointMgrCommon::MarshBuf       marshBuf;

    // just a single instance of this class
    static MountPointMgrImp *obj;
    DEFINE_GLOBALPADDED_NEW(MountPointMgrImp);

    void remarshalIntoBuffer();

    virtual SysStatus registerMountPoint(const PathName *mountPath, uval lenMP,
					 ObjectHandle oh,
					 const PathName *relPath, uval lenRP,
					 const char *desc, uval lenDesc,
					 uval isCoverable) {
	SysStatus rc;
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	rc = mountPointCommon.registerMountPoint(mountPath, lenMP, oh,
						 relPath, lenRP,
						 desc, lenDesc,
						 isCoverable);
	remarshalIntoBuffer();
	return rc;
    }

    virtual SysStatus bind(PathName *oldPath, uval oldLen,
			   const PathName *newPath, uval newLen,
			   uval isCoverable) {
	SysStatus rc;
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	rc = mountPointCommon.bind(oldPath, oldLen, newPath, newLen,
				   isCoverable);
	remarshalIntoBuffer();
	return rc;
    }

    virtual SysStatusUval readMarshBuf(uval len, char *buf,
				       uval &cur, uval &left);

    virtual SysStatus printMtab() {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return mountPointCommon.printMtab();
    }

    void init();

public:
    static void ClassInit();
    /*
     * duplicates some piece of existing name space at another point
     * in the name space (e.g., if /nfs is mounted, we can duplicate
     * /nfs/tmp as /tmp by invoking Bind("nfs/tmp", "/tmp")
     */
    static void Bind(char *oldname, char *newname, uval isCoverable);
};

#endif /* #ifndef __MOUNT_POINT_MGR_IMP_H_ */
