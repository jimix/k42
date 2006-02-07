/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MountPointMgrClient.C,v 1.23 2005/01/13 22:33:44 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines interface for client side to register
 * and look up mount points.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <io/PathName.H>
#include "MountPointMgrClient.H"
#include <io/FileLinux.H>
#include <stub/StubMountPointMgr.H>
#include <stub/StubNameTreeLinux.H>
#include <cobj/CObjRootSingleRep.H>
#include <sys/KernelInfo.H>

void
MountPointMgrClient::ClassInit(VPNum vp)
{
    if (vp!=0) return;

    MountPointMgrClient *ptr = new MountPointMgrClient();
    CObjRootSingleRep::Create(ptr, (RepRef)GOBJ(TheMountPointMgrRef));
    ptr->init();
}


void
MountPointMgrClient::reloadMountInfo()
{
    SysStatusUval rc;

    const uval REQUEST_BUF_SIZE = 3800; /* yes, this may not be enough; if so,
					 * we handle by dynamically allocated
					 * the buffer */

    char lbuf[REQUEST_BUF_SIZE];    
    char *buffer = lbuf;
    uval bufsize = sizeof(lbuf);

    MountPointMgrCommon::MarshBuf marshBuf;

    // new version that is <= version we will be loading
    version = kernelInfoLocal.systemGlobal.mountVersionNumber;

    uval cur = 0;
    uval left;
    uval dynamicAlloc = 0;

    rc = StubMountPointMgr::_ReadMarshBuf(REQUEST_BUF_SIZE, lbuf, cur, left);
    tassert(_SUCCESS(rc), err_printf("woops rc =%lx\n", rc));

    if (left != 0) { // we need more space
	tassertMsg(_SGETUVAL(rc) == REQUEST_BUF_SIZE, "rc %ld\n", rc);
	dynamicAlloc = 1;
	bufsize = _SGETUVAL(rc) + left;
	buffer = (char*) allocGlobal(bufsize);
	memcpy(buffer, lbuf, _SGETUVAL(rc));

	while (left != 0) {
	    rc = StubMountPointMgr::_ReadMarshBuf(REQUEST_BUF_SIZE,
						  &buffer[cur], cur, left);
	    tassert(_SUCCESS(rc), err_printf("woops rc =%lx\n", rc));
	}
    }

    marshBuf.init(buffer, bufsize, cur);
    mpc.reInit();

    mpc.demarshalFromBuf(&marshBuf);

    if (dynamicAlloc) {
	freeGlobal(buffer, bufsize);
    }
}

/* virtual */ SysStatus
MountPointMgrClient::registerMountPoint(const PathName *mountPath, uval lenMP,
					ObjectHandle oh,
					const PathName *relPath, uval lenRP,
					const char *desc, uval lenDesc,
					uval isCoverable /* = 1 */)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    char *mbuf, *rbuf;
    tassertMsg(mountPath != NULL, "invalid\n");

    // to work around warnings on const declarations
    mbuf = ((PathName *)mountPath)->getBuf();
    // if relPath is NULL, we need to build a buffer for it
    char emptybuf[1];
    if (relPath == NULL) {
	tassertMsg(lenRP == 0, "?");
	rbuf = emptybuf;
    } else {
	rbuf = ((PathName *)relPath)->getBuf();
    }
    return StubMountPointMgr::_RegisterMountPoint(mbuf, lenMP, oh, rbuf, lenRP,
						  desc, lenDesc, isCoverable);
}

SysStatus
MountPointMgrClient::lookup(PathName *name, uval &namelen, uval maxlen,
			    PathName *&unRes, uval &unResLen,
			    ObjectHandle &oh, uval followLink /*= 1*/)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    if (version < kernelInfoLocal.systemGlobal.mountVersionNumber) {
	reloadMountInfo();
    }
    return mpc.lookup(name, namelen, maxlen, unRes, unResLen, oh, followLink);
}

/* virtual */ SysStatusUval
MountPointMgrClient::bind(PathName *oldPath, uval oldlen,
			  PathName *newPath, uval newlen, uval isCoverable)
{
    SysStatusUval retvalue;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    retvalue = StubMountPointMgr::_Bind(oldPath->getBuf(), oldlen,
					newPath->getBuf(), newlen, isCoverable);
    return (retvalue);
}

/* virtual */ SysStatusUval
MountPointMgrClient::getDents(PathName *name, uval namelen, char *buf, uval len)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    if (version < kernelInfoLocal.systemGlobal.mountVersionNumber) {
	reloadMountInfo();
    }
    return mpc.getDents(name, namelen, (struct direntk42 *)buf, len);
}

/* virtual */ SysStatusUval
MountPointMgrClient::readlink(PathName *pth, uval pthlen, uval maxpthlen,
			      char *buf, uval bufsize)
{
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatusUval rc;

    /* 
     * We may want to eventually look in cache in MountPointCommon for this, 
     * but probably negligible advantage.
     */

    // locking in lookup
    rc = lookup(pth, pthlen, maxpthlen, doh, 0);
    if (_FAILURE(rc)) return rc; 

    stubNT.setOH(doh);

    // FIXME: check if cached, if not, read and cache
    rc = stubNT._readlink(pth->getBuf(), pthlen, buf, bufsize);
    return rc;
}

// resolve and cache any symbolic links in this path
/* virtual */ SysStatus
MountPointMgrClient::resolveSymbolicLink(const PathName *pth, uval pthlen)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    return mpc.resolveSymbolicLink(pth, pthlen);
}

/* virtual */ void
MountPointMgrClient::print()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    if (version < kernelInfoLocal.systemGlobal.mountVersionNumber) {
	reloadMountInfo();
    }
    return mpc.print();
}

/* virtual */ SysStatus
MountPointMgrClient::printMtab()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    if (version < kernelInfoLocal.systemGlobal.mountVersionNumber) {
	reloadMountInfo();
    }
    return mpc.printMtab();
}

/* virtual */ SysStatus
MountPointMgrClient::getNameTreeList(
    ListSimple<ObjectHandle*, AllocGlobal>* &list)
{
    return mpc.getNameTreeList(list);
}
