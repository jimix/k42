/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemNFS.C,v 1.169 2005/07/21 13:59:34 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <time.h>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaFileSystemNFS.H>
#include <scheduler/Scheduler.H>
#include <stub/StubRegionFSComm.H>
#include <stub/StubKBootParms.H>
#include <stub/StubKernelPagingTransportVA.H>
#include <io/FileLinux.H>
#include <fslib/NameTreeLinuxFS.H>
#include <trace/traceFS.h>
#include <misc/baseStdio.H>
#include <sys/KernelInfo.H>

#include "NFSExport.H"
#include "NFSMount.H"
#include "NFSClient.H"
#include "FileSystemNFS.H"
#include "ServerFileBlockNFS.H"
#include "ServerFileDirNFS.H"

#include <fslib/PagingTransportVA.H>

#include <fslib/FileSystemList.H>
/* static */ FileSystemList FileSystemNFS::instances;

// temporary for performance debugging
/* static */ uval FileSystemNFS::getStatusCounter = 0;
/* static */ uval FileSystemNFS::reValidateTokenCounter = 0;

// if pathNameTo is provided, lookup file before that
SysStatus
FileSystemNFS::lookup(PathName *pathName, uval pathLen,
		PathName *pathNameTo, NFSHandle *fhandle, NFSStat &nfsStat)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS::lookup: method intercepted\n");
    }

    SysStatus rc;
    PathName *currentName = (PathName*)(uval(pathName) + rootPathLen);
    PathName *endName;
    char buf[PATH_MAX+1];

    if (pathNameTo) {
	endName = pathNameTo;
    } else {
	endName = (PathName*)(uval(pathName) + pathLen);
    }

    *fhandle = rootHandle;

    tassert((currentName <= endName),
	    err_printf("currentName > endName"));

    if (currentName == endName) {
	// lookup for root: no NFS lookup to be done, but we need to
	// retrieve NFSStat for it
	NFSClient *cl = getFCL();
	rc = cl->getAttribute(fhandle, nfsStat);
	putFCL(cl);
	return rc;
    }

    // There is a name to iterate on
    uval currentNameLen = pathLen;
    NFSClient *cl = getFCL();
    while (currentName < endName) {
	currentNameLen = currentName->getCompLen(currentNameLen);
	memcpy(buf, currentName->getCompName(currentNameLen), currentNameLen);
	buf[currentNameLen] = 0;

	diropargs dopargs;

	fhandle->copyTo(dopargs.dir);
	dopargs.name = buf;

	rc = cl->lookup(dopargs, fhandle, nfsStat);
	if (_FAILURE(rc)) {
	    putFCL(cl);
	    return rc;
	}

	currentName = currentName->getNext(currentNameLen);
    }
    putFCL(cl);
    return 0;
}

// looks up for a component given the directory handle
SysStatus
FileSystemNFS::lookupComp(FileInfoNFS *dirinfo, char *compName,
			  uval compLen, NFSHandle *fhandle, NFSStat &nfsStat)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for dir inode %ld\n",
		   (uval) dirinfo->status.st_ino);
    }

    char buf[PATH_MAX+1];
    memcpy(buf, compName, compLen);
    buf[compLen] = 0;

    //TraceOSFSNFSLookupComp(dirinfo, buf);

    NFSHandle *dirhandle = &dirinfo->fhandle;
    diropargs dopargs;
    dirhandle->copyTo(dopargs.dir);
    dopargs.name = buf;

    SysStatus rc;
    NFSClient *cl = getFCL();
    rc = cl->lookup(dopargs, fhandle, nfsStat);
    putFCL(cl);
    if (_FAILURE(rc)) {
	return rc;
    }

    // FIXME: access time for directory has not been updated. NFS
    // lookup does not return it, so if we want to keep this information
    // consistent we would have to go to the file server to get it.
    // We are not doing that now, because periodically we do go to
    // the file system any way (there is a time out for cached info)
    return 0;
}

/*
 * forceRevaliation == 0 means that the static variable nfsRevalidation
 * should be checked to decide if we should go to server
 */
SysStatus
FileSystemNFS::revalidate(FileToken finfo, FileLinux::Stat *status,
			  uval forceRevalidation)
{
    // ignoring race conditions related to nfsRevalidation (they're
    // not relevant
    if (forceRevalidation == 0 && shouldRevalidate() == 0) {
	// no revalidation should go to server
	if (status != NULL) {
	    memcpy(status, &FINF(finfo)->status, sizeof(FileLinux::Stat));
	}
	return 0;
    }

    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS:method intercepted for file inode %ld\n",
		   FINF(finfo)->status.st_ino);
    }

    // temporary for performance debugging
    FetchAndAddVolatile(&reValidateTokenCounter, 1);

    SysStatus rc;
    NFSStat nfsStat;
    NFSHandle *fhandle = FHPTR(finfo);
    NFSClient *cl = getFCL();
    rc = cl->getAttribute(fhandle, nfsStat);
    putFCL(cl);
    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == ESTALE) {
	    FileLinux::Stat *oldstat = &FINF(finfo)->status;
	    if (oldstat->isFile() && oldstat->st_nlink > 1) {
		// MultiLinkManager was tracking it, so we need to get rid of
		// it now
		(void) multiLinkMgr.remove(oldstat->st_ino);
	    }
	}
	return rc;
    }
    nfsStat.toLinuxStat(&FINF(finfo)->status);
    if (status != NULL) {
	memcpy(status, &FINF(finfo)->status, sizeof(FileLinux::Stat));
    }

    //TraceOSFSNFSRevalidate((uval) finfo, forceRevalidation,
    //     (uval) FINF(finfo)->status.st_mtime, (uval) FINF(finfo)->modtime);

    return 0;
}

SysStatus
FileSystemNFS::setAttribute(sattrargs &saargs, FileInfoNFS *finfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS:method intercepted for file inode %ld \n",
		   (uval) finfo->status.st_ino);
    }

    //TraceOSFSNFSSetAttribute((uval) finfo);

    SysStatus rc;
    u_int tmpm, tmpc;

    NFSClient *cl = getFCL();
    rc = cl->setAttribute(saargs, &tmpm, &tmpc);
    putFCL(cl);
    if (_FAILURE(rc)) {
	return rc;
    }

    if (finfo) {
	finfo->status.st_mtime = tmpm;
	finfo->modtime = tmpm;
	finfo->status.st_ctime = tmpc;
	finfo->ctime = tmpc;
    }
    return 0;
}

