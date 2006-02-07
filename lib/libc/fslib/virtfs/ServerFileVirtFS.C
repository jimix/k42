/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileVirtFS.C,v 1.22 2005/07/20 21:29:44 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <fcntl.h>
#include <io/FileLinux.H>
#include <fslib/DirLinuxFSVolatile.H>
#include "FileInfoVirtFS.H"
#include "ServerFileVirtFS.H"
#include "ServerFileDirVirtFS.H"

#include <scheduler/Scheduler.H>

#include <sys/ProcessSet.H>

/* static */ SysStatus
ServerFileVirtFS::Create(ServerFileRef &fref, FileInfoVirtFS *fsFile,
			 uval extraData, uval tok)
{
    SysStatus rc;
    ServerFileVirtFS* file=NULL;

    SysStatusUval type = fsFile->getServerFileType();
    if (_SUCCESS(type)) {
	switch (_SGETUVAL(type)) {
	case FileInfoVirtFS::VirtLocal:
	    file = new ServerFileVirtFSLocal;
	    break;
	case FileInfoVirtFS::VirtFSInfo:
	    file = new ServerFileVirtFSMerged;
	    break;
	case FileInfoVirtFS::VirtRemote:
	    file = new ServerFileVirtFSRemote;
	    break;
	}
    }
    tassert((file != NULL),
	    err_printf("failed allocate of ServerFileBlockVirtFS\n"));

    rc = file->init(fsFile, extraData, tok);

    if (_FAILURE(rc)) {
	// FIXME: shouldn't delete passed in file here
	// delete file;
	fref = 0;
	DREF(file->getRef())->destroy();
	return rc;
    }

    fref = (ServerFileRef)file->getRef();

    return 0;
}


/* virtual */ SysStatus
ServerFileVirtFS::init(FileInfoVirtFS *fsFile, uval extraData, uval tok)
{
    SysStatus rc;
    FileLinux::Stat stat;
    ServerFile::init();
    fileInfo = fsFile;
    token = tok;

    rc = fsFile->getStatus(&stat);
    _IF_FAILURE_RET(rc);
    fileLength = stat.st_size;
    extraDataSize = extraData;
    return rc;
}

/* virtual */ SysStatus
ServerFileVirtFS::tryToDestroy()
{
    //No locking need here -- in case of destroy race, it's self resolving
    ((FileInfoVirtFS*)fileInfo)->setActiveServerFile(NULL);
    fileInfo = (FileInfoVirtFSDir*)theDeletedObj;
    return ServerFile::tryToDestroy();
}

/* virtual */ SysStatus
ServerFileDirVirtFS::init(PathName *pathName, uval pathLen,
			  FileInfoVirtFSDir* theInfo,
			  DirLinuxFSRef par)
{
    SysStatus rc;
    rc = DirLinuxFSVolatile::init(pathName, pathLen, theInfo, par);
    _IF_FAILURE_RET(rc);
    invalidCache = 0;
    ((FileInfoVirtFS*)fileInfo)->setActiveServerFile((void**)getRef());
    return rc;
}

/* virtual */ SysStatus
ServerFileDirVirtFS::tryToDestroy()
{
    //No locking need here -- in case of destroy race, it's self resolving
    ((FileInfoVirtFS*)fileInfo)->setActiveServerFile(NULL);
    fileInfo = (FileInfoVirtFSDir*)theDeletedObj;
    return DirLinuxFSVolatile::tryToDestroy();
}


template<class T>
/* virtual */ SysStatus
ServerFileVirtFSTmp<T>::init(FileInfoVirtFS *fsFile, T** ref, uval extraData,
			     uval tok)
{
    SysStatus rc;
    obj = ref;
    useType = FileLinux::FIXED_SHARED;

    // FIXME: this's a copy of the definition of FileLinuxVirtFile::MAX_IO_LOAD
    maxIOLoad = PPCPAGE_LENGTH_MAX - 2*sizeof(uval) - sizeof(__XHANDLE);

    rc = DREF(obj)->_getMaxReadSize(bufsize, tok);
    tassertRC(rc, "getMaxReadSize failed\n");
    _IF_FAILURE_RET(rc);


    return ServerFileVirtFS::init(fsFile, extraData, tok);
}


