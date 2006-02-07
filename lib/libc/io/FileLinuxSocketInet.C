/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxSocketInet.C,v 1.7 2005/07/15 17:14:22 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxSocketInet.H"
#include <stub/StubIPSock.H>

/*static*/ SysStatus
FileLinuxSocketInet::Create(FileLinuxRef& newSocket, sval domain, sval type,
			    sval protocol)
{
    SysStatus rc;
    ObjectHandle stubOH;
    uval clientType;

    rc = StubIPSock::_Create(stubOH, clientType, domain, type, protocol);
    if (_SUCCESS(rc))
	rc = FileLinuxSocket::CreateInternal(newSocket, stubOH, clientType,
					     O_RDWR, domain, type, protocol,
					     NULL, 0, NULL, 0);
    return rc;
}

/*virtual*/ SysStatus
FileLinuxSocketInet::bind(const char* addr, uval addrLen)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    SysStatus rc;
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    struct sockaddr *buf = (struct sockaddr*)allocGlobal(addrLen);
    memcpy(buf, addr, addrLen);
    rc = sss._bind((char*)buf, addrLen);
    if (_SUCCESS(rc)) {
	if (local != NULL) {
	    freeGlobal((char*)local, addrLen);
	}
	local = buf;
	localLen = addrLen;
    } else {
	freeGlobal(buf, addrLen);
    }
    return rc;
}

/*virtual*/ SysStatus
FileLinuxSocketInet::connect(const char* addr, uval addrLen,
			     GenState &moreAvail, ThreadWait **tw)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    AutoLock<LockType> al(&objLock);
    SysStatus rc;

    if (remote != NULL) {
	tassertMsg(remoteLen != 0, "freeing 0 sized mem.\n");
	freeGlobal(remote, remoteLen);
	remote = NULL;
	remoteLen = 0;
    }

    moreAvail = available;
    rc = sss._connect(addr, addrLen, moreAvail);

    if (_SUCCESS(rc)) {
	setAvailable(moreAvail);
	(void) checkAvail(tw, WRITE_AVAIL|EXCPT_SET);

	// It would be better to check that the socket is indeed connecting
	// but that desicion is made by our caller.  We are setting remote,
	// which indicates a connected socket even when we are waiting
	// to connect.
	remote = (struct sockaddr*)allocGlobal(addrLen);
	memcpy(remote, addr, addrLen);
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
FileLinuxSocketInet::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, 
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