/* virtual */ SysStatus
FileSystemNFS::getFileTokenOrServerFile(
    FileToken dirinfo, char *entryName, uval entryLen,
    FileToken &entryInfo, ServerFileRef &ref, MultiLinkMgrLock* &lock,
    FileLinux::Stat *status)
{
    //TraceOSFSNFSGetFileTokenOrServerFile((uval) dirinfo);

    SysStatus rc;

    if (entryInfo == INVTOK) {
	entryInfo = (FileToken)FileInfoNFS::Alloc();
    }

    NFSHandle *entryhandle = FHPTR(entryInfo);
    NFSStat nfsStat;
    rc = lookupComp(FINF(dirinfo), entryName, entryLen, entryhandle, nfsStat);
    _IF_FAILURE_RET(rc);

    nfsStat.toLinuxStat(&FINF(entryInfo)->status);
    FINF(entryInfo)->modtime = FINF(entryInfo)->status.st_mtime;
    if (status == NULL) {
	status = &FINF(entryInfo)->status;
    } else {
	memcpy(status, &FINF(entryInfo)->status, sizeof(FileLinux::Stat));
    }

    ref = NULL;
    if ((status->st_nlink > 1) && status->isFile()) {
	MultiLinkManager::SFHolder *href;
	multiLinkMgr.acquireLock();
	if (multiLinkMgr.locked_search(status->st_ino, href)==0) {
	    // create server file and add
	    FSFileNFS *fsFile;
	    fsFile = new FSFileNFS((FileSystemRef)getRef(), entryInfo, &st);
	    ObjectHandle oh;
	    SysStatus trc = DREF(tref)->getKptoh(oh);
	    tassertMsg(_SUCCESS(trc) && oh.valid(), "?");
	    ServerFileBlockNFS::Create(ref, fsFile, oh);
	    href = MultiLinkManager::AllocHolder(ref);
	    multiLinkMgr.locked_add(status->st_ino, href);
	} else {
	    ref = href->fref;
	}
	lock = &href->lock;
	lock->acquire();
	multiLinkMgr.releaseLock();
    }

    return rc;
}

SysStatus
FileSystemNFS::create(FileInfoNFS *dinfo, char *name, uval namelen,
		      NFSMode nfsMode, FileInfoNFS *finfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for dir inode %ld\n", (uval)
		   dinfo->status.st_ino);
    }

    SysStatus rc;

    char buf[PATH_MAX+1];
    memcpy(buf, name, namelen);
    buf[namelen] = 0;

    //TraceOSFSNFSCreate((uval) dinfo, (uval) finfo, buf);

    NFSHandle *dhandle = &dinfo->fhandle;

    createargs cargs;

    dhandle->copyTo(cargs.where.dir);
    cargs.where.name = buf;
    cargs.attributes.uid            =  (u_int)-1;
    cargs.attributes.gid            =  (u_int)-1;
    cargs.attributes.size           =   0;
    cargs.attributes.atime.seconds  =  (u_int)-1;
    cargs.attributes.atime.useconds =  (u_int)-1;
    cargs.attributes.mtime.seconds  =  (u_int)-1;
    cargs.attributes.mtime.useconds =  (u_int)-1;
    cargs.attributes.mode           =  nfsMode.mode;
    if (S_ISFIFO(nfsMode.mode)) {
	cargs.attributes.mode = (nfsMode.mode & ~S_IFMT) | S_IFCHR;
	cargs.attributes.size = u_int(NFS2_FIFO_DEV);
    } else if (S_ISCHR(nfsMode.mode) || S_ISBLK(nfsMode.mode)) {
	passertMsg(0, "NIY\n");
    } else {
	tassertMsg(S_ISREG(nfsMode.mode) || S_ISSOCK(nfsMode.mode), 
		   "mode %o\n", nfsMode.mode);
    }
    
    NFSHandle *fhandle = &finfo->fhandle;
    NFSClient *cl = getFCL();
    rc = cl->create(cargs, fhandle);
    putFCL(cl);
    if (_FAILURE(rc)) {
	return rc;
    }

    // We are contacting the server again (asking for attributes) because
    // Linux code in 2.2 said we can't trust attributes being provided by
    // NFS server on creation.
    // FIXME: this is not true anymore on 2.6. We should get create to use the
    // sattr it got back and fill up the other necessary attributes ourselves.
    rc = revalidate((FileToken)finfo, NULL, 1);
    finfo->modtime = finfo->status.st_mtime;
    finfo->ctime = finfo->status.st_ctime;
    _IF_FAILURE_RET(rc);

    // update modtime for directory
    rc = revalidate((FileToken)dinfo, NULL, 1);
    _IF_FAILURE_RET(rc);
    dinfo->modtime = dinfo->status.st_mtime;
    dinfo->ctime = dinfo->status.st_ctime;

    return 0;
}

SysStatusUval
FileSystemNFS::readSynchronous(FileToken finfo, const char *buf,
                               uval32 nbytes, uval32 offset)
{
    // for debugging
    tassert(FINF(finfo) != NULL, err_printf("FileInfo argument NULL\n"));

    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for inode %ld\n", (uval)
	    FINF(finfo)->status.st_ino);
    }

    //TraceOSFSNFSRead((uval) finfo, (uval) nbytes, (uval) offset);

    SysStatus rc;

    readargs rargs;
    NFSHandle *fhandle = FHPTR(finfo);

    fhandle->copyTo(rargs.file);
    rargs.offset = offset;
    rargs.count = nbytes;
    u_int tmpa, tmpm, tmpc;
    NFSClient *cl = getFCL(FINF(finfo)->status.st_uid,
			   FINF(finfo)->status.st_gid);
    rc = cl->read(rargs, (char *)buf, &tmpa, &tmpm, &tmpc);
    putFCL(cl);
    _IF_FAILURE_RET(rc);

    // 0-fill to end of buffer
    if (_SUCCESS(rc) && _SGETUVAL(rc)<nbytes) {
	memset(((char*)buf)+_SGETUVAL(rc), 0, nbytes-_SGETUVAL(rc));
    }

    // FIXME: should these values be updated here or in ServerFileBlockNFS?
    FINF(finfo)->status.st_atime = tmpa;
    if (FINF(finfo)->status.st_mtime < tmpm) {
	FINF(finfo)->status.st_mtime = FINF(finfo)->modtime = tmpm;
    }
    if (FINF(finfo)->status.st_ctime < tmpc) {
	FINF(finfo)->status.st_ctime = FINF(finfo)->ctime = tmpc;
    }

    return rc;
}