/* virtual */ SysStatus
ServerFileVirtFSLocal::init(FileInfoVirtFS *fsFile, uval extraData, uval tok)
{
    SysStatus rc;

    rc = fsFile->getVirtFile(vfr);
    _IF_FAILURE_RET(rc);
    rc = ServerFileVirtFSTmp<VirtFile>::init(fsFile, &vfr, extraData, tok);
    _IF_FAILURE_RET(rc);
    CObjRootSingleRep::Create(this);
    return rc;
}

/* virtual */ SysStatus
ServerFileVirtFSMerged::init(FileInfoVirtFS* finfo, uval extraData, uval tok)
{
    SysStatus rc;
    vfile = (VirtFSFile*)finfo;
    rc = ServerFileVirtFSTmp<VirtFSFile>::init(finfo, &vfile, extraData, tok);
    _IF_FAILURE_RET(rc);
    CObjRootSingleRep::Create(this);
    return rc;
}

/* virtual */ SysStatus
ServerFileVirtFSRemote::init(FileInfoVirtFS *fsFile, uval extraData, uval tok)

{
    SysStatus rc;

    ObjectHandle oh;
    rc = fsFile->getOH(oh);
    _IF_FAILURE_RET(rc);

    stub.setOH(oh);

    stubPtr = &stub;
    rc = ServerFileVirtFSTmp<StubVirtFile>::init(fsFile, &stubPtr,
						 extraData, tok);
    _IF_FAILURE_RET(rc);

    useType = FileLinux::FIXED_SHARED;
    CObjRootSingleRep::Create(this);

    return rc;
}

/* virtual */ SysStatus
ServerFileVirtFS::giveAccessSetClientData(ObjectHandle &oh,
					  ProcessID toProcID,
					  AccessRights match,
					  AccessRights nomatch,
					  TypeID type)
{
    SysStatus retvalue;
    ClientData *clientData = new(extraDataSize) ClientData(extraDataSize);
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
				  type, (uval)clientData);
    return (retvalue);
}

template<class T>
/* virtual */ SysStatus
ServerFileVirtFSTmp<T>::open(uval oflag, ProcessID pid, ObjectHandle &oh,
			  uval &ut, TypeID &type)
{
    SysStatus rc;
    FileLinux::Stat stat;

    rc = getStatus(&stat);
    if (_FAILURE(rc)) { return rc; }

    fileLength = 0;

    // FIXME get rights correct
    rc = giveAccessByServer(oh, pid,
			    MetaObj::read|MetaObj::write|
			    MetaObj::controlAccess,
			    MetaObj::none);
    if (_FAILURE(rc)) { return rc; }

    // now get xhandle and initialize
    XHandle xh = oh._xhandle;
    ClientData *cd = VClnt(xh);
    cd->flags = oflag;
    cd->filePosition = 0;
    cd->isSharingOffset = 0;
    cd->useType = FileLinux::FIXED_SHARED;
    cd->token = token;
    // at this point, nothing can fail, so no need to free anything
    type = FileLinux_VIRT_FILE;

    ut = useType = FileLinux::FIXED_SHARED;
    return DREF(obj)->_open(oflag, cd->userData(), cd->token);
}

