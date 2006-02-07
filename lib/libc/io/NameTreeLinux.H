#ifndef __NAME_TREE_LINUX_H_
#define __NAME_TREE_LINUX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinux.H,v 1.32 2005/04/27 17:37:04 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: All requests with pathname go through a mount point.
 * **************************************************************************/

#include <io/PathName.H>
#ifndef EXPORT_NAMETREELINUX
#define EXPORT_NAMETREELINUX
#include <sys/stat.h>
#include <utime.h>
#include <sys/vfs.h>
#endif /* #ifndef EXPORT_NAMETREELINUX */

class NameTreeLinux : public Obj {
__xprotected:
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
			      __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _mknod(__inbuf(namelen)char *name,
                             __in uval namelen,
			     __in uval mode,
			     __in uval dev,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _bind(__inbuf(namelen)char *name,
                            __in uval namelen,
			    __in uval mode,
			    __in ObjectHandle serverSocketOH,
			    __CALLER_PID processID) __xa(none)=0;
    virtual SysStatus _getSocketObj(__inbuf(namelen)char *name,
                                    __in uval namelen,
                                    __out ObjectHandle &serverSocketOH,
                                    __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _chown(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval uid, __in uval gid,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _chmod(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _truncate(__inbuf(namelen) char *name,
				__in uval namelen,
				__in uval length,
				__CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _mkdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in uval mode,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _rmdir(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __in const struct utimbuf& utbuf,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _utime(__inbuf(namelen) char *name,
			     __in uval namelen,
			     __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _getStatus(__inbuf(namelen) char *name,
				 __in uval namelen,
				 __out struct stat& status,
				 __in uval followLink,
				 __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _link(__inbuf(namelen) char *name,
			    __in uval namelen,
			    __inbuf(newlen) char *newname,
			    __in uval newlen,
			    __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _unlink(__inbuf(namelen) char *name,
			      __in uval namelen,
			      __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _rename(__inbuf(namelen) char *name,
			      __in uval namelen,
			      __inbuf(newlen) char *newname,
			      __in uval newlen,
			      __CALLER_PID processID) __xa(none)=0;

    virtual SysStatus _statfs(__out struct statfs& buf,
			      __CALLER_PID processID) __xa(none)=0;

    virtual SysStatusUval _readlink(__inbuf(namelen) char *name, 
				    __in uval namelen, 
				    __outbuf(__rc:bufsize) char *buf, 
				    __in uval bufsize,
				    __CALLER_PID processID) __xa(none)=0;
    virtual SysStatus _symlink(__inbuf(namelen) char *name, 
			       __in uval namelen, 
			       __inbuf(*) char *oldpath,
			       __CALLER_PID processID) __xa(none)=0;
    /* This method is intended for NameTrees that involve virtual files
     * (files associated with some process/data structure). 
     */
    virtual SysStatus _createVirtFile(__inbuf(namelen) char *name,
				      __in uval namelen,
				      __in uval mode,
				      __in ObjectHandle& vfoh,
				      __CALLER_PID processID) __xa(none)=0;
    virtual SysStatus _sync() __xa(none)=0;
};

typedef NameTreeLinux **NameTreeLinuxRef;

#endif /* #ifndef __NAME_TREE_LINUX_H_ */