/* virtual */ SysStatus
FileSystemNFS::startRead(FileToken finfo, const char *virtAddr, uval32 len,
			 uval32 objOffset, ServerFileBlockNFSRef sf)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for finfo %ld\n", (uval)
	    FINF(finfo)->status.st_ino);
    }

    SysStatus rc;
    uval nPgrs = FetchAndAddSigned(&numberOfPagers, 1);
    if (nPgrs < MAX_ASYNC_CONCURRENT) {
        // handle my own request directly
	rc = readSynchronous(finfo, virtAddr, len, objOffset);
	DREF(sf)->completeFillPage((uval)virtAddr, objOffset, len, rc);
    } else {
        // one of existing pagers will handle
	PagingInfo *pi = new PagingInfo(FINF(finfo), virtAddr, len, objOffset,
					sf, PagingInfo::READ_IO);
	pagerInfoList.add(pi);
    }
    FetchAndAddSigned(&numberOfPagers, -1);

    // become a pager and handle outstanding async i/o if necessary
    return doPendingIO();
}

/* virtual */ SysStatus
FileSystemNFS::writeSynchronous(FileToken finfo, const char *virtAddr,
				uval32 len, uval32 objOffset)
{
    // for debugging
    tassertMsg(FINF(finfo) != NULL, "finfo argument is NULL\n");

    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for inode %ld\n", (uval)
	    FINF(finfo)->status.st_ino);
    }

    //TraceOSFSNFSWriteSynchronous((uval) finfo, (uval) len,
    //     (uval) objOffset);

    SysStatus rc;
    uval s;
    writeargs wargs;
    NFSHandle *fhandle = FHPTR(finfo);

    while (len > 0) {
	u_int tmpm, tmpc;

	s = len;
	if (s > FileSystemNFS::RPC_BUF_MAX) {
	    s = FileSystemNFS::RPC_BUF_MAX;
	}

	fhandle->copyTo(wargs.file);
	wargs.offset = objOffset;
	wargs.data.nfsdata_len = s;
	wargs.data.nfsdata_val = (char*)virtAddr;

	NFSClient *cl = getFCL(FINF(finfo)->status.st_uid,
			       FINF(finfo)->status.st_gid);
	rc = cl->write(wargs, &tmpm, &tmpc);
	putFCL(cl);

	_IF_FAILURE_RET(rc);

	// FIXME dilma: shouldn't we update ctime modtime also?

	// these times are wrong, FIXME, get rid of finfo and pass
	// times to ServerFile on call back
	if (FINF(finfo)->status.st_mtime < tmpm) {
	    FINF(finfo)->status.st_mtime = FINF(finfo)->modtime = tmpm;
	}
	if (FINF(finfo)->status.st_ctime < tmpc) {
	    FINF(finfo)->status.st_ctime = FINF(finfo)->ctime = tmpc;
	}

	s = _SGETUVAL(rc);
	len -= s;
	virtAddr += s;
	objOffset += s;
    }

    //TraceOSFSNFSWriteSynchFinished((uval) finfo,
    //     (uval) FINF(finfo)->status.st_mtime,
    //     (uval) FINF(finfo)->modtime);

    return 0;
}

/* virtual */ SysStatus
FileSystemNFS::startWrite(FileToken finfo, const char *virtAddr, uval32 len,
			 uval32 objOffset, ServerFileBlockNFSRef sf)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for inode %ld\n", (uval)
	    FINF(finfo)->status.st_ino);
    }

    //TraceOSFSNFSStartWrite((uval) finfo,
    //       (uval) len, (uval) objOffset);

    SysStatus rc;
    uval nPgrs = FetchAndAddSigned(&numberOfPagers, 1);
    if (nPgrs < MAX_ASYNC_CONCURRENT) {
	// handle my own request directly
	rc = writeSynchronous(finfo, virtAddr, len, objOffset);
	DREF(sf)->completeWrite((uval)virtAddr, objOffset, len, rc);
    } else {
        // one of existing pagers will handle
	PagingInfo *pi = new PagingInfo(FINF(finfo), virtAddr, len, objOffset,
					sf, PagingInfo::WRITE_IO);
	pagerInfoList.add(pi);
    }
    FetchAndAddSigned(&numberOfPagers, -1);

    // become a pager and handle outstanding async i/o if necessary
    return doPendingIO();
}

SysStatus
FileSystemNFS::doPendingIO()
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted\n");
    }

    PagingInfo *pi;
    SysStatus rc;
    uval nPgrs, got;

    while (1) {
	// if nothing to do, just return
	if (pagerInfoList.isEmpty()) return 0;

	// see if I should be a pager
	nPgrs = FetchAndAddSigned(&numberOfPagers, 1);
	if (nPgrs < MAX_ASYNC_CONCURRENT) { // am a pager
	    // process as many messages as I can get
	    while ((got = pagerInfoList.removeHead(pi)) != 0) {
                switch (pi->ioType) {
                case(PagingInfo::READ_IO):
                    rc = readSynchronous((FileToken)pi->finfo,
                                         pi->buf, pi->nbytes, pi->offset);
                    DREF(pi->sf)->completeFillPage((uval)pi->buf, pi->offset,
                                                   pi->nbytes, rc);
                    delete pi;
                    break;
                case(PagingInfo::WRITE_IO):
                    rc = writeSynchronous((FileToken)pi->finfo,
                                          pi->buf, pi->nbytes, pi->offset);
                    DREF(pi->sf)->completeWrite((uval)pi->buf, pi->offset,
                                                pi->nbytes, rc);
                    delete pi;
                    break;
                }
	    }
	}
	nPgrs = FetchAndAddSigned(&numberOfPagers, -1);
	// if there are already enough pagers, get out of the picture
	if (nPgrs > MAX_ASYNC_CONCURRENT) return 0;
    }

    return 0;
}