template<class T>
/* virtual */ SysStatusUval
ServerFileVirtFSTmp<T>::_read(char *buf, uval len, __XHANDLE xhandle)
{
    SysStatus rc;
    acquireLock();
#ifdef TESTING_ITFC_WITH_OFFSET
    tassertMsg(len <= maxIOLoad, "len %ld too big for ppc call\n", len);
    rc = DREF(obj)->_readOff(buf, len, Clnt(xhandle)->filePosition,
			     VClnt(xhandle)->userData(),
			     VClnt(xhandle)->token);
    _IF_FAILURE_RET(rc);

    Clnt(xhandle)->filePosition += _SGETUVAL(rc);
    releaseLock();
    return rc;
#else
    passertMsg(Clnt(xhandle)->isSharingOffset == 0, "NIY\n");

    rc = DREF(obj)->_getMaxReadSize(bufsize, VClnt(xhandle)->token);
    if (_FAILURE(rc)) {
	releaseLock();
	return rc;
    }
    tassertMsg(bufsize <= maxIOLoad, "bufsize %ld too big for ppc call\n",
	       len);
    // using read interface where data is returned from beginging of file
    char *full_buf = (char*) AllocGlobal::alloc(bufsize);
    rc = DREF(obj)->_read(full_buf, bufsize,
			  VClnt(xhandle)->userData(), VClnt(xhandle)->token);
    _IF_FAILURE_RET(rc);

    uval length_read = _SGETUVAL(rc);
    if (length_read > 0 && Clnt(xhandle)->filePosition < length_read) {
	uval offset = Clnt(xhandle)->filePosition;
	if (len > length_read - offset) {
	    len = length_read - offset;
	}
	memcpy(buf, &full_buf[offset], len);
	Clnt(xhandle)->filePosition += len;
	releaseLock();
	return _SRETUVAL(len);
    } else {
	releaseLock();
	return _SERROR(2160, FileLinux::EndOfFile, 0);
    }
#endif
}

template<class T>
/* virtual */ SysStatusUval
ServerFileVirtFSTmp<T>::_write(const char *buf, uval len, __XHANDLE xhandle)
{
    SysStatusUval rc;
    acquireLock();
    tassertMsg(len <= maxIOLoad, "len %ld too big for ppc call\n", len);
    rc = DREF(obj)->_write(buf, len, VClnt(xhandle)->userData(),
			   VClnt(xhandle)->token);
    releaseLock();
    return rc;
}

template<class T>
/* virtual */ SysStatus
ServerFileVirtFSTmp<T>::handleXObjFree(XHandle xhandle)
{
    SysStatusUval rc;
    (void) ServerFile::handleXObjFree(xhandle);
    acquireLock();
    rc = DREF(obj)->_close(VClnt(xhandle)->userData(), VClnt(xhandle)->token);
    releaseLock();
    return rc;
}

template<class T>
/* virtual */ SysStatusUval
ServerFileVirtFSTmp<T>::_setFilePosition(__in sval position, __in uval at,
				   __XHANDLE xhandle)
{
    SysStatus rc;
    rc = DREF(obj)->_setFilePosition(position, at,
				     VClnt(xhandle)->userData(),
				     VClnt(xhandle)->token);
    if (_FAILURE(rc) && _SGENCD(rc) == ENOSYS) {
	// implementor does note care about file position

	/* Comment from Dilma: although we don't have an implementation for
	 * _setFilePosition on the obj, we still want to update the filePosition
	 * in the client data. */
	passertMsg(at == FileLinux::ABSOLUTE, "other options not implemented yet\n");
	Clnt(xhandle)->filePosition = position;

	return _SRETUVAL(position);
    } else {
	passertMsg(0, "here?");
	return rc;
    }
}

/* virtual */ SysStatus
ServerFileVirtFS::_lazyGiveAccess(__XHANDLE xhandle,
				  __in sval file, __in uval type,
				  __in sval closeChain,
				  __inbuf(dataLen) char *data,
				  __in uval dataLen)
{
    BaseProcessRef pref;
    SysStatus rc;
    AccessRights match, nomatch;
    ProcessID dummy;
    ObjectHandle oh;
    ProcessID procID;

    // go a giveacessfromserver on object to kernel, passing same rights
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    rc = giveAccessByServer(oh, _KERNEL_PID, match, nomatch);
    if (_FAILURE(rc)) {
	return rc;
    }

    // get process from xhandle
    procID = XHandleTrans::GetOwnerProcessID(xhandle);
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(procID, pref);
    if (_FAILURE(rc)) {
	DREFGOBJ(TheXHandleTransRef)->free(xhandle);
	return rc;
    }

    // make a call on process object to pass along new object handle and
    //    data
    rc = DREF(pref)->lazyGiveAccess(file, type, oh, closeChain, match, nomatch,
				    data, dataLen);
    return rc;
}

template class ServerFileVirtFSTmp<StubVirtFile>;
template class ServerFileVirtFSTmp<VirtFile>;
template class ServerFileVirtFSTmp<VirtFSFile>;
