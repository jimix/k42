/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSClient.C,v 1.58 2005/04/12 13:53:42 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>

#include "NFSHandle.H"
#include "NFSExport.H"
#include "NFSClient.H"
#include <dirent.h>
#include <io/Socket.H>
#include "nfs.h"
#include <arpa/inet.h>
#include <sys/vfs.h>
#include <stub/StubSystemMisc.H>

// This is NFSv2, only udp is supported.
const char NFSClient::proto[] = "udp";

// Defualt Timeout and Retries
const struct timeval NFSClient::DEF_TIMEOUT = {0,700000}; // 7/10th of a second
const struct timeval NFSClient::DEF_WAIT = {60,0};	// 60 seconds

// g++ does not handle the gcc [index] = value
// initialization syntax yet, so explictly
// initialize the array
uval16 NFSClient::LinuxErrorNo[] =
{
    0,						//  0
    /* [NFSERR_PERM]        = */ EPERM,		//  1
    /* [NFSERR_NOENT]       = */ ENOENT,	//  2
    0, 0, 					//  3,4
    /* [NFSERR_IO]          = */ EIO,          	//  5
    /* [NFSERR_NXIO]        = */ ENXIO,	        //  6
    0, 0, 0, 0, 0, 0, 				//  7-12
    /* [NFSERR_ACCES]       = */ EACCES,	// 13
    0, 0, 0,					// 14-16
    /* [NFSERR_EXIST]       = */ EEXIST,       	// 17
    0,						// 18
    /* [NFSERR_NODEV]       = */ ENODEV,      	// 19
    /* [NFSERR_NOTDIR]      = */ ENOTDIR,      	// 20
    /* [NFSERR_ISDIR]       = */ EISDIR,       	// 21
    0, 0, 0, 0, 0,					// 22-26
    /* [NFSERR_FBIG]        = */ EFBIG,        	// 27
    /* [NFSERR_NOSPC]       = */ ENOSPC,	// 28
    0,						// 29
    /* [NFSERR_ROFS]        = */ EROFS,		// 30
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 			// 31-40
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 			// 41-50
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 			// 51-60
    0, 0, 					// 61,62
    /* [NFSERR_NAMETOOLONG] = */ ENAMETOOLONG,	// 63
    0, 0, 					// 64,65
    /* [NFSERR_NOTEMPTY]    = */ ENOTEMPTY,	// 66
    0, 0, 					// 68,67
    /* [NFSERR_DQUOT]       = */ EDQUOT,	// 69
    /* [NFSERR_STALE]       = */ ESTALE,	// 70
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 			// 71-80
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 			// 81-90
    0, 0, 0, 0, 0, 0, 0, 0, 				// 91-98
    /* [NFSERR_WFLUSH]      = */ 0		// 99 FIXME: not handling yet
};
SysStatus
NFSClient::rpcCall(uval procnum, uval inProcPtr, uval outProcPtr,
		    uval inPtr, uval outPtr)
{
    SysStatus rc;
    enum clnt_stat cs;
    struct timeval tv = {0,0};
    char *expected = "";

retry:
    cs = clnt_call(client, procnum,
		   (xdrproc_t)inProcPtr, (caddr_t)outProcPtr,
		   (xdrproc_t)inPtr, (caddr_t)outPtr, tv);
    switch (cs) {
    case RPC_SUCCESS:
	rc = 0;
	break;

    // These errors should always retry
    case RPC_CANTSEND:		// failure in sending call
	// sendto(2) failed
    case RPC_CANTRECV:		// failure in receiving result
	// either:
	//  1. network interface is down
	//  2. poll(2) failed
	//  3. rcvfrom(2) failed
    case RPC_TIMEDOUT:		// call timed out
	// should we wait here?
	err_printf("%s[%d]: %s: %s\n", __FUNCTION__, cs, host,
		   clnt_sperrno(cs));

	// just to shut the compiler up
	rc = -1;
	goto retry;
	break;

    // These are unrecoverable at the moment
    default:
	expected = "unexpected error: ";
    case RPC_CANTENCODEARGS:	// can't encode arguments
    case RPC_AUTHERROR:		// authentication error
	// 	      cu->cu_error.re_why = AUTH_INVALIDRESP;
    case RPC_CANTDECODERES:	// can't decode results
	passert(0, err_printf("%s[%d]: %s: %s%s\n", __FUNCTION__, cs,
			      host, expected, clnt_sperrno(cs)));
	rc = -1;
	break;
    }
    return rc;
}