SysStatus
FileSystemNFS::fchown(NFSHandle *fhandle, uval32 uid, uval32 gid,
		      FileInfoNFS *finfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted\n");
    }

    SysStatus rc;

    sattrargs saargs;

    fhandle->copyTo(saargs.file);
    saargs.attributes.mode           = (u_int)-1;
    saargs.attributes.uid            = uid;
    saargs.attributes.gid            = gid;
    saargs.attributes.size           = (u_int)-1;
    saargs.attributes.atime.seconds  = (u_int)-1;
    saargs.attributes.atime.useconds = (u_int)-1;
    saargs.attributes.mtime.seconds  = (u_int)-1;
    saargs.attributes.mtime.useconds = (u_int)-1;

    rc = setAttribute(saargs, finfo);
    _IF_FAILURE_RET(rc);

    if (finfo) {
	if (uid != uval32(-1)) {
	    finfo->status.st_uid = uid;
	}
	if (gid != uval32(-1)) {
	    finfo->status.st_gid = gid;
	}
    }

    return 0;
}

SysStatus
FileSystemNFS::mkdir(FileInfoNFS *dinfo, char *compName, uval compLen,
		     NFSMode nfsMode, FileToken *newDirInfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for dir inode %ld\n", (uval)
	    dinfo->status.st_ino);
    }

    SysStatus rc;

    // FIXME dilma: can't we use compName directly?
    char buf[PATH_MAX+1];
    memcpy(buf, compName, compLen);
    buf[compLen] = 0;

    //TraceOSFSNFSMkdir(dinfo, buf);

    createargs cargs;
    NFSHandle *dhandle = FHPTR(dinfo);

    dhandle->copyTo(cargs.where.dir);
    cargs.where.name = buf;
    cargs.attributes.mode           =  nfsMode.mode;
    cargs.attributes.uid            =  (u_int)-1;
    cargs.attributes.gid            =  (u_int)-1;
    cargs.attributes.size           =  (u_int)-1;
    cargs.attributes.atime.seconds  =  (u_int)-1;
    cargs.attributes.atime.useconds =  (u_int)-1;
    cargs.attributes.mtime.seconds  =  (u_int)-1;
    cargs.attributes.mtime.useconds =  (u_int)-1;

    FileInfoNFS *fi;
    fi = FileInfoNFS::Alloc();
    *newDirInfo = (FileToken)fi;

    NFSHandle *fhandle = &fi->fhandle;
    NFSClient *cl = getFCL();
    rc = cl->mkdir(cargs, fhandle);
    putFCL(cl);

    if (_FAILURE(rc)) {
	FileInfoNFS::Free(fi);
	return rc;
    }

    // fill up correctly status information
    rc = revalidate((FileToken)fi, NULL, 1);

    // need to update modtime (and status.st_mtime), but NFS mkdir
    // does not return file attributes for the directory. By querying
    // we get status.st_nlink right too
    revalidate((FileToken) dinfo, NULL, 1);
    dinfo->modtime =dinfo->status.st_mtime;
    dinfo->ctime =dinfo->status.st_ctime;

    return 0;
}

SysStatus
FileSystemNFS::rmdir(FileToken dinfo, char *name, uval namelen)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for dir inode %ld\n", (uval)
	    FINF(dinfo)->status.st_ino);
    }

    char buf[PATH_MAX+1];
    // FIXME dilma: can't we use name directly?
    memcpy(buf, name, namelen);
    buf[namelen] = 0;

    //TraceOSFSNFSRmdir(dinfo, buf);

    NFSHandle *dhandle = FHPTR(dinfo);
    diropargs dopargs;
    dhandle->copyTo(dopargs.dir);
    dopargs.name = buf;

    SysStatus rc;

    NFSClient *cl = getFCL();
    rc = cl->rmdir(dopargs);
    putFCL(cl);

    _IF_FAILURE_RET(rc);

    // need to update modtime (and status.st_mtime), but NFS mkdir
    // does not return file attributes for the directry. By querying
    // we get status.st_nlink right too
    rc = revalidate(dinfo, NULL, 1);
    FINF(dinfo)->modtime = FINF(dinfo)->status.st_mtime;
    FINF(dinfo)->ctime = FINF(dinfo)->status.st_ctime;

    return rc;
}

SysStatus
FileSystemNFS::fchmod(NFSHandle *fhandle, mode_t mode, FileInfoNFS *finfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted\n");
    }

    SysStatus rc;
    NFSMode nfsMode(mode);
    sattrargs saargs;

    fhandle->copyTo(saargs.file);
    saargs.attributes.mode           = nfsMode.mode;
    saargs.attributes.uid            = (u_int)-1;
    saargs.attributes.gid            = (u_int)-1;
    saargs.attributes.size           = (u_int)-1;
    saargs.attributes.atime.seconds  = (u_int)-1;
    saargs.attributes.atime.useconds = (u_int)-1;
    saargs.attributes.mtime.seconds  = (u_int)-1;
    saargs.attributes.mtime.useconds = (u_int)-1;

    rc = setAttribute(saargs, finfo);
    _IF_FAILURE_RET(rc);

    if (finfo) {
	mode_t fmt = finfo->status.st_mode & S_IFMT;
	finfo->status.st_mode = fmt|mode;
    }

    return 0;
}

SysStatus
FileSystemNFS::truncate(FileInfoNFS *dirinfo, char *entryName, uval entryLen,
			uval32 length)
{
    SysStatus rc;
    NFSHandle fhandle;
    NFSStat nfsStat;
    rc = lookupComp(dirinfo, entryName, entryLen, &fhandle, nfsStat);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = ftruncate(&fhandle, length);
    if (_FAILURE(rc)) {
	return rc;
    }

    return 0;
}

SysStatus
FileSystemNFS::utime(NFSHandle *fhandle, const struct utimbuf *utbuf,
		     FileInfoNFS *finfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted\n");
    }

    SysStatus rc;

    sattrargs saargs;

    fhandle->copyTo(saargs.file);
    saargs.attributes.mode           = (u_int)-1;
    saargs.attributes.uid            = (u_int)-1;
    saargs.attributes.gid            = (u_int)-1;
    saargs.attributes.size           = (u_int)-1;

    // Always set usecons to zero
    saargs.attributes.atime.useconds = 0;
    saargs.attributes.mtime.useconds = 0;

    if (utbuf == NULL) {
	time_t now = time(NULL);
	saargs.attributes.atime.seconds  = (u_int) now;
	saargs.attributes.mtime.seconds  = (u_int) now;
    } else {
	saargs.attributes.atime.seconds  = (u_int)utbuf->actime;
	saargs.attributes.mtime.seconds  = (u_int)utbuf->modtime;
    }

    rc = setAttribute(saargs, finfo);
    _IF_FAILURE_RET(rc);

    return 0;
}

