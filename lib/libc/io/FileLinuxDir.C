/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxDir.C,v 1.43 2004/03/09 19:32:15 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxDir.H"
#include <cobj/CObjRootSingleRep.H>
#include <sys/MountPointMgrClient.H>
#include <sys/ppccore.H>

/*static*/ SysStatus
FileLinuxDir::Create(FileLinuxRef &r, ObjectHandle toh,
		     uval oflags, const char *nm)
{
    FileLinuxDir *newp;

    newp = new FileLinuxDir;
    newp->init(toh, nm);
    newp->setFlags(oflags);

    r = (FileLinuxRef)newp->getRef();
    return _SRETUVAL(0);
}

void
FileLinuxDir::init(ObjectHandle oh, const char *nm)
{
    //Creates the object ref
    FileLinuxRef none = NULL;
    FileLinux::init(none);

    stub.setOH(oh);
    SysStatusUval rc;

    localCookie = 0;
    goMountP = 0;

    if (nm == NULL) {			// if initialize with null entry
	pth = NULL;
	pthlen = 0;
	return;
    }

    // get the absolute pathname for the
    rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	tassert(0, err_printf("woops\n"));
	return;
    }

    char tmpbuf[sizeof(struct direntk42)];
    tassert (sizeof(tmpbuf) >= sizeof(struct direntk42),
	     err_printf("tmpbuf not large enough for struct direntk42\n"));

    // we now have the path, check if anything mounted underneath
    rc = DREFGOBJ(TheMountPointMgrRef)->getDents(pth, pthlen, tmpbuf,
						 sizeof(tmpbuf));
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    if (_SGETUVAL(rc) != 0) {
	 goMountP = 1;
    }
}

/*virtual*/ SysStatusUval
FileLinuxDir::destroy()
{
    stub._releaseAccess();

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }
    if (pth != NULL) {
	pth->destroy(maxpthlen);
	pthlen = 0;
	pth = NULL;
    }
    destroyUnchecked();
    return _SRETUVAL(0);
}

/*virtual*/ SysStatus
FileLinuxDir::dup(FileLinuxRef& newfile)
{
    ObjectHandle newoh;
    SysStatus rc = stub._dup(_SGETPID(DREFGOBJ(TheProcessRef)->getPID()), newoh);
    tassertMsg(_SUCCESS(rc), "?");

    if (_SUCCESS(rc)) {
	rc = Create(newfile, newoh, openFlags, NULL);
    }
    return rc;
}

/* virtual */ SysStatus
FileLinuxDir::lazyGiveAccess(sval fd)
{
    SysStatus rc;
    LazyReOpenData data;
    data.openFlags = openFlags;
    data.pathLen = pthlen;
    data.maxpthlen = maxpthlen;
    if (pthlen) {
	memcpy(&data.pathBuf, pth->getBuf(), pthlen);
    }
    rc = stub._lazyGiveAccess(fd, FileLinux_DIR, -1,
			      (char *)&data, sizeof(LazyReOpenData));
    tassertMsg(_SUCCESS(rc), "?");

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}

/* virtual */ SysStatusUval
FileLinuxDir::getDents(struct direntk42 *buf, uval len)
{
    SysStatusUval retvalue;

    // Make sure the buffee can hold at least one entry
    if (len < sizeof(*buf)) {
	return _SERROR(1422, 0, EINVAL);
    }
    // FIXME: PPCPAGE is too small, looping over it would be to
    // confusing and would require cursor resets which we are not
    // willing to do yet.
    // FIXME: We reserve room for an extra pointer in our ppc page
    if (len > PPCPAGE_LENGTH_MAX - sizeof(uval))
	len = PPCPAGE_LENGTH_MAX - sizeof(uval);

    if (localCookie == 0 && goMountP) { // go mount point server first
	localCookie++;
	retvalue = DREFGOBJ(TheMountPointMgrRef)->
	    getDents(pth, pthlen, (char *)buf, len);
	return (retvalue);
    }

    return stub._getDents((char *)buf, len);
}

/* virtual */ SysStatus
FileLinuxDir::fchdir()
{
    PathNameDynamic<AllocGlobal> *tmpPath;
    // It is necessary to allocate PATH_MAX+1 regardless the size of
    // pth, because destruction of cwd assumes the maximum size is used
    uval tmpLen = pth->dupCreate(pthlen, tmpPath, PATH_MAX+1);
    return FileLinux::Setcwd(tmpPath, tmpLen);
}