NFSClient::NFSClient(char *aHost, char *aProto, uval prog, uval ver)
    :gid(-1),uid(-1)
{
    lock.init();

    if (aHost == NULL || *aHost == '\0') {
	// FIXME: We accept NULL or an empty string to be INADDR_ANY
	// or more precisely "any local interface" (loopback
	// hopefully). This is required for use by the simulator
	// otherwise we would not allow the NFS mounting of a
	// localdisk.
	aHost = "0";
    }

    // We can ask the client obj for this info but that would require
    // locking it so we just keep it around.
    strncpy(host, aHost, sizeof(host));
    // make sure it's null terminated.
    host[sizeof (host) - 1] = '\0';

    if (strncasecmp(proto, aProto, sizeof(proto)) != 0) {
	passert(0, err_printf("NFS: unsupported protocol: %s\n", aProto));
    }

    // Create the client handle.
    // Since we may be mounting root we cannot use the generic
    // clnt_create() call since it relies /etc/protocols which would
    // not exists yet.

    int sock = RPC_ANYSOCK;
    struct timeval tv = {5, 0};
    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    // set host address
    passert((inet_aton(host, &sin.sin_addr) != 0),
	    err_printf("NFS: inet_aton failed\n"));

    client = clntudp_create(&sin, prog, ver, tv, &sock);

    // WARNING: clnt_spcreateerror() is not async safe and "returns
    // pointer to static data that is over-written on each call".
    passert(client != NULL,
	    err_printf("%s: %s\n", __FUNCTION__, clnt_spcreateerror(host)));

    // FIXME: these should be mount options
    timeout = DEF_TIMEOUT;
    wait = DEF_WAIT;

    //WARNING: setting timeout this way ignores the parameter in cln_call()
    clnt_control(client, CLSET_TIMEOUT, (caddr_t)&wait);
    clnt_control(client, CLSET_RETRY_TIMEOUT, (caddr_t)&timeout);

}

// Special ctor for clone
// By grabbing the sockaddr_in from orig, we save ourselves from
// having to do a remote port lookup.
NFSClient::NFSClient(NFSClient &orig)
    :gid(orig.gid),uid(orig.uid)
{
    lock.init();

    strcpy(host, orig.host);

    // Create the client handle.
    // Since we may be mounting root we cannot use the generic
    // clnt_create() call since it relies /etc/protocols which would
    // may not exists yet.

    int sock = RPC_ANYSOCK;
    struct timeval tv = {5, 0};
    struct sockaddr_in sin;

    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    inet_aton(host,&sin.sin_addr);

    client = clntudp_create(&sin, NFS_PROGRAM, NFS_VERSION, tv, &sock);

    // FIXME: these should be mount options
    timeout = DEF_TIMEOUT;
    wait = DEF_WAIT;

    //WARNING: setting timeout this way ignores the parameter in cln_call()
    clnt_control(client, CLSET_TIMEOUT, (caddr_t)&wait);
    clnt_control(client, CLSET_RETRY_TIMEOUT, (caddr_t)&timeout);

    //FIXME: this is a temporary hack, that assigns
    //       the uid,gid values for the client
    //       from an envvar (NFS_ID) provide by thinwire.
    client->cl_auth = authunix_create("k42", uid, gid, 0, NULL);
}

NFSClient *
NFSClient::clone()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    NFSClient *n = new NFSClient(*this);

    return n;
}

NFSClient::~NFSClient()
{
    passert(0, err_printf("%s: Not supported yet\n", __FUNCTION__));
}

uval16
NFSClient::ToLinuxErrorNo(uval16 NFSErrorNo)
{
    return LinuxErrorNo[NFSErrorNo];
}

SysStatus
NFSClient::changeAuth(sval Uid, sval Gid)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    AUTH *auth;

    if (client->cl_auth)
	auth_destroy(client->cl_auth);
    uid = Uid;
    gid = Gid;
    auth = authunix_create("k42", uid, gid, 0, NULL);
    if (auth == NULL) {
	return _SERROR(1359, 0, EPERM);
    }
    client->cl_auth = auth;

    return 0;
}

