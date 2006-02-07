/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxSocketUnix.C,v 1.9 2005/07/15 17:14:22 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxSocketUnix.H"
#include <stub/StubStreamServerSocket.H>
#include <stub/StubNameTreeLinux.H>
#include <sys/MountPointMgrClient.H>
#include <sys/socket.h>

/*static*/ SysStatus
FileLinuxSocketUnix::Create(FileLinuxRef& newSocket, sval domain, sval type,
			    sval protocol)
{
    SysStatus rc;
    ObjectHandle stubOH;
    uval clientType;
    uval trace = 0;

    rc = StubStreamServerSocket::_CreateSocket(stubOH, clientType, domain,
					       type, protocol, trace);
    if (_SUCCESS(rc))
	rc = FileLinuxSocket::CreateInternal(newSocket, stubOH, clientType,
					     O_RDWR, type, domain, protocol,
					     NULL, 0, NULL, 0,
					     trace);
    if (_FAILURE(rc)) {_SERROR_EPRINT(rc);}
    return rc;
}

/*virtual*/ SysStatus
FileLinuxSocketUnix::bind(const char* addr, uval addrLen)
{
    SysStatus rc;
    sockaddr *saddr;
    char buf[MAXPATHLEN+1];
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    uval retry, numretry = 0;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    ObjectHandle doh, copyOH;

    // turn address into real struct for addresses
    saddr = (sockaddr *)addr;
    strncpy(buf, saddr->sa_data, MAXPATHLEN);

    if (traceThisSocket) {
	err_printf("%s %ld (type=%d) %s\n",
		__PRETTY_FUNCTION__, addrLen, saddr->sa_family, buf);
	traceThisSocket = 0;
    }

    // and make sure it is good (for us)
    if (saddr->sa_family!=AF_UNIX) {
	err_printf("I don't know how to bind type %d\n", saddr->sa_family);
	return _SERROR(2881, 0, EINVAL);
    }

    do {
	retry = 0;

	// make the path obj for the socket file name
	rc = FileLinux::GetAbsPath(buf, pth, pthlen, maxpthlen);
	_IF_FAILURE_RET(rc);

	// get the right fs
	rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
	if (_FAILURE(rc)) {
	   pth->destroy(maxpthlen);
	   return rc;
	}

	// fixme Can I do this more than once?
	rc = Obj::GiveAccessByClient(sss.getOH(), copyOH, doh.pid());
	_IF_FAILURE_RET(rc);

	// talk to the fs to bind the socket
	stubNT.setOH(doh);
        uval mode = S_IFSOCK | DEFFILEMODE;

        rc = stubNT._bind(pth->getBuf(), pthlen, mode, copyOH);

        pth->destroy(maxpthlen);

	if (_FAILURE(rc)) {
	    if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	        (_SGENCD(rc) == ECANCELED)) {
		if (numretry++ > 10) {
		    tassertWrn(0, "too many symbolic links\n");
		    return _SERROR(2889, 0, ELOOP);
		}
		retry = 1;              // retry operation

		rc = FileLinux::GetAbsPath(buf, pth, pthlen, maxpthlen);
		DREFGOBJ(TheMountPointMgrRef)->resolveSymbolicLink(pth, pthlen);
		pth->destroy(maxpthlen);
            } else {
                return rc;
            }

	}
    // and repeat till all symlink in path are resolved.
    } while (retry!=0);

    return rc;
}

/*virtual*/ SysStatus
FileLinuxSocketUnix::connect(const char* addr, uval addrLen,
			     GenState &moreAvail, ThreadWait **tw)
{
    SysStatus rc = 0;
    sockaddr *saddr;
    char buf[MAXPATHLEN+1];
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    uval retry, numretry = 0;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    ObjectHandle doh, serverSocketOH;
    StubStreamServerSocket serverSocket(StubObj::UNINITIALIZED);

    // turn address into real struct for addresses
    saddr = (sockaddr *)addr;
    strncpy(buf, saddr->sa_data, MAXPATHLEN);

    if (traceThisSocket) {
	err_printf("%s: pid=0x%lx  this=%p  len=%ld (type=%d) %s\n",
		__PRETTY_FUNCTION__, DREFGOBJ(TheProcessRef)->getPID(), this,
		addrLen, saddr->sa_family, buf);
        traceThisSocket = 0;
    }

    // and make sure it is good (for us)
    if (saddr->sa_family!=AF_UNIX) {
	err_printf("I don't know how to connect type %d\n", saddr->sa_family);
	return _SERROR(2898, 0, EINVAL);
    }

    if (!startedToConnect) {
    do {
        retry = 0;

        // make the path obj for the socket file name
        rc = FileLinux::GetAbsPath(buf, pth, pthlen, maxpthlen);
        _IF_FAILURE_RET_VERBOSE(rc);

        // get the right fs
        rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
        if (_FAILURE(rc)) {
           pth->destroy(maxpthlen);
           return rc;
        }

        // talk to the fs to connect the socket
        stubNT.setOH(doh);
        rc = stubNT._getSocketObj(pth->getBuf(), pthlen, serverSocketOH);

        pth->destroy(maxpthlen);

	if (_FAILURE(rc)) {
	    if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	        (_SGENCD(rc) == ECANCELED)) {
		if (numretry++ > 10) {
		    tassertWrn(0, "too many symbolic links\n");
		    return _SERROR(2899, 0, ELOOP);
		}
		retry = 1;              // retry operation

		rc = FileLinux::GetAbsPath(buf, pth, pthlen, maxpthlen);
		DREFGOBJ(TheMountPointMgrRef)->resolveSymbolicLink(pth, pthlen);
		pth->destroy(maxpthlen);
            } else {
                return rc;
            }
	} 
    // and repeat till all symlink in path are resolved.
    } while (retry!=0);


    }

    AutoLock<LockType> al(&objLock);

    if (remote != NULL) {
	freeGlobal(remote, remoteLen);
	remote = NULL;
	remoteLen = 0;
    }

    if (!startedToConnect && _SUCCESS(rc)) {
	serverSocket.setOH(serverSocketOH);
	rc = serverSocket._queueToConnect(sss.getOH());
    }


    // a small delay or even a yield is probably most efficient in case we
    // already have another client ready to accept
    if (_SUCCESS(rc)) {
        startedToConnect = 1;
	moreAvail = available;
	rc = sss._connect(NULL, 0, moreAvail);
    }

    if (_SUCCESS(rc)) {
	setAvailable(moreAvail);
	(void) checkAvail(tw, WRITE_AVAIL|EXCPT_SET);
	// Note that this sets the remote address even when we have
	// no yet connected.  We could check here that WRITE_AVAIL is
	// asserted, but the detemination of when a socket is considered
	// connect is in the caller of this method and not here.
	remote = (struct sockaddr*)allocGlobal(addrLen);
	memcpy(remote, buf, addrLen);
	remoteLen = addrLen;
    } else {
	if (traceThisSocket)
	    err_printf("%s: %p: error <%ld %ld %ld>\n",
	           __PRETTY_FUNCTION__, this,
	           _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
    }

    return rc;
}

/* static */ SysStatus
FileLinuxSocketUnix::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, 
				char *buf,
				uval bufLen)
{
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");

    rc = FileLinuxSocket::CreateInternal(flRef, oh, d->transType, d->openFlags,
		        d->domain, d->type, d->protocol,
			&d->local, d->localLen, &d->remote, d->remoteLen);
    return rc;
}
