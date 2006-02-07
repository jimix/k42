#ifndef __MOUNT_POINT_MGR_CLIENT_H_
#define __MOUNT_POINT_MGR_CLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgrClient.H,v 1.14 2004/03/01 16:03:24 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines interface for client side to register
 * and look up mount points.
 * **************************************************************************/

#include <io/PathName.H>
#include <sys/MountPointMgrCommon.H>
class MountPointMgrClient : public Obj {
    DEFINE_GLOBALPADDED_NEW(MountPointMgrClient);
    MountPointMgrCommon mpc;

    uval version;
    typedef BLock LockType;
    LockType lock;

    void init() {
	lock.init();
	version = 0;
	mpc.init();
    }

    void reloadMountInfo();

public:
    static void ClassInit(VPNum vp);

    /*
     * registers a new mount point:
     * mountPath - path we are mounting at
     * oh        - name tree (served by a file server) to be mounted
     * relPath   - pathname of mount point in server *relative* to the root
     *             of the name tree being mounted (the absolute pathname of
     *             mount point in server is already being stored in the name
     *             tree root)
     */
    virtual SysStatus registerMountPoint(const PathName *mountPath, uval lenMP,
					 ObjectHandle oh,
					 const PathName *relPath, uval lenRP,
					 const char *desc, uval lenDesc,
					 uval isCoverable = 1);

    /*
     * duplicates some piece of existing name space at another point
     * in the name space
     */
    virtual SysStatusUval bind(PathName *oldname, uval oldnamelen,
			       PathName *newname, uval newnamelen,
			       uval isCoverable);

    /*
     * get directory entries from some path,
     * for now, no cookie, i.e., everything must fit in one buffer
     * no . or .., i.e., only files really there if they exist
     */
    virtual SysStatusUval getDents(PathName *name, uval namelen,
				   char *buf, uval len);

    // on return:
    // name - holds the new path to go to server
    // namelen - is new length of path
    // unRes - points to part of name not yet resolved
    // unResLen - points to part of length not yet resolved
    // oh - is the object handle identifies server
    virtual SysStatus lookup(PathName *name, uval &namelen, uval maxlen,
			     PathName *&unRes, uval &unResLen,
			     ObjectHandle &oh, uval followLink=1);

    // on return:
    // name - holds the new path to go to server
    // namelen - is new length of path
    // oh - is the object handle identifies server
    // folowLink - indicates if should follow a link that is the last component
    //    don't follow for unlink and lstat
    virtual SysStatus lookup(PathName *name, uval &namelen, uval maxlen,
			     ObjectHandle &oh, uval followLink=1) {
	PathName *unRes;
	uval unResLen;
	return lookup(name, namelen, maxlen, unRes, unResLen, oh, followLink);
    }

    virtual void print();

    /* operations that deal with symbolic links
     * FIXME: operations that need to be added here:
     * o whenever unlink, need to check if symlink and cached
     * o some uplink to delete cache of symbolic links
     */

    // read link, look up in cache before calling server
    virtual SysStatusUval readlink(PathName *pth, uval pthlen, uval maxpthlen,
				   char *buf, uval bufsize);

    // resolve and cache any symbolic links in this path
    virtual SysStatus resolveSymbolicLink(const PathName *pth, uval pthlen);

    // print list of mounted file systems
    virtual SysStatus printMtab();

    // support to iterate over mounted file systems.
    // The 'list' parameter will return a list of object holder
    // for NameTrees currently mounted. It's responsability of the caller
    // to free the list after usage
    virtual SysStatus getNameTreeList(
	ListSimple<ObjectHandle*, AllocGlobal> *&list);
};

#endif /* #ifndef __MOUNT_POINT_MGR_CLIENT_H_ */
