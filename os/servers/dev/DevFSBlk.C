/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSBlk.C,v 1.11 2004/10/05 21:28:20 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple devfs nodes for block devices.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DevFSBlk.H"
#include "DevFSDir.H"
#include <fslib/ServerFileBlock.H>
#include <meta/MetaBlockDev.H>
#include "FileSystemDev.H"
#include "FileInfoDev.H"

class ServerFileBlk;
typedef ServerFileBlk** ServerFileBlkRef;

/* virtual */ void
DevFSBlk::__DevFSBlk::init(const char* n, dev_t dev,
			   uval mode, uval rdev,
			   uval uid, uval gid, ObjectHandle oh)
{
    stubFRP.setOH(oh);
    FileInfoDev::init(n, dev, mode, rdev, uid, gid);
}


class ServerFileBlk : public ServerFileBlock<StubFRPA> {

    StubFRProvider stubFRP;
    char* name;
    uval clientCount;
public:
    ServerFileBlk():stubFRP(StubBaseObj::UNINITIALIZED) {};
    virtual ~ServerFileBlk() {};
    DEFINE_REFS(ServerFileBlk);
    DEFINE_GLOBAL_NEW(ServerFileBlk);
    virtual SysStatus init(ObjectHandle frProviderOH, FileInfoVirtFS *tok,
			   char *fname) {
	SysStatus rc = 0;
	fileInfo = tok;
	name = fname;
	clientCount = 0;
	stubFRP.setOH(frProviderOH);


	uval bSize,dSize;
	rc = stubFRP._open(dSize,bSize);
	_IF_FAILURE_RET(rc);

	ObjectHandle foooh;
	foooh.init();
	rc = ServerFileBlock<StubFRPA>::init(foooh);
	if (_SUCCESS(rc)) {
	    CObjRootSingleRep::Create(this);
	}

	FileLinux::Stat status;
	rc = tok->lockGetStatus(&status);

	_IF_FAILURE_RET(rc);

	status.st_size = dSize;
	status.st_blksize = bSize;
	status.st_blocks = dSize/bSize;

	fileLength = dSize;

	rc = tok->unlockPutStatus(&status);

	_IF_FAILURE_RET(rc);

	return rc;
    }
    virtual SysStatus locked_createFR() {
	SysStatus rc = 0;
	ObjectHandle frpOH;
	rc = stubFRP._getFROH(frpOH, DREFGOBJ(TheProcessRef)->getPID());

	tassert( _SUCCESS(rc), err_printf("woops\n"));

	_IF_FAILURE_RET(rc);

	stubFR =  new StubFRHolder();
	stubFR->stub.setOH(frpOH);
	return 0;
    }
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type) {
	SysStatus rc;

	if (type!=0 && MetaBlockDev::isChildOf(type)) {
	    return stubFRP._giveAccess(oh, toProcID, match, nomatch, type);
	}
	rc = ServerFileBlock<StubFRPA>::giveAccessSetClientData(oh,
								toProcID,
								match,
								nomatch,
								type);
	if (_SUCCESS(rc)) {
	    ++clientCount;
	}
	return rc;
    }
    static SysStatus Create(ServerFileRef &fref, FileInfoVirtFS *token,
			    ObjectHandle pageServerOH, char *fname) {
	SysStatus rc;
	ServerFileBlk *file = new ServerFileBlk;

	tassert((file != NULL),
		err_printf("failed allocate of ServerFileBlockRamFS\n"));

	rc = file->init(pageServerOH, token, fname);
	if (_FAILURE(rc)) {
	    delete file;
	    fref = NULL;
	    return rc;
	}

	fref = (ServerFileRef) file->getRef();

	return 0;
    }
    virtual SysStatus handleXObjFree(XHandle xhandle) {
	acquireLock();
	--clientCount;
	if (clientCount==0  && stubFR) {
	    stubFR->stub._explicitFsync();
	}
	releaseLock();
	return ServerFileBlock<StubFRPA>::handleXObjFree(xhandle);
    }
    virtual SysStatusUval _ioctl(__in uval req,
				 __inoutbuf(size:size:size) char* buf,
				 __inout uval &size) {
	return stubFRP._ioctl(req,buf,size);
    }
    // call from kernel to re-open this file
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	passertMsg(0, "NYI\n");
	return 0;
    }
};

/* virtual */ SysStatus
DevFSBlk::__DevFSBlk::createServerFileBlock(ServerFileRef &fref) {
    return ServerFileBlk::Create(fref, this, stubFRP.getOH(), name);
}


/* static */ void
DevFSBlk::ClassInit() {
    MetaDevFSBlk::init();
}

/* virtual */SysStatus
DevFSBlk::exportedXObjectListEmpty()
{
    return destroy();
}

/* virtual */ SysStatus
DevFSBlk::destroy() {
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    //This deletes dfsb
    dfsb->destroy();
    dfsb = NULL;
    return destroyUnchecked();
}


/* virtual */ SysStatus
DevFSBlk::init(char* fname, dev_t devID, mode_t mode,
	       ObjectHandle frProviderOH, uval tok)
{
    SysStatus rc = 0;
    CObjRootSingleRep::Create(this);
    token = tok;
    dfsb = new __DevFSBlk;
    dfsb->init(fname, 0, mode, devID, 0, 0, frProviderOH);

    return rc;
}


/* Static */ SysStatus
DevFSBlk::_CreateNode(__inbuf(*) char* name,
		      __in dev_t  devID,
		      __in mode_t mode,
		      __in ObjectHandle parent,
		      __in ObjectHandle pageServer,
		      __out ObjectHandle &oh,
		      __in uval token,
		      __CALLER_PID pid) //StubDevFSBlk
{
    SysStatus rc;
    DevFSBlk *dfb = new DevFSBlk;
    rc = dfb->init(name, devID, mode, pageServer, token);

    _IF_FAILURE_RET(rc);

    rc = FileSystemDev::RegisterNode(dfb->dfsb, name, mode, parent, pid);

    if (_SUCCESS(rc)) {
	rc = dfb->giveAccessByServer(oh,pid);
    }

    if (_FAILURE(rc)) {
	dfb->destroy();
    }

    return rc;
}

/*static */ SysStatus
DevFSBlk::_KickCreateNode(__in ObjectHandle frProviderOH, __CALLER_PID pid)
{
    SysStatus rc;
    StubFRProvider stub(StubBaseObj::UNINITIALIZED);

    char name[256];
    uval devID;
    uval mode;
    ObjectHandle parent;
    ObjectHandle oh;
    uval token;
    stub.setOH(frProviderOH);
    rc = stub._getRegistration(name, 256, devID, mode, parent, token);
    tassertRC(rc,"getting registration params\n");

    rc = _CreateNode(name, devID, mode, parent, frProviderOH, oh, token, pid);
    stub._registerComplete(rc, oh);
    return rc;
}