SysStatus
FileSystemNFS::ftruncate(NFSHandle *fhandle, uval32 length, FileInfoNFS *finfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted\n");
    }

    SysStatus rc;

    sattrargs saargs;
    fhandle->copyTo(saargs.file);
    saargs.attributes.mode           = (u_int)-1;
    saargs.attributes.uid            = (u_int)-1;
    saargs.attributes.gid            = (u_int)-1;
    saargs.attributes.size           = (u_int)length;
    saargs.attributes.atime.seconds  = (u_int)-1;
    saargs.attributes.atime.useconds = (u_int)-1;
    saargs.attributes.mtime.seconds  = (u_int)-1;
    saargs.attributes.mtime.useconds = (u_int)-1;

    rc = setAttribute(saargs, finfo);
    _IF_FAILURE_RET(rc);

    finfo->status.st_size = (off_t) length;

    return 0;
}

SysStatus
FileSystemNFS::link(NFSHandle *oldFileHandle, FileInfoNFS *newDirInfo,
		    char *newname, uval newlen, FileInfoNFS *oldFileInfo,
		    ServerFileRef fref)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for old file inode %ld\n",
		   (uval) oldFileInfo->status.st_ino);
    }

    char buf[PATH_MAX+1];
    memcpy(buf, newname, newlen);
    buf[newlen] = 0;

    //traceFS_ref2_str1(TRACE_FS_NFS_link, (uval) newDirInfo, (uval) oldFileInfo,
    //	      buf);

    SysStatus rc;
    linkargs largs;

    NFSHandle *newDirHandle = FHPTR(newDirInfo);
    oldFileHandle->copyTo(largs.from);
    newDirHandle->copyTo(largs.to.dir);
    largs.to.name = buf;

    NFSClient *cl = getFCL();
    rc = cl->link(largs);
    putFCL(cl);

    _IF_FAILURE_RET(rc);

    rc = revalidate((FileToken)oldFileInfo, NULL, 1);
    if ((fref != NULL) &&
	(FINF((FileToken)oldFileInfo)->status.st_nlink == 2)) {
	MultiLinkManager::SFHolder *href = MultiLinkManager::AllocHolder(fref);
	multiLinkMgr.add(
	    (uval)(FINF((FileToken)oldFileInfo)->status.st_ino), href);
    }

    // need to update modtime (and status.st_mtime), but NFS link
    // does not return file attributes for the directory. By querying
    // we get status.st_nlink right too
    rc = revalidate((FileToken) newDirInfo, NULL, 1);
    newDirInfo->modtime = newDirInfo->status.st_mtime;
    newDirInfo->ctime = newDirInfo->status.st_ctime;

    return rc;
}

/* virtual */ SysStatus
FileSystemNFS::rename(FileToken oldDirInfo, char *oldname, uval oldlen,
		      FileToken newDirInfo, char *newname, uval newlen,
		      FileToken renamedFinfo)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for oldDir inode %ld "
		   "and new dir inode %ld\n",
		   (uval) FINF(oldDirInfo)->status.st_ino,
		   (uval) FINF(newDirInfo)->status.st_ino);
    }

    SysStatus rc;
    char oldbuf[PATH_MAX+1];
    memcpy(oldbuf, oldname, oldlen);
    oldbuf[oldlen] = 0;
    char newbuf[PATH_MAX+1];
    memcpy(newbuf, newname, newlen);
    newbuf[newlen] = 0;

    //traceFS_ref2_str2(TRACE_FS_NFS_rename, (uval) oldDirInfo, (uval) newDirInfo,
    //	      oldbuf, newbuf);

    NFSHandle *oldDirHandle = FHPTR(oldDirInfo);
    NFSHandle *newDirHandle = FHPTR(newDirInfo);

    renameargs  rnargs;

    oldDirHandle->copyTo(rnargs.from.dir);
    rnargs.from.name = oldbuf;
    newDirHandle->copyTo(rnargs.to.dir);
    rnargs.to.name = newbuf;

    NFSClient *cl = getFCL();
    rc = cl->rename(rnargs);
    putFCL(cl);
    _IF_FAILURE_RET(rc);

    // if renamedFinfo provided, keep it up to date
    if (renamedFinfo != INVTOK) {
	rc = revalidate(renamedFinfo, NULL, 1);
	FINF(renamedFinfo)->modtime = FINF(renamedFinfo)->status.st_mtime;
	FINF(renamedFinfo)->ctime = FINF(renamedFinfo)->status.st_ctime;
	_IF_FAILURE_RET(rc);
    }

    // update cached info about directories
    rc = revalidate(oldDirInfo, NULL, 1);
    _IF_FAILURE_RET(rc);
    FINF(oldDirInfo)->modtime = FINF(oldDirInfo)->status.st_mtime;
    FINF(oldDirInfo)->ctime = FINF(oldDirInfo)->status.st_ctime;

    rc = revalidate(newDirInfo, NULL, 1);
    _IF_FAILURE_RET(rc);
    FINF(newDirInfo)->modtime = FINF(newDirInfo)->status.st_mtime;
    FINF(newDirInfo)->ctime = FINF(newDirInfo)->status.st_ctime;

    //TraceOSFSNFSRenameFinalDirMtime(
    //     (uval)oldDirInfo, (uval)FINF(oldDirInfo)->modtime,
    //     (uval)newDirInfo, (uval)FINF(newDirInfo)->modtime);

    return rc;
}

