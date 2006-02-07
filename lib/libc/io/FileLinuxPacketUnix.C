/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxPacketUnix.C,v 1.3 2005/05/06 19:31:36 butrico Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxPacketUnix.H"
#include <stub/StubNameTreeLinux.H>
#include <sys/MountPointMgrClient.H>
#include <sys/socket.h>

/*virtual*/ SysStatus
FileLinuxPacketUnix::bind(const char* addr, uval addrLen)
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

    err_printf("%s %ld (type=%d) %s\n",
	 __PRETTY_FUNCTION__, addrLen, saddr->sa_family, buf);

    // and make sure it is good (for us)
    if (saddr->sa_family!=AF_UNIX) {
	err_printf("I don't know how to bind type %d\n", saddr->sa_family);
	return _SERROR(2914, 0, EINVAL);
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
	rc = Obj::GiveAccessByClient(sps.getOH(), copyOH, doh.pid());
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
		    return _SERROR(2915, 0, ELOOP);
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
FileLinuxPacketUnix::connect(const char* addr, uval addrLen,
			     GenState &moreAvail, ThreadWait **tw)
{
    SysStatus rc;
    sockaddr *saddr;
    char buf[MAXPATHLEN+1];


    // turn address into real struct for addresses
    saddr = (sockaddr *)addr;
    strncpy(buf, saddr->sa_data, MAXPATHLEN);

    uval traceThisSocket = 0;
    if (traceThisSocket)
	err_printf("%s %ld (type=%d) %s\n",
		__PRETTY_FUNCTION__, addrLen, saddr->sa_family, buf);

    // and make sure it is good (for us)
    if (saddr->sa_family!=AF_UNIX) {
	err_printf("I don't know how to connect type %d\n", saddr->sa_family);
	return _SERROR(2916, 0, EINVAL);
    }

    return _SERROR(0, 0, ECONNREFUSED);

    // to be continued
    return rc;
}
