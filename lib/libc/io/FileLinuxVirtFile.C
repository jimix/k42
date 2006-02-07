/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxVirtFile.C,v 1.20 2005/07/15 17:14:23 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxVirtFile.H"
#include <cobj/CObjRootSingleRep.H>

#include "FileLinux.H"
#include <sys/MountPointMgrClient.H>
#include <sys/ppccore.H>
#include <stub/StubNameTreeLinux.H>

/* static */ const uval
FileLinuxVirtFile::MAX_IO_LOAD = (
    PPCPAGE_LENGTH_MAX - 2*sizeof(uval) - sizeof(__XHANDLE));

/* static */ SysStatus
FileLinuxVirtFile::CreateFile(char *name, mode_t mode, ObjectHandle vfoh)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    SysStatus rc;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    rc = FileLinux::GetAbsPath(name, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);

    rc = stubNT._createVirtFile(pth->getBuf(), pthlen, mode, vfoh);

    pth->destroy(maxpthlen);

    return rc;

}

/* virtual */ SysStatusUval
FileLinuxVirtFile::readv(struct iovec* vec, uval vecCount,
			 ThreadWait **tw, GenState &moreAvail)
{
    /* FIXME: for now implementing only for vecCount == 1.
     * In this case it's not necessary to allocate a buffer for
     * performing the read operation and then invoke memcpy_toiovec
     */
    tassertMsg(vecCount == 1, "FileLinuxVirtFile:readv does not support "
	       "vecCount > 1 yet. See FIXME note.\n");
    SysStatusUval rc;
    uval length_read = 0, l, lr;
    uval length = vecLength(vec,vecCount);
    streamLock.acquire();
    moreAvail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;

    char *buf_read = (char*) vec->iov_base;
    char *buf_cur_ptr = buf_read;

    while (length > 0) {
	l = length;
	if (l > MAX_IO_LOAD) {
	    l = MAX_IO_LOAD;
	}

	rc = stub._read(buf_cur_ptr, l);

	if (_FAILURE(rc)) {
	    //semantics shift from EOF error to 0 length on EOF
	    if (_SCLSCD(rc) == FileLinux::EndOfFile) {
		moreAvail.state = FileLinux::ENDOFFILE;
		rc = 0;
	    }
	    streamLock.release();
	    return rc;
	}

	lr = _SGETUVAL(rc);
	length_read += lr;
	buf_cur_ptr += lr;
	length -= lr;
	if (lr < l) break;    // got all there is to it
    }

    streamLock.release();
    return _SRETUVAL(length_read);
}

/* virtual */ SysStatusUval
FileLinuxVirtFile::writev(const struct iovec *vec, uval vecCount,
			  ThreadWait **tw, GenState &moreAvail)
{
    /* FIXME: for now implementing only for vecCount == 1.
     * In this case it's not necessary to allocate a buffer for
     * performing the read operation and then invoke memcpy_toiovec
     */
    tassertMsg(vecCount == 1, "FileLinuxVirtFile:writev does not support "
	       "vecCount > 1 yet. See FIXME note.\n");

    SysStatusUval rc;
    uval length_written = 0, l, lw;
    char* buf_write = (char*) vec->iov_base;
    uval length = vecLength(vec, vecCount);

    streamLock.acquire();
    moreAvail.state = FileLinux::READ_AVAIL|FileLinux::WRITE_AVAIL;

    char *buf_cur_ptr = buf_write;

    while (length > 0) {
	l = length;
	if (l > MAX_IO_LOAD) {
	    l = MAX_IO_LOAD;
	}

	rc = stub._write(buf_cur_ptr, l);

	if (_FAILURE(rc)) {
	    streamLock.release();
	    return rc;
	}

	lw = _SGETUVAL(rc);
	length_written += lw;
	buf_cur_ptr += lw;
	length -= lw;
	if (lw < l) break;    // couldn't write all requested
    }

    streamLock.release();
    return _SRETUVAL(length_written);
}

SysStatus
FileLinuxVirtFile::init(ObjectHandle oh, uval oflags)
{
    FileLinuxRef none = NULL;
    FileLinux::init(none);
    stub.setOH(oh);
    streamLock.init();
    openFlags = oflags;
    return 0;
}

SysStatus
FileLinuxVirtFile::Create(FileLinuxRef &newFile, ObjectHandle toh, uval oflags)
{
    FileLinuxVirtFile *newp;
    SysStatus rc;

    newp = new FileLinuxVirtFile;

    rc = newp->init(toh, oflags);
    tassertMsg(_SUCCESS(rc),"What happens to object ref on failure?\n");
    newFile = (FileLinuxRef)newp->getRef();

    //FIXME - cleanup on failure paths?
    return rc;
}

/* virtual */ SysStatus
FileLinuxVirtFile::lazyGiveAccess(sval fd)
{
    SysStatus rc;
    LazyReOpenData data;
    data.openFlags = openFlags;
    err_printf("FLVF: lazy give access\n");
    // call server to transfer to my process
    rc = stub._lazyGiveAccess(fd, FileLinux_FILE, -1,
			      (char *)&data, sizeof(LazyReOpenData));

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}

/* static */ SysStatus
FileLinuxVirtFile::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, char *buf,
			      uval bufLen)
{
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");
    err_printf("FLVF: lazy re open: %lx\n",oh.xhandle());
    rc = Create(flRef, oh, d->openFlags);
    return rc;
}

/* virtual */ SysStatus
FileLinuxVirtFile::destroy()
{
    AutoLock<StreamLockType> al(&streamLock); // locks now, unlocks on return

    {   // remove all ObjRefs to this object
	SysStatus rc = closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return (_SCLSCD(rc) == 1) ? 0 : rc;
    }

    SysStatus rc = stub._releaseAccess();
    tassertMsg(_SUCCESS(rc), "not dealing with this error\n");

    destroyUnchecked();

    return 0;
}

/* virtual */ SysStatus
FileLinuxVirtFile::dup(FileLinuxRef& newFile)
{
    ObjectHandle newoh;
    SysStatus rc;

    rc = giveAccessByClient(newoh,_SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    if (_SUCCESS(rc)) {
	rc = Create(newFile, newoh, openFlags);
	tassertMsg(_SUCCESS(rc), "Create failed\n");
    }
    return rc;
}
