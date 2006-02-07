#ifndef __NFSCLIENT_H_
#define __NFSCLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSClient.H,v 1.35 2003/05/21 18:03:27 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include "nfs.h"

#include <io/DirBuf.H>
#include <io/Socket.H>
#include <scheduler/Scheduler.H>

class NFSHandle;
class NFSStat;

class NFSClient {
protected:
    typedef BLock LockType;
    LockType  lock;

    CLIENT *client;

    // Enought to fit an ipv6 address, LOL.
    char host[sizeof ("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF")];
    static const char proto[];
    sval gid;
    sval uid;
    static uval16 LinuxErrorNo[];
    // Every unix seems to agree with this value
    enum {NFS_SUPER_MAGIC = 0x6969};
    // RPC retry timeout
    static const struct timeval DEF_TIMEOUT;
    struct timeval timeout;

    static const struct timeval DEF_WAIT;
    struct timeval wait;

    SysStatus rpcCall(uval procnum, uval inProcPtr, uval outProcPtr,
		       uval inPtr, uval outPtr);

    NFSClient(NFSClient &orig);

    union {
	diropres dopres;
	diropres dp;
	readdirres rdres;
	readres rres;
	readlinkres rl;
	statfsres sf;
	attrstat astat;
	attrstat at;
	nfsstat stat;
    };
public:
    DEFINE_GLOBAL_NEW(NFSClient);
    NFSClient(char *aHost, char *aProto,
	      uval prog = NFS_PROGRAM, uval ver = NFS_VERSION);
    virtual ~NFSClient();

    static uval16 ToLinuxErrorNo(uval16 NFSErrorNo);

    SysStatus changeAuth(sval uid, sval gid);

    SysStatus lookup(diropargs &dopargs, NFSHandle *fhandle, NFSStat &nfsStat);
    SysStatus getAttribute(NFSHandle *fhandle, NFSStat &nfsStat);
    SysStatus setAttribute(sattrargs &saargs, u_int *modTime, u_int *cTime);
    SysStatusUval getDents(readdirargs &rdargs,
			   struct direntk42 *buf, uval len);
    SysStatus create(createargs &cargs, NFSHandle *fhandle);
    SysStatus mkdir(createargs &cargs, NFSHandle *fhandle);
    SysStatusUval read(readargs &rargs, char *buf, u_int *accessTime,
		       u_int *modTime, u_int *cTime);
    SysStatusUval write(writeargs &wargs, u_int *modTime, u_int *cTime);
    SysStatus link(linkargs &largs);
    SysStatus unlink(diropargs &uargs);
    SysStatus rmdir(diropargs &rdargs);
    SysStatus rename(renameargs &rnargs);
    NFSClient *clone();

    SysStatus symlink(symlinkargs &slargs);
    SysStatus readlink(NFSHandle *fhandle, char *rlbuf, uval maxbuflen);
    SysStatus statfs(NFSHandle *fhandle, struct statfs *fsStat);

    // these are generally unused
    SysStatus noOp(void);
    SysStatus writecache(void);
    SysStatus root(void);
};

#endif /* #ifndef __NFSCLIENT_H_ */
