#ifndef __NAME_TREE_LINUX_FS_H_
#define __NAME_TREE_LINUX_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinuxFS.H,v 1.35 2005/04/27 17:35:07 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Entry for name lookups.
 ****************************************************************************/

#include <io/NameTreeLinux.H>

class DirLinuxFS;
class NameTreeLinuxFS;
typedef DirLinuxFS      **DirLinuxFSRef;
typedef NameTreeLinuxFS **NameTreeLinuxFSRef;

// FIXME: where should this go?
extern NameTreeLinuxFSRef TempNameTreeRef;

class NameTreeLinuxFS : public NameTreeLinux {
protected:
    ObjectHandle oh;
    DirLinuxFSRef rootDirLinuxRef;

    DEFINE_GLOBAL_NEW(NameTreeLinuxFS);
    // exported routines
    virtual SysStatus _getObj(__inbuf(namelen)char *name,
			      __in uval namelen,
			      __in uval oflag, __in uval mode,
			      __out ObjectHandle &oh,
			      __out TypeID &type,
			      __out uval &useType,
			      /* argument for simplifying gathering traces of
			       * file sharing information. This should go
			       * away. */
			      __out ObjRef &fref,
			      __CALLER_PID processID);

    virtual SysStatus _mknod(__inbuf(namelen)char *name,
                             __in uval namelen,
                             __in uval mode,
                             __in uval dev,
			     __CALLER_PID processID);

    virtual SysStatus _bind(__inbuf(namelen)char *name,
                            __in uval namelen,
                            __in uval mode,
			    __in ObjectHandle serverSocketOH,
			    __CALLER_PID processID);

    virtual SysStatus _getSocketObj(__inbuf(namelen)char *name,
                                    __in uval namelen,
			            __out ObjectHandle &serverSocketOH,
			            __CALLER_PID processID);

    virtual SysStatus _chown(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval uid, __in uval gid,
			     __CALLER_PID processID);

    virtual SysStatus _chmod(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID);

    virtual SysStatus _truncate(__inbuf(namelen) char *name,
				__in uval namelen,
				__in uval length,
				__CALLER_PID processID);

    virtual SysStatus _mkdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID);

    virtual SysStatus _rmdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID);

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in const struct utimbuf& utbuf,
			     __CALLER_PID processID);

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID);

    virtual SysStatus _getStatus(__inbuf(namelen) char *name,
				 __in uval namelen,
				 __out struct stat& status,
				 __in uval followLink,
				 __CALLER_PID processID);

    virtual SysStatus _link(__inoutbuf(oldlen) char *name,
			    __in uval namelen,
			    __inbuf(newlen) char *newname,
			    __in uval newlen,
			    __CALLER_PID processID);

    virtual SysStatus _unlink(__inbuf(namelen) char *name,
			      __in uval namelen,
			      __CALLER_PID processID);

    virtual SysStatus _rename(__inbuf(oldlen) char *name,
			      __in uval namelen,
			      __inbuf(newlen) char *newname,
			      __in uval newlen,
			      __CALLER_PID processID);

    virtual SysStatus _statfs(__out struct statfs& buf,
			      __CALLER_PID processID) __xa(none);

    virtual SysStatusUval _readlink(__inbuf(namelen) char *name, 
				    __in uval namelen, 
				    __outbuf(__rc:bufsize) char *buf, 
				    __in uval bufsize,
				    __CALLER_PID processID) __xa(none);

    virtual SysStatus _symlink(__inbuf(namelen) char *name, 
			       __in uval namelen, 
			       __inbuf(*) char *oldpath,
			       __CALLER_PID processID) __xa(none);

    /* This method is intended for NameTrees that involve virtual files
     * (files associated with some process/data structure). For storage
     * based files, it should not be invoked
     */
    virtual SysStatus _createVirtFile(__inbuf(namelen)char *name,
				      __in uval namelen,
				      __in uval mode,
				      __in ObjectHandle& vfoh,
				      __CALLER_PID processID);

    virtual SysStatus _sync();

    // anyone can use this oh
    virtual SysStatus giveAccessByServer(ObjectHandle& returnOh) {
	// FIXME: delete
	tassert(0, err_printf("is this called\n"));
	returnOh = oh;
	return 0;
    }

public:
    // string is
    // the path that the file system is mounted under.
    // Boolean argument 'volatile' specifies if data cached in the name tree
    // can change behind the scenes, i.e., without going through the
    // FS interface (e.g. NFS)
    static void Create(char *pathToMount, DirLinuxFSRef dir,
		       char *desc, uval descLen, uval isCoverable = 1);
};

#endif /* #ifndef __NAME_TREE_LINUX_FS_H_ */
