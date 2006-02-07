#ifndef __MOUNT_POINT_MGR_H_
#define __MOUNT_POINT_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgr.H,v 1.14 2005/01/13 22:33:50 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to service for registering and
 * looking up mount points.
 * FIXME: we need a security model, i.e., what gives a file system the right
 * to register in some part of the name space, what happens when there is
 * something already there... for now, it is not valid to "remount" as root,
 * but for other parts of the name space we print an warning and go ahead.
 * **************************************************************************/

class MountPointMgr : public Obj {
__xprivate:
    static SysStatus _RegisterMountPoint(
	// path we are mounting at
	__inbuf(lenMP) const char *mountPath, __in uval lenMP,
	// name tree (served by a file server) to be mounted at the above path
	__in ObjectHandle oh,
	// pathname of mount point in server *relative* to the root of the
	// name tree being mounted (the absolute pathname of mount point
	// in server is already being stored in the name tree root)
	__inbuf(lenRP) const char *relPath, __in uval lenRP,
	// desc is a string provided by the file system describing the mount
	__inbuf(lenDesc) const char *desc, __in uval lenDesc,
	__in uval isCoverable);

    /* duplicates some piece of existing name space at another point
     * in the name space
     */
    static SysStatusUval _Bind(
	// existing point in the name space
	__inbuf(oldnamelen)char *oldname,
	__in uval oldnamelen,
	// new point in the name space to point to existing one
	__inbuf(newnamelen)char *newname,
	__in uval newnamelen,
	__in uval isCoverable);

    // prints on the console information about file systems currently
    // mounted (a separate list for the ones being covered by file
    // systems up in the name space tree, and therefore not reachable.
    // (File systems can be mounted so that they are never covered)
    static SysStatus _PrintMtab();

    /* Reads mount information marshalled into buffer.
     * The buffer in the server may be bigger than we can get be returned
     * on a PPC call, so this will return the part of the buffer starting
     * at position 'cur', and as much as it can fit in buf. 'left'
     * returns how much of the information is still left to be returned */
    static SysStatusUval _ReadMarshBuf(__in uval len,
				       __outbuf(__rc:len) char *buf,
				       __inout uval &cur,
				       __out uval &left);
};

#endif /* #ifndef __MOUNT_POINT_MGR_H_ */
