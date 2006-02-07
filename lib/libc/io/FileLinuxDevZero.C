/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxDevZero.C,v 1.9 2005/07/15 17:14:21 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinuxDevZero.H>
#include <stub/StubFRComputation.H>
#include <cobj/CObjRootSingleRep.H>
#include <io/FileLinuxStream.H>

SysStatus
FileLinuxDevZero::Create(FileLinuxRef &newFile, ObjectHandle toh, uval oflags)
{
    FileLinuxDevZero *newp;

    // no initialization, this stinker is *simple*
    newp = new FileLinuxDevZero;
    tassert(newp != NULL, err_printf("new failed.\n"));

    newp->init(toh);
    newp->setFlags(oflags);
    newFile = (FileLinuxRef)CObjRootSingleRep::Create(newp);

    //FIXME - cleanup on failure paths?
    return _SRETUVAL(0);
}

/* virtual */ SysStatus
FileLinuxDevZero::destroy()
{
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }
    destroyUnchecked();
    return 0;
}

/* virtual */ SysStatus
FileLinuxDevZero::getFROH(ObjectHandle &frOH, FRType frType)
{
    SysStatus rc;

    /*
     * Linux provides a new memory object every time /dev/null is mapped
     * Since we are always providing a new memory object, it doesn't
     * matter if the mapping is PRIVATE (copy on write) or SHARED
     */
    rc = StubFRComputation::_Create(frOH);
    return rc;
}

/* virtual */ SysStatusUval
FileLinuxDevZero::writev(const struct iovec *vec, uval vecCount,
			 ThreadWait **tw, GenState &moreAvail)
{
    moreAvail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;
    return _SRETUVAL(vecLength(vec, vecCount));
}

/* virtual */ SysStatusUval
FileLinuxDevZero::readv(struct iovec *vec, uval vecCount,
			ThreadWait **tw, GenState &moreAvail)
{
    uval bytes;

    moreAvail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;

    bytes = 0;
    for (uval i = 0; i<vecCount; ++i) {
	memset(vec[i].iov_base, 0, vec[i].iov_len);
	bytes += vec[i].iov_len;
    }
    return bytes;
}

/*virtual*/ SysStatus
FileLinuxDevZero::dup(FileLinuxRef& newfile)
{
    ObjectHandle newoh;
    SysStatus rc;
    rc = giveAccessByClient(newoh,_SGETPID(DREFGOBJ(TheProcessRef)->getPID()));
    if (_SUCCESS(rc)) {
	//FIXME pass the typeid for this kind of file
	//      either its fixed, or must be remebered in the file object
	rc = Create(newfile, newoh, openFlags);
    }
    return rc;
}

/* virtual */ SysStatus
FileLinuxDevZero::lazyGiveAccess(sval fd)
{
    LazyReOpenData data;
    data.openFlags = openFlags;
    data.transType = TransStreamServer::TRANS_NONE;
     // call server to transfer to my process
    SysStatus rc = stub._lazyGiveAccess(fd, FileLinux_CHR_ZERO, -1,
					(char *)&data, sizeof(LazyReOpenData));
    tassertMsg(_SUCCESS(rc), "?");

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}

/* static */ SysStatus
FileLinuxDevZero::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, char *buf,
			     uval bufLen)
{
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");
    rc = Create(flRef, oh, d->openFlags);
    return rc;
}