SysStatus
NFSClient::lookup(diropargs &dopargs, NFSHandle *fhandle, NFSStat &nfsStat)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&dopres,0, sizeof(dopres));
    rc = rpcCall(NFSPROC_LOOKUP, (uval)xdr_diropargs, (uval)&dopargs,
		  (uval)xdr_diropres, (uval)&dopres);
    if (_SUCCESS(rc)) {
	if (dopres.status != NFS_OK) {
	    rc = _SERROR(1360, 0, ToLinuxErrorNo(dopres.status));
	} else {
	    fhandle->init(dopres.diropres_u.diropok.file);
	    nfsStat.init(&dopres.diropres_u.diropok.attributes);
	}
    }
    return rc;
}

SysStatus
NFSClient::getAttribute(NFSHandle *fhandle, NFSStat &nfsStat)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    nfsfhandle fh;

    fhandle->copyTo(fh);

    memset(&at,0, sizeof(at));
    rc = rpcCall(NFSPROC_GETATTR, (uval)xdr_nfsfhandle, (uval)&fh,
		     (uval)xdr_attrstat, (uval)&at);
    if (_SUCCESS(rc)) {
	if (at.status != NFS_OK) {
	    rc = _SERROR(1361, 0, ToLinuxErrorNo(at.status));
	} else {
	    nfsStat.init(&at.attrstat_u.attributes);
	}
    }
    return rc;
}

SysStatus
NFSClient::setAttribute(sattrargs &saargs, u_int *modTime, u_int *cTime)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&at,0, sizeof(at));
    rc = rpcCall(NFSPROC_SETATTR, (uval)xdr_sattrargs, (uval)&saargs,
		  (uval)xdr_attrstat, (uval)&at);
    if (_SUCCESS(rc)) {
	if (at.status != NFS_OK) {
	    return _SERROR(1362, 0, ToLinuxErrorNo(at.status));
	} else {
	    *modTime = at.attrstat_u.attributes.mtime.seconds;
	    *cTime = at.attrstat_u.attributes.ctime.seconds;
	}
    }
    return rc;
}

inline uval
getRecLen(uval lengthString)
{
    uval recLen = sizeof(struct direntk42);
    recLen += lengthString - sizeof(((direntk42 *)0)->d_name);
    recLen = ALIGN_UP(recLen, sizeof(uval64));
    return recLen;
}

SysStatusUval
NFSClient::getDents(readdirargs &rdargs, struct direntk42 *buf, uval len)
{
    SysStatusUval rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&rdres,0, sizeof(readdirres));
    rc = rpcCall(NFSPROC_READDIR, (uval)xdr_readdirargs, (uval)&rdargs,
		  (uval)xdr_readdirres, (uval)&rdres);

    if (_FAILURE(rc)) {
	// FIXME??? -JX
	return _SERROR(1559, 0, EINVAL);
    }

    if (rdres.status != NFS_OK) {
	return _SERROR(1427, 0, ToLinuxErrorNo(rdres.status));
    }

    entry *ent;
    uval nameLen;
    struct direntk42 *dp;
    struct direntk42 *nextdp;
    uval dpend = (uval)buf + len;

    ent = rdres.readdirres_u.readdirok.entries;
    dp = buf;

    if (ent == NULL) {
	return _SRETUVAL(0);
    }

    // We have already asserted that the buffer is big enough for one dirent
    for (;;) {
	nameLen = strlen(ent->name) + 1; // don't forget '\0'
	if (nameLen > sizeof(dp->d_name)) {
	    return _SERROR(1428, 0,  EINVAL);  //Result buffer is too small.
	}

	dp->d_ino	= ent->fileid;
	dp->d_reclen	= getRecLen(nameLen);
#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	dp->d_type	= DT_UNKNOWN;
#endif
#if defined _DIRENT_HAVE_D_NAMLEN
	dp->d_namlen	= nameLen;
#endif
	memcpy(dp->d_name, ent->name, nameLen);

	rdargs.cookie = ent->cookie;

	ent = ent->nextentry;

	nextdp = (struct direntk42 *)((uval)dp + dp->d_reclen);

	// Make sure we can fit another
	if ((ent != NULL) &&
	    (((uval)nextdp + getRecLen(strlen(ent->name)+1)) < dpend)) {
	    dp->d_off = (uval)nextdp - (uval)buf;
	    dp = nextdp;
	} else {
	    dp->d_off = 0;
	    goto out;
	}
    }

out:
    return _SRETUVAL((uval)nextdp - (uval)buf);
}

