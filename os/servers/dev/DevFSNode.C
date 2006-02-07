/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSNode.C,v 1.8 2003/01/16 06:09:44 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple devfs nodes.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DevFSNode.H"
#include <stub/StubFRPANonPageable.H>
#include <fslib/ServerFileBlock.H>
#include <meta/MetaDevFSNode.H>
#include <stub/StubDevFSNode.H>
#include <meta/MetaBlockDev.H>
class ServerFileDev;
typedef ServerFileDev** ServerFileDevRef;

class ServerFileDev : public ServerFileBlock<StubFRPA> {

    StubFRProvider stubFRP;
    char* name;
    uval clientCount;
public:
    ServerFileDev():stubFRP(StubBaseObj::UNINITIALIZED) {};
    virtual ~ServerFileDev() {};
    DEFINE_REFS(ServerFileDev);
    DEFINE_GLOBAL_NEW(ServerFileDev);
    virtual SysStatus init(ObjectHandle frProviderOH, FSFile *fsFile,
			   char *fname) {
	SysStatus rc = 0;
	fileInfo = fsFile;
	name = fname;
	clientCount = 0;
	stubFRP.setOH(frProviderOH);


	uval bSize,dSize;
	rc = stubFRP._open(dSize,bSize);
	_IF_FAILURE_RET(rc);

	rc = ServerFileBlock<StubFRPA>::init();
	if (_SUCCESS(rc)) {
	    CObjRootSingleRep::Create(this);
	}

	FileLinux::Stat status;
	rc = DREF(FileSystemVirtFS::FINF(fileInfo->getToken()))->
	    lockGetStatus(fileInfo->getToken(), &status);

	_IF_FAILURE_RET(rc);

	status.st_size = dSize;
	status.st_blksize = bSize;
	status.st_blocks = dSize/bSize;

	fileLength = dSize;

	rc = DREF(FileSystemVirtFS::FINF(fileInfo->getToken()))->
	    unlockPutStatus(fileInfo->getToken(), &status);

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
    static SysStatus Create(ServerFileRef &fref, FSFile *fsFile,
			    ObjectHandle pageServerOH,
			    char *fname) {
	SysStatus rc;
	ServerFileDev *file = new ServerFileDev;

	tassert((file != NULL),
		err_printf("failed allocate of ServerFileBlockRamFS\n"));

	rc = file->init(pageServerOH, fsFile, fname);
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
	err_printf("Closing %s\n",name);
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


FileSystemVirtFSRef DevFSNode::fs;

/* static */ void
DevFSNode::ClassInit(VPNum vp, FileSystemVirtFSRef fsRef) {
    if (vp) return;
    MetaDevFSNode::init();
    fs = fsRef;
}

/* virtual */
DevFSNode::~DevFSNode() {
    if (name) {
	freeGlobal(name,strlen(name)+1);
    }
}

/* virtual */ SysStatus
DevFSNode::init(char* fname, uval namelen, mode_t mode,
		ObjectHandle frProviderOH)
{
    SysStatus rc;
    name = (char*)allocGlobal(namelen+1);
    memcpy(name, fname, namelen);
    name[namelen] = 0;
    tassertMsg(strlen(name)==namelen,"Bad string length\n");

    stubFRP.setOH(frProviderOH);
    FileInfoVirtFS::init(mode);
    status.st_mode = mode;
    status.st_rdev = FileLinux::FILE;
    status.st_dev  = 76;
    rc = DREF(fs)->addFSNode(NULL, name, namelen, mode,
			     (FileInfoVirtFSRef)getRef());
    return rc;
}

/* virtual */ SysStatusUval
DevFSNode::getServerFileType(FileSystemGlobal::FileToken token) {
    return FileInfoVirtFS::VirtLocal;
}

/* virtual */ SysStatus
DevFSNode::getServerFile(FSFile *fsFile, ServerFileRef &fref)
{
    SysStatus rc;
    rc = ServerFileDev::Create(fref, fsFile, stubFRP.getOH(), name);
    _IF_FAILURE_RET(rc);
    sfRef = fref;
    return rc;
}

/* static */ SysStatus
DevFSNode::_createNode(__inbuf(namelen) char* name,
		       __in uval namelen,
		       __in mode_t mode,
		       __in ObjectHandle pageServer,
		       __out ObjectHandle &oh,
		       __CALLER_PID pid) //StubDevFSNode
{
    SysStatus rc;
    DevFSNode *dfn = new DevFSNode;

    rc = dfn->init(name, namelen, mode, pageServer);

    DevFSNodeRef dref = dfn->getRef();
    _IF_FAILURE_RET(rc);
    rc = DREF(dref)->giveAccessByServer(oh,pid);
    if (_FAILURE(rc)) {
	DREF(dref)->destroy();
    }
    return rc;
}

/* virtual */ SysStatus
DevFSNode::_deleteNode()
{
    return 0;
}