SysStatus
FileSystemNFS::unlink(FileToken dinfo, char *name, uval namelen,
		      FileToken finfo, uval *nlinkRemain)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	uval size = strlen(".tmpFile");
	if (namelen > size  && strncmp(name, ".tmpFile", size)) {
	    // removing a file created through renameForUnlink
	    return 0;
	} else {
	    passertWrn(0, "FileSystemNFS: method intercepted for dir inode %ld\n",
		       (uval) FINF(dinfo)->status.st_ino);
	}
    }

    SysStatus rc;

    /*
     * check if I have to remove from list of files with multiple
     * links
     */
    FileToken tok;
    if (finfo != INVTOK) {
	tok = finfo;
	rc = revalidate(tok, NULL, 1);
    } else {
	tok = (FileToken)FileInfoNFS::Alloc();
	NFSStat nfsStat;
	rc = lookupComp(FINF(dinfo), name, namelen, FHPTR(tok), nfsStat);
	if (_FAILURE(rc)) {
	    freeFileToken(tok);
	    return rc;
	}
	nfsStat.toLinuxStat(&FINF(tok)->status);
    }

    FileSystemNFS::FileInfoNFS *fi = FileSystemNFS::FINF(tok);
    if (_SUCCESS(rc)) {
	if (fi->status.st_nlink == 2) {	 // was multi, will become single
	    (void) multiLinkMgr.remove(fi->status.st_ino);
	}
	if (nlinkRemain != NULL) {
	    *nlinkRemain = fi->status.st_nlink-1;
	}
    } else {
	return rc;
    }

    if (finfo == INVTOK) {
	// free token we allocated for checking nlink
	freeFileToken(tok);
    }

    char buf[PATH_MAX+1];
    memcpy(buf, name, namelen);
    buf[namelen] = 0;

    //TraceOSFSNFSUnlink(dinfo, finfo, buf);

    // now do the real unlink

    NFSHandle *dhandle = FHPTR(dinfo);
    diropargs dopargs;
    dhandle->copyTo(dopargs.dir);
    dopargs.name = buf;

    NFSClient *cl = getFCL();
    rc = cl->unlink(dopargs);
    putFCL(cl);

    _IF_FAILURE_RET(rc);

    // update cached info about directory
    rc = revalidate(dinfo, NULL, 1);
    FINF(dinfo)->modtime = FINF(dinfo)->status.st_mtime;
    FINF(dinfo)->ctime = FINF(dinfo)->status.st_ctime;
    _IF_FAILURE_RET(rc);

    // if given finfo, we still have access to its status. Let's
    // update it if file still exists
    if (finfo != INVTOK && fi->status.st_nlink > 1) {
	(void) revalidate(finfo, NULL, 1);
	FINF(finfo)->modtime = FINF(finfo)->status.st_mtime;
	FINF(finfo)->ctime = FINF(finfo)->status.st_ctime;
    }

    //TraceOSFSNFSUnlinkFinalDirMtime((uval) dinfo, (uval)
    //	     FINF(dinfo)->modtime);

    return rc;
}

SysStatus
FileSystemNFS::readlink(NFSHandle *fhandle, char *rlbuf, uval maxbuflen)
{
    NFSClient *cl = getFCL();
    SysStatus rc = cl->readlink(fhandle, rlbuf, maxbuflen);
    putFCL(cl);
    return rc;
}

// FIXME Orran (from dilma): I'm assuming that oldname is null terminated.
// Notice that newname is not null terminated (due to the way we pass around
// parts of a PathName representation of the path name)
SysStatus
FileSystemNFS::symlink(FileInfoNFS *dinfo, char *newname, uval newlen,
		       char *oldname)
{
    SysStatus rc;

    // transform newname into a null terminated string
    char buf[PATH_MAX+1];
    memcpy(buf, newname, newlen);
    buf[newlen] = 0;

    NFSHandle *dhandle = FHPTR(dinfo);

    symlinkargs slargs;

    dhandle->copyTo(slargs.from.dir);
    slargs.from.name = buf;
    slargs.to = oldname;
    slargs.attributes.mode           =  S_IFLNK;
    slargs.attributes.uid            =  (u_int)-1;
    slargs.attributes.gid            =  (u_int)-1;
    slargs.attributes.size           =   0;
    slargs.attributes.atime.seconds  =  (u_int)-1;
    slargs.attributes.atime.useconds =  (u_int)-1;
    slargs.attributes.mtime.seconds  =  (u_int)-1;
    slargs.attributes.mtime.useconds =  (u_int)-1;
    NFSClient *cl = getFCL();
    rc = cl->symlink(slargs);
    putFCL(cl);
    _IF_FAILURE_RET(rc);

    // need to update modtime (and status.st_mtime), but NFS symlink
    // does not return file attributes for the directory.
    revalidate((FileToken) dinfo, NULL, 1);
    dinfo->modtime =dinfo->status.st_mtime;
    dinfo->ctime =dinfo->status.st_ctime;

    return 0;
}

void
FileSystemNFS::GetRootHostAndPath(char *host, char *path)
{
    static char buf[256];
    if (!buf[0]) {
	StubKBootParms::_GetParameterValue("K42_NFS_ROOT", buf, 256);
    }

    uval pathLen = strlen(buf);

    // the following is probably not needed
    if ((buf[pathLen-1]) == '/') {
	buf[pathLen-1] = 0;
    }

    char *colon = strchr(buf, ':');

    passertMsg(colon,"K42_NFS_ROOT env variable is broken");
    *colon = 0;

    strcpy(host, buf);
    strcpy(path, colon+1);
}

void
FileSystemNFS::GetUIDAndGID(sval &uid, sval &gid)
{
    static char buf[256];
    if (!buf[0]) {
	StubKBootParms::_GetParameterValue("K42_NFS_ID", buf, 256);
    }

    char *colon = strchr(buf, ':');
    passertMsg(colon,"K42_NFS_ID env variable is broken");
    *colon = 0;

    uid = atoi(buf);
    gid = atoi(colon+1);
}

char FileSystemNFS::host[256]={0,};
char FileSystemNFS::path[PATH_MAX+1];
sval FileSystemNFS::defuid = -1;
sval FileSystemNFS::defgid = -1;

/*
 * ClassInit
 * remotepath is the path on the remote server. If none (NULL) is
 *     provided, than we get it from an environment variable.
 * mpath is the place in the name space where we're adding this mount point
 */
/* static */ SysStatus
FileSystemNFS::ClassInit(VPNum vp)
{
    if (vp != 0) {
	return 0;	// nothing to do proc two
    }

    MetaFileSystemNFS::init();
    PagingTransportVA::ClassInit(0);

    return 0;
}

