/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxDevNull.C,v 1.19 2004/01/16 16:37:37 mostrows Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <io/FileLinuxDevNull.H>
#include <io/FileLinuxStream.H>
SysStatus
FileLinuxDevNull::Create(FileLinuxRef &newFile, ObjectHandle toh, uval oflags)
{
    FileLinuxDevNull *newp;

    // no initialization, this stinker is *simple*
    newp = new FileLinuxDevNull;
    tassert(newp != NULL, err_printf("new failed.\n"));

    newp->init(toh);
    newp->setFlags(oflags);
    newFile = (FileLinuxRef)CObjRootSingleRep::Create(newp);

    //FIXME - cleanup on failure paths?
    return _SRETUVAL(0);
}

/* virtual */ SysStatus
FileLinuxDevNull::destroy()
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

/*virtual*/ SysStatus
FileLinuxDevNull::dup(FileLinuxRef& newfile)
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
FileLinuxDevNull::lazyGiveAccess(sval fd)
{
    LazyReOpenData data;
    data.openFlags = openFlags;
    data.transType = TransStreamServer::TRANS_NONE;
     // call server to transfer to my process
    SysStatus rc = stub._lazyGiveAccess(fd, FileLinux_CHR_NULL, -1,
					(char *)&data, sizeof(LazyReOpenData));
    tassertMsg(_SUCCESS(rc), "?");

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}

/* static */ SysStatus
FileLinuxDevNull::LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh, char *buf,
			     uval bufLen)
{
    SysStatus rc;
    LazyReOpenData *d = (LazyReOpenData *)buf;
    tassertMsg((bufLen == sizeof(LazyReOpenData)), "got back bad len\n");
    rc = Create(flRef, oh, d->openFlags);
    return rc;
}

