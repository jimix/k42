#ifndef __FSNODE_H_
#define __FSNODE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSNode.H,v 1.8 2003/06/03 14:10:30 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: All requests with pathname go through a mount point.
 * **************************************************************************/

#include <io/PathName.H>
#ifndef EXPORT_FSNODE
#define EXPORT_FSNODE
#include <sys/stat.h>
#include <utime.h>
#include <sys/types.h>
#endif /* #ifndef EXPORT_FSNODE */
#include <io/NameTreeLinux.H>

class FSNode : public NameTreeLinux {
public:
    DEFINE_GLOBAL_NEW(FSNode);
__xprotected:

    virtual SysStatus _getObj(__inbuf(namelen)char *name,
			      __in uval namelen,
			      __in uval oflag, __in uval mode,
			      __out ObjectHandle &oh,
			      __out TypeID &type,
			      __out uval &useType,
			      __CALLER_PID processID) __xa(none){
	return _SERROR(2431, 0, EOPNOTSUPP);
    };

    virtual SysStatus _chown(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval uid, __in uval gid,
			     __CALLER_PID processID) __xa(none){
	return _SERROR(2432, 0, EPERM);
    };

    virtual SysStatus _chmod(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID) __xa(none){
	return _SERROR(2434, 0, EPERM);
    };

    virtual SysStatus _truncate(__inbuf(namelen)
				char *name,
				__in uval namelen,
				__in uval length,
				__CALLER_PID processID) __xa(none){
	return _SERROR(2435, 0, EPERM);
    };

    virtual SysStatus _mkdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID) __xa(none){
	return _SERROR(2436, 0, EOPNOTSUPP);
    };

    virtual SysStatus _rmdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID) __xa(none){
	return _SERROR(2437, 0, EOPNOTSUPP);
    };

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in const struct utimbuf& utbuf,
			     __CALLER_PID processID) __xa(none){
	return _SERROR(2438, 0, EOPNOTSUPP);
    };

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID) __xa(none){
	return _SERROR(2439, 0, EOPNOTSUPP);
    };

    virtual SysStatusUval _getStatus(__inbuf(namelen) char *name,
				     __in uval namelen,
				     __out struct stat& status,
				     __CALLER_PID processID) __xa(none);

    virtual SysStatus _link(__inbuf(namelen) char *name,
			    __in uval namelen,
			    __inbuf(newlen) char *newname,
			    __in uval newlen,
			    __CALLER_PID processID) __xa(none){
	return _SERROR(2440, 0, EPERM);
    };

    virtual SysStatus _unlink(__inbuf(namelen) char *name,
			      __in uval namelen,
			      __CALLER_PID processID) __xa(none){
	return _SERROR(2441, 0, EPERM);
    };

    virtual SysStatus _rename(__inbuf(namelen) char *name,
			      __in uval namelen,
			      __inbuf(newlen) char *newname,
			      __in uval newlen,
			      __CALLER_PID processID) __xa(none){
	return _SERROR(2442, 0, EOPNOTSUPP);
    };

    virtual SysStatus _statfs(__out struct statfs& buf,
			      __CALLER_PID processID) __xa(none) {
	return _SERROR(2096, 0, EOPNOTSUPP);
    }

    virtual SysStatus _createVirtFile(__inbuf(namelen)char *name,
				      __in uval namelen,
				      __in uval mode,
				      __in ObjectHandle & vfoh,
				      __CALLER_PID processID) {
	return _SERROR(2147, 0, EOPNOTSUPP);
    }
public:
    char entryName[16];
    ObjectHandle selfOH;
    TypeID devType;
    struct stat *status;
    SysStatus Register(const char* name, TypeID type, struct stat *s);
};

typedef FSNode **FSNodeRef;

#endif /* #ifndef __FSNODE_H_ */