/* static */ SysStatus
FileSystemNFS::Create(char* server, char *rpath, char *mpath,
		      sval uid /* = -1 */, sval gid /* = -1 */,
		      uval isCoverable /* = 1 */)
{
    SysStatus rc;
    DirLinuxFSRef dir;
    FileSystemRef fsRef;

    // check if defuid, defgid, host and path have been already
    // retrieved from the environment for this process
    if (defuid == -1) {
	GetRootHostAndPath(host, path);
	GetUIDAndGID(defuid, defgid);
    }
    passertMsg(defgid != -1, "initialization of defuid/defgid/host/path"
	       " failed\n");

    if (!server) {
	server = host;
    }

    if (rpath == NULL) { // none has been provided, use from environment
	rpath = path;
    }

    if (uid == -1) uid = defuid;
    if (gid == -1) gid = defgid;

    err_printf("NFS mountpoint %s:\n"
	       "  server: %s\n  host:   %s\n  uid:    %ld\n"
	       "  gid:    %ld\n  pid:    0x%ld\n  path:   %s\n",
	       mpath, server, host, uid, gid,
	       _SGETPID(DREFGOBJ(TheProcessRef)->getPID()), rpath);

    FileSystemNFS *obj = new FileSystemNFS();
    rc = obj->init(server, "UDP", rpath, uid, gid);
    if (_FAILURE(rc)) {
	/* if we're doing a mount for a remote path we got from an environment
	 * variable, let's break if things failed */
	tassertMsg(rpath != NULL, "NFS mountpoint %s: request failed\n",
		   rpath);
	return rc;
    }

    // initialize as clustered object, FIXME: do inside object
    fsRef = (FileSystemRef)CObjRootSingleRep::Create(obj);

    instances.add((ObjRef)fsRef, mpath);

    obj->setupFileDestruction();

    FSFile *fi;
    { // CLEAN UP, removing this logic from NameTreeLinuxFS
	PathNameDynamic<AllocGlobal> *pathToFS;
	uval pathLenToFS;
	SysStatus rc;
	pathLenToFS = PathNameDynamic<AllocGlobal>::Create(rpath,
							   strlen(rpath), 0,
							   0, pathToFS,
							   PATH_MAX+1);
	rc = obj->getFSFile(pathToFS, pathLenToFS, NULL, &fi);
	tassertMsg(_SUCCESS(rc), "woops\n");
	pathToFS->destroy(PATH_MAX+1);
    }

    // description of mountpoint
    char tbuf[256];
#if 0
    sprintf(tbuf, "%s:%s nfs rw pid 0x%lx", server, rpath,
	    _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));
#else
    tbuf[0] = '\0';
#endif
    DirLinuxFSVolatile::CreateTopDir(dir, ".", fi);
    NameTreeLinuxFS::Create(mpath, dir, tbuf, strlen(tbuf), isCoverable);

    // initialize special region to be used for mapping pages into FS
    rc = StubRegionFSComm::_Create(obj->vaddrOfSpecialRegion);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    // create PagingTransport Object
    ObjectHandle fsptoh;
    rc = PagingTransportVA::Create(obj->tref, fsptoh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    ObjectHandle kptoh, sfroh;
    // asks the kernel to create a KernelPagingTransport
    rc = StubKernelPagingTransportVA::_Create(fsptoh, kptoh, sfroh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    tassertMsg(kptoh.valid(), "ops");
    DREF(obj->tref)->setKernelPagingOH(kptoh, sfroh);
    return rc;
}

SysStatus
FileSystemNFS::init(char *host, char *proto, char *path, sval uid, sval gid)
{
    SysStatus rc;

    numberOfPagers = 0;

    mount = new NFSMount(host, proto);

    client = new NFSClient(host, proto);

    rc = client->changeAuth(uid, gid);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    rc = mount->mount(path, rootHandle);
    tassertWrn(_SUCCESS(rc), "Invocation of NFS mount failed for host %s "
	       "path %s rc = %ld\n", host, path, rc);
    _IF_FAILURE_RET(rc);

    FileSystemGlobal::init();

    clientControl.init(32);
    for (uval i=0; i<Scheduler::VPLimit;++i) {
	clientList[i].reinit();
    }

    {
	/* We have to be careful while computing rootPathLen. A simple
	 * strlen(path) is not enough, because the provided path may have
	 * stuff like "/a//b/./c", and the code path for future file system
	 * lookup use normalized versions of the path ("/a/b/c").
	 * rootPathLen = strlen(path); */
	PathNameDynamic<AllocGlobal> *pn;
	rootPathLen = PathNameDynamic<AllocGlobal>::Create(path,
							   strlen(path), 0,
							   0, pn,
							   PATH_MAX+1);
	pn->destroy(PATH_MAX+1);
    }

    return 0;
}

static ThreadID BlockedThread = Scheduler::NullThreadID;

FileSystemNFS::~FileSystemNFS()
{
    delete mount;
    delete client;
    // destroy PagingTransport object
    DREF(tref)->destroy();
}

/* static */ SysStatus
FileSystemNFS::Die()
{
    ThreadID tid;

    while (BlockedThread == Scheduler::NullThreadID) {
	Scheduler::DelayMicrosecs(100000);
    }
    tid = BlockedThread;
    BlockedThread = Scheduler::NullThreadID;
    Scheduler::Unblock(tid);
    return 0;
}

/* static */ void
FileSystemNFS::Block()
{
    BlockedThread = Scheduler::GetCurThread();
    while (BlockedThread != Scheduler::NullThreadID) {
	// NOTE: this object better not go away while deactivated
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
}

SysStatusUval
FileSystemNFS::getDents(FileToken dinfo, uval &cookie, struct direntk42 *buf,
			uval len)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS:method intercepted for dir inode %ld\n",
		   (uval) FINF(dinfo)->status.st_ino);
    }

    //TraceOSFSNFSGetDents((uval) dinfo, cookie);

    readdirargs rdargs;
    NFSHandle *dhandle = FHPTR(dinfo);
    dhandle->copyTo(rdargs.dir);
    rdargs.cookie = (nfscookie)cookie;

    tassert(len >= sizeof(struct direntk42),
	    err_printf("buf not large enough for struct dirent\n"));
    tassert(sizeof(struct direntk42) >= sizeof(entry),
	    err_printf("Linux dirent is smaller than NFS dir entry.\n"));
    rdargs.count = FileSystemNFS::RPC_BUF_MAX;

    SysStatusUval rc;
    NFSClient *cl = getFCL();
    rc = cl->getDents(rdargs, buf, len);
    putFCL(cl);
    if (_SUCCESS(rc)) {
	cookie = rdargs.cookie;
    }

    // FIXME: do we want to update atime in dinfo?
    return rc;
}

// create a server file object to represent this block file
/* virtual*/ SysStatus
FileSystemNFS::createServerFileBlock(ServerFileRef &fref, FSFile *finfo)
{
    ObjectHandle oh;
    SysStatus rc = DREF(tref)->getKptoh(oh);
    tassertMsg(_SUCCESS(rc) && oh.valid(), "?");
    return ServerFileBlockNFS::Create(fref, finfo, oh);
}

