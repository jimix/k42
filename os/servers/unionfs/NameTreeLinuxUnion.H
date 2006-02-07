#ifndef __NAME_TREE_LINUX_UNION_H_
#define __NAME_TREE_LINUX_UNION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinuxUnion.H,v 1.8 2005/04/27 17:37:36 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Entry for name lookups for our (hacked) union fs.
 ****************************************************************************/

#include <io/NameTreeLinux.H>
#include <stub/StubNameTreeLinux.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <meta/MetaNameTreeLinux.H>

class NameTreeLinuxUnion : public NameTreeLinux __xbase(Obj) {
protected:
    ObjectHandle oh;
    StubNameTreeLinux primNT; // tree for primary FS
    StubNameTreeLinux secNT;  // tree for sec FS

    SysStatus createDirs(char *name, uval namelen, ProcessID pid);

    DEFINE_GLOBAL_NEW(NameTreeLinuxUnion);
    NameTreeLinuxUnion() : primNT(StubObj::UNINITIALIZED),
			   secNT(StubObj::UNINITIALIZED) {}

    SysStatus passAccess(ObjectHandle poh, ObjectHandle &oh, ProcessID pid,
			 AccessRights match, AccessRights nomatch);
	
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

    virtual SysStatus _chown(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval uid, __in uval gid,
			     __CALLER_PID processID) {
	return primNT._chown(name, namelen, uid, gid);
    }

    virtual SysStatus _chmod(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID) {
	return primNT._chmod(name, namelen, mode);
    }

    virtual SysStatus _truncate(__inbuf(namelen) char *name,
				__in uval namelen,
				__in uval length,
				__CALLER_PID processID) {
	return primNT._truncate(name, namelen, length);
    }

    virtual SysStatus _mkdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID);

    virtual SysStatus _rmdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID) {
	return primNT._rmdir(name, namelen);
    }

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in const struct utimbuf& utbuf,
			     __CALLER_PID processID) {
	return primNT._utime(name, namelen, utbuf);
    }

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID) {
	return primNT._utime(name, namelen);
    }

    virtual SysStatus _getStatus(__inbuf(namelen) char *name,
				 __in uval namelen,
				 __out struct stat& status,
				 __in uval followLink,
				 __CALLER_PID processID);

    virtual SysStatus _link(__inoutbuf(oldlen) char *name,
			    __in uval namelen,
			    __inbuf(newlen) char *newname,
			    __in uval newlen,
			    __CALLER_PID processID) {
	return primNT._link(name, namelen, newname, newlen);
    }

    virtual SysStatus _unlink(__inbuf(namelen) char *name,
			      __in uval namelen,
			      __CALLER_PID processID) {
	return primNT._unlink(name, namelen);
    }

    virtual SysStatus _rename(__inbuf(oldlen) char *name,
			      __in uval namelen,
			      __inbuf(newlen) char *newname,
			      __in uval newlen,
			      __CALLER_PID processID) {
	return primNT._rename(name, namelen, newname, newlen);
    }

    virtual SysStatus _statfs(__out struct statfs& buf,
			      __CALLER_PID processID) __xa(none) {
	passertMsg(0, "NIY");
	return 0;
    }

    virtual SysStatus _sync() { return 0; }

    virtual SysStatusUval _readlink(__inbuf(namelen) char *name, 
				    __in uval namelen, 
				    __outbuf(__rc:bufsize) char *buf, 
				    __in uval bufsize,
				    __CALLER_PID processID) __xa(none) {
	SysStatus rc;
	rc = primNT._readlink(name, namelen, buf, bufsize);
	if (_FAILURE(rc)) { // try secondary file system
	    rc = secNT._readlink(name, namelen, buf, bufsize);
	}
	return rc;
    }

    virtual SysStatus _symlink(__inbuf(namelen) char *name, 
			       __in uval namelen, 
			       __inbuf(*) char *oldpath,
			       __CALLER_PID processID) __xa(none) {
	// doing on primary tree only
	return primNT._symlink(name, namelen, oldpath);
    }
    /* This method is intended for NameTrees that involve virtual files
     * (files associated with some process/data structure). For storage
     * based files, it should not be invoked
     */
    virtual SysStatus _createVirtFile(__inbuf(namelen)char *name,
				      __in uval namelen,
				      __in uval mode,
				      __in ObjectHandle& vfoh,
				      __CALLER_PID processID) {
	return _SERROR(2729, 0, EOPNOTSUPP);
    }
    virtual SysStatus _mknod(__inbuf(namelen)char *name,
                             __in uval namelen,
                             __in uval mode,
                             __in uval dev,
			     __CALLER_PID processID) {
	return _SERROR(2879, 0, EOPNOTSUPP);
    }
    virtual SysStatus _bind(__inbuf(namelen)char *name,
			    __in uval namelen,
			    __in uval mode,
			    __in ObjectHandle serverSocketOH,
			    __CALLER_PID processID) {
	return _SERROR(2888, 0, EOPNOTSUPP);
    }
    virtual SysStatus _getSocketObj(__inbuf(namelen)char *name,
			            __in uval namelen,
			            __in ObjectHandle &serverSocketOH,
			            __CALLER_PID processID) {
	return _SERROR(2900, 0, EOPNOTSUPP);
    }
public:
    // string pathToMount is  the path that the file system is mounted under.
    static void Create(char *pathToMount, ObjectHandle primOH,
		       ObjectHandle secOH, char *desc, uval descLen,
		       uval isCoverable = 1);
};

#endif /* #ifndef __NAME_TREE_LINUX_FS_H_ */