SysStatus
NFSClient::mkdir(createargs &cargs, NFSHandle *fhandle)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&dp,0, sizeof(diropres));
    rc = rpcCall(NFSPROC_MKDIR, (uval)xdr_createargs, (uval)&cargs,
		  (uval)xdr_diropres, (uval)&dp);
    if (_SUCCESS(rc)) {
	if (dp.status != NFS_OK) {
	    rc = _SERROR(1364, 0, ToLinuxErrorNo(dp.status));
	} else {
	    fhandle->init(dp.diropres_u.diropok.file);
	}
    }
    return rc;
}

SysStatus
NFSClient::rmdir(diropargs &rdargs)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&stat,0, sizeof(nfsstat));
    rc = rpcCall(NFSPROC_RMDIR, (uval)xdr_diropargs, (uval)&rdargs,
		   (uval)xdr_nfsstat, (uval)&stat);
    if (_SUCCESS(rc)) {
	if (stat != NFS_OK) {
	    rc = _SERROR(2563, 0, ToLinuxErrorNo(stat));
	}
    }
    return rc;
}

SysStatus
NFSClient::create(createargs &cargs, NFSHandle *fhandle)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&dp,0, sizeof(diropres));
    rc = rpcCall(NFSPROC_CREATE, (uval)xdr_createargs, (uval)&cargs,
		  (uval)xdr_diropres, (uval)&dp);
    if (_SUCCESS(rc)) {
	if (dp.status != NFS_OK) {
	    rc = _SERROR(1860, 0, ToLinuxErrorNo(dp.status));
	} else {
	    fhandle->init(dp.diropres_u.diropok.file);
	}
    }
    return rc;
}

SysStatusUval
NFSClient::read(readargs &rargs, char *buf, u_int *accessTime, u_int *modTime,
		u_int *cTime)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    //stick buffer in the read result
    memset(&rres,0, sizeof(readres));
    rres.readres_u.readok.data.nfsdata_val = buf;
    rc = rpcCall(NFSPROC_READ, (uval)xdr_readargs, (uval)&rargs,
		  (uval)xdr_readres, (uval)&rres);
    if (_FAILURE(rc)) {
	tassert(0, err_printf("nfs read failure\n"));
        return _SERROR(1834, 0, EINVAL);
    }

    if (rres.status != NFS_OK) {
	return _SERROR(1365, 0, ToLinuxErrorNo(rres.status));
    }

    *accessTime = rres.readres_u.readok.attributes.atime.seconds;
    *modTime =  rres.readres_u.readok.attributes.mtime.seconds;
    *cTime = rres.readres_u.readok.attributes.ctime.seconds;

    return _SRETUVAL(rres.readres_u.readok.data.nfsdata_len);
}

SysStatusUval
NFSClient::write(writeargs &wargs, u_int *modTime, u_int *cTime)
{
    SysStatusUval rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&astat,0, sizeof(attrstat));
    rc = rpcCall(NFSPROC_WRITE, (uval)xdr_writeargs, (uval)&wargs,
		   (uval)xdr_attrstat, (uval)&astat);
    if (_SUCCESS(rc)) {
	if (astat.status != NFS_OK) {
#ifdef DISPLAY_WRITE_STATUS
	    err_printf("write status:%d\n", astat.status);
#endif
	    rc = _SERROR(1366, 0, ToLinuxErrorNo(astat.status));
	} else {
	    *modTime = astat.attrstat_u.attributes.mtime.seconds;
	    *cTime = astat.attrstat_u.attributes.ctime.seconds;
	    rc = _SRETUVAL(wargs.data.nfsdata_len);
	}
    }
    return rc;
}

SysStatus
NFSClient::link(linkargs &largs)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&stat,0, sizeof(nfsstat));
    rc = rpcCall(NFSPROC_LINK, (uval)xdr_linkargs, (uval)&largs,
		  (uval)xdr_nfsstat, (uval)&stat);
    if (_SUCCESS(rc)) {
	if (stat != NFS_OK) {
	    rc = _SERROR(1764, 0, ToLinuxErrorNo(stat));
	}
    }
    return rc;
}

SysStatus
NFSClient::unlink(diropargs &uargs)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&stat,0, sizeof(nfsstat));
    rc = rpcCall(NFSPROC_REMOVE, (uval)xdr_diropargs, (uval)&uargs,
		  (uval)xdr_nfsstat, (uval)&stat);

    if (_SUCCESS(rc)) {
	if (stat != NFS_OK) {
	    rc = _SERROR(1771, 0, ToLinuxErrorNo(stat));
	}
    }
    return rc;
}