/* virtual */ SysStatus
FileSystemNFS::createDirLinuxFS(DirLinuxFSRef &rf, PathName *pathName,
				uval pathLen, FSFile *theInfo,
				DirLinuxFSRef par) {
    SysStatus retvalue;
    tassert(FINF(theInfo->getToken())->status.isDir(),
	    err_printf("not a dir.\n"));
    retvalue = ServerFileDirNFS::Create(rf, pathName, pathLen, theInfo, par);
    return (retvalue);
}

/* virtual */ SysStatus
FileSystemNFS::getStatus(FileToken tok, FileLinux::Stat *status)
{
    // temporary for performance debugging
    FetchAndAddVolatile(&getStatusCounter, 1);
    memcpy(status, &FINF(tok)->status, sizeof(FileLinux::Stat));
    return 0;
}

/* virtual */ SysStatus
FileSystemNFS::statfs(struct statfs *buf)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted\n");
    }

    SysStatus rc;
    NFSClient *cl = getFCL();
    rc = cl->statfs(&rootHandle, buf);
    putFCL(cl);

    return rc;
}

/* renameForUnlink is invoked by ServerFileBlockNFS for unlinking a file
 * with nlink==1 and outstanding clients */
/* virtual */ SysStatus
FileSystemNFS::renameForUnlink(FileToken olddinfo, char *name,
			       uval namelen, FileToken finfo,
			       FileToken &dirinfo, char *newname,
			       uval &newlen)
{
    // used when investigating if there is NFS activity going on
    if (shouldIntercept()) {
	passertWrn(0, "FileSystemNFS: method intercepted for olddinfo %ld\n",
		   (uval) FINF(olddinfo)->status.st_ino);
    }

    tassertMsg(finfo != INVTOK, "shouldn't happen\n");
    // we need a FileInfoNFS for root (for returning and for invoking rename)
    FileInfoNFS *di = FileInfoNFS::Alloc();
    tassertMsg(di != NULL, "no memory?\n");
    di->fhandle = rootHandle;

    SysStatus rc;
    // FIXME: find a better way to get an unique ID
    uval id = Scheduler::SysTimeNow();

    do {
	sprintf(newname, ".tmpFile%lx", id);
	newlen = strlen(newname);
	rc = rename(olddinfo, name, namelen, (FileToken) di, newname, newlen,
		    FileSystemGlobal::INVTOK);
	// next id to try, if needed
	id++;
    } while (_FAILURE(rc) && _SGENCD(rc) == EEXIST);

    _IF_FAILURE_RET(rc);

    /* some file systems generate new tokens on rename, so we need
     * to query file system.
     */
    NFSStat nfsStat;
    rc = lookupComp(di, newname, newlen, FHPTR(finfo), nfsStat);
    _IF_FAILURE_RET(rc);
    nfsStat.toLinuxStat(&FINF(finfo)->status);
    FINF(finfo)->modtime = FINF(finfo)->status.st_mtime;
    FINF(finfo)->ctime = FINF(finfo)->status.st_ctime;

    dirinfo = (FileToken) di;

    return 0;
}

/* virtual */ SysStatus
FileSystemNFS::getFSFile(PathName *pathName, uval pathLen,
			 PathName *pathNameTo,
			 FSFile **file)
{
    FileToken finfo;
    NFSStat nfsStat;
    SysStatus rc;

    finfo = (FileToken)FileInfoNFS::Alloc();

    rc = lookup(pathName, pathLen, pathNameTo, FHPTR(finfo), nfsStat);
    if (_FAILURE(rc)) {
	return rc;
    }

    nfsStat.toLinuxStat(&FINF(finfo)->status);
    FINF(finfo)->modtime = FINF(finfo)->status.st_mtime;
    FINF(finfo)->ctime = FINF(finfo)->status.st_ctime;

    *file = new FSFileNFS((FileSystemRef)getRef(), finfo, &st);
    return rc;
}

/* virtual */ SysStatus
FileSystemNFS::revalidate(FileToken dirInfo, char *name, uval namelen,
			  FileLinux::Stat *status)
{
    SysStatus rc;
    NFSHandle handle;
    NFSStat nfsStat;
    rc = lookupComp(FINF(dirInfo), name, namelen, &handle, nfsStat);
    _IF_FAILURE_RET(rc);
    tassertMsg(status != NULL, "?");
    nfsStat.toLinuxStat(status);

    return 0;
}

/* static */ SysStatus
FileSystemNFS::_Mkfs(__inbuf(serverLen) char *server,
		     __in uval serverLen,
		     __inbuf(rpathLen) char *rpath,
		     __in uval rpathLen,
		     __inbuf(mpathLen) char *mpath,
		     __in uval mpathLen,
		     __in sval uid /* = -1 */,
		     __in sval gid /* = -1 */,
		     __in uval isCoverable /* = 1 */)
{
    char s[PATH_MAX+1];
    memcpy(s, server, serverLen);
    s[serverLen] = '\0';

    char r[PATH_MAX+1];
    memcpy(r, rpath, rpathLen);
    s[rpathLen] = '\0';

    char mp[PATH_MAX+1];
    memcpy(mp, mpath, mpathLen);
    mp[mpathLen] = '\0';

    return Create(s, r, mp, uid, gid, isCoverable);
}

/* static */ SysStatus
FileSystemNFS::_TestAlive(char *mpath, uval len)
{
    FileSystemNFSRef fsref = (FileSystemNFSRef) instances.find(mpath, len);
    if (fsref) {
	return 0;
    } else {
	return _SERROR(2774, 0, ENOENT);
    }
    return 0;
}

/* static */ SysStatus
FileSystemNFS::_PrintStats()
{
    if (instances.isEmpty()) {
	err_printf("No kfs file system mounted\n");
	return 0;
    }

    SysStatus rc;
    void *curr = NULL;
    FileSystemNFSRef fsref = NULL;
    while ((curr = instances.next(curr, (ObjRef&)fsref))) {
	rc = DREF(fsref)->printStats();
	_IF_FAILURE_RET(rc);
    }
    return 0;
}

SysStatus
FileSystemNFS::printStats()
{
    err_printf("no stats available for NFS file systems\n");
    return 0;
}