SysStatus
NFSClient::rename(renameargs &rnargs)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&stat,0, sizeof(nfsstat));
    rc = rpcCall(NFSPROC_RENAME, (uval)xdr_renameargs, (uval)&rnargs,
		  (uval)xdr_nfsstat, (uval)&stat);
    if (_SUCCESS(rc)) {
	if (stat != NFS_OK) {
	    rc = _SERROR(1861, 0, ToLinuxErrorNo(stat));
	}
    }
    return rc;
}


SysStatus
NFSClient::symlink(symlinkargs &slargs)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    memset(&stat,0, sizeof(nfsstat));
    rc = rpcCall(NFSPROC_SYMLINK, (uval)xdr_symlinkargs, (uval)&slargs,
		  (uval)xdr_nfsstat, (uval)&stat);
    if (_SUCCESS(rc)) {
	if (stat != NFS_OK) {
	    rc = _SERROR(1773, 0, ToLinuxErrorNo(stat));
	}
    }
    return rc;

}

SysStatus
NFSClient::readlink(NFSHandle *fhandle, char *rlbuf, uval buflen)
{
    SysStatus rc;

    uval maxpath = MIN(buflen, MAXPATHLENGTH);

    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    nfsfhandle fh;
    fhandle->copyTo(fh);

    memset(&rl,0, sizeof(readlinkres));
    rc = rpcCall(NFSPROC_READLINK, (uval)xdr_nfsfhandle, (uval)&fh,
		  (uval)xdr_readlinkres, (uval)&rl);
    if (_SUCCESS(rc)) {
	if (rl.status != NFS_OK) {
	    rc = _SERROR(2896, 0, ToLinuxErrorNo(rl.status));
	} else {
	    strncpy(rlbuf, rl.readlinkres_u.data, maxpath);
	}
    }
    return rc;
}

SysStatus
NFSClient::statfs(NFSHandle *fhandle, struct statfs *fsStat)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    nfsfhandle fh;
    fhandle->copyTo(fh);

    // undefined fields are set to 0 (at least this is what the main
    // page states, and it seems to be what we actually get in a Linux box).
    // This will make sure the data makes sense even on failure.
    memset(fsStat, 0, sizeof (*fsStat));
    fsStat->f_type = NFS_SUPER_MAGIC;	// type of filesystem

    //FIXME: f_fsid should be unique, but for now we let it 0 (as undefined)

    fsStat->f_namelen = MAXNAMELENGTH;  // maximum length of filenames

    memset(&sf,0, sizeof(statfsres));
    rc = rpcCall(NFSPROC_STATFS, (uval)xdr_nfsfhandle, (uval)&fh,
		  (uval)xdr_statfsres, (uval)&sf);
    if (_SUCCESS(rc)) {
	if (sf.status != NFS_OK) {
	    rc = _SERROR(2897, 0, ToLinuxErrorNo(sf.status));
	} else {
	    // If someone wants to make a special object to do this
	    // xlate, go ahead!!

	    // Handy dandy short pointer
	    info_tag *i = &sf.statfsres_u.info;
	    // nfs v2 gives us i->tsize.. I don't know what to use it for.
	    fsStat->f_bsize = i->bsize;	   // optimal transfer block size
	    fsStat->f_blocks = i->blocks;  // total data blocks in file system
	    fsStat->f_bfree = i->bfree;	   // blocks in fs
	    fsStat->f_bavail = i->bavail;  // blocks avail to non-superuser
	}
    }
    return rc;
}

// Not used for anything other than pinging the server. Can be used
// for timeing tests.
// NOTE: not tested or used at the moment.
SysStatus
NFSClient::noOp(void)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return rpcCall(NFSPROC_NULL, (uval)xdr_void, 0, (uval)xdr_void, 0);
}


// unused, saved for next protocol version.
// To be used to tell server to sync(2) filesystem.
SysStatus
NFSClient::writecache(void)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return rpcCall(NFSPROC_WRITECACHE, (uval)xdr_void, 0, (uval)xdr_void, 0);
}

// Obsolete. The function of looking up the root file handle is now
// handled by the mount protocol. This procedure is no longer used
// because finding the root file handle of a file system requires
// moving pathnames between client and server. To do this correctly
// would require the definition of a network standard representation
// of pathnames. Instead, the function of looking up the root file
// handle is done by the MNTPROC_MNT procedure.
// It is here for completion.
SysStatus
NFSClient::root(void)
{
    passert(0, err_printf("Obsolete.. Do NOT use.\n"));

    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return rpcCall(NFSPROC_ROOT, (uval)xdr_void, 0, (uval)xdr_void, 0);
}
