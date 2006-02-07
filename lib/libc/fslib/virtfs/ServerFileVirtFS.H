#ifndef __SERVER_FILE_VIRT_FS_H_
#define __SERVER_FILE_VIRT_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileVirtFS.H,v 1.14 2004/10/02 19:57:55 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <fslib/ServerFile.H>
#include <stub/StubVirtFile.H>
#include <io/VirtFile.H>
#include <fslib/virtfs/FileInfoVirtFS.H>

#include <stub/StubFRPA.H>

class ServerFileVirtFS;
typedef ServerFileVirtFS** ServerFileVirtFSRef;

// A ServerFile in a virt FS must bind to a VirtFile object that
// provides the data.  This can be done in one of three ways:
// - via a VirtFileRef (ServerFileVirtFSLocal)
// - via a StubVirtFile** (ServerFileVirtFSRemote)
// - via a VirtFSFileRef (ServerFileVirtFSMerged)
//
// use templating to avoid code replication
//

class ServerFileVirtFS : public ServerFile {
protected:
    uval bufsize;
    uval maxIOLoad; // max length we can use for a stub invocation
    uval extraDataSize;
    uval token;
public:
    DEFINE_REFS(ServerFileVirtFS);
    static  SysStatus Create(ServerFileRef &fref, FileInfoVirtFS* fsFile,
			     uval extraData=0, uval tok = 0);

    //Extra data is the number of bytes to be reserved in ClientData
    //for provider-specific data.
    virtual SysStatus init(FileInfoVirtFS* finfo, uval extraData=0,
			   uval tok = 0);

    virtual SysStatus tryToDestroy();

    struct ClientData: public ServerFile::ClientData{
	uval token;
    public:
	inline void * operator new(size_t size, size_t extra) {
	    tassert(size==sizeof(ClientData),
		    err_printf("Wrong new called for ClientData\n"));
	    return AllocGlobalPadded::alloc(sizeof(ClientData));
	}
	inline void operator delete(void * p, uval size) {
	    AllocGlobalPadded::free(p, size+((ClientData*)p)->extraSize);
	}
	uval userData() {return (uval)extraData; };
	ClientData(uval size):extraSize(size) {};
	virtual ~ClientData() {
	    // breakpoint();
	};
    protected:
	uval extraSize;
	char  extraData[0];
    };

    // A kludge to type cheat XBaseObj clientData to (protected struct)
    // ClientData
    static ClientData *VClnt(XHandle xhandle) {
	return (ClientData *) (XHandleTrans::GetClientData(xhandle));
    }
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type);

    virtual SysStatus _lazyGiveAccess(__XHANDLE xhandle,
				      __in sval file, __in uval type,
				      __in sval closeChain,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen);

    // call from kernel to re-open this file, eventually change to giveAccess
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	return giveAccessByServer(oh, toProcID, match, nomatch);
    }

};

template <class T>
class ServerFileVirtFSTmp : public ServerFileVirtFS {
private:
    T** obj;
protected:
    ServerFileVirtFSTmp() {};
    virtual SysStatus init(FileInfoVirtFS *fsFile, T** ref,
			   uval extraData = 0, uval tok = 0);
public:

    virtual SysStatus open(uval oflag, ProcessID pid, ObjectHandle &oh,
			   uval &useType, TypeID &type);

    virtual SysStatus exportedXObjectListEmpty() { // last client gone away
	// nothing to do
	return 0;
    }
    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus getFROH(ObjectHandle &oh, ProcessID pid) {
	tassertMsg(0, "shouldn't be called\n");
	// this sould never be called
	return _SERROR(2141, 0, EINVAL);
    }
    virtual SysStatus noLinks(DirLinuxFSRef dirRef) {
	tassertMsg(0, "shouldn't be called\n");
	// this sould never be called
	return _SERROR(2142, 0, EINVAL);
    }
    virtual SysStatus tryToDestroy() {
	tassertMsg(0, "shouldn't be called\n");
	// this sould never be called
	return _SERROR(2143, 0, EINVAL);
    }

    // functions to support server FileLinux interface
    virtual SysStatus _flush() {
	/* nothing to do (actually current itfc FileLinuxVirtFile doesn't invoke
	 * this */
	return 0;
    }

    virtual SysStatusUval _setFilePosition(__in sval position, __in uval at,
					   __XHANDLE xhandle);

    virtual SysStatusUval _read(char *buf, uval len, __XHANDLE xhandle);

    virtual SysStatusUval _write(const char *buf, uval len, __XHANDLE xhandle);

    virtual SysStatusUval _getDents(char *buf, uval len, __XHANDLE xhandle) {
	// shouldn't be called for files
	return _SERROR(2144, 0, ENOSYS);
    }
};

class ServerFileVirtFSLocal : public ServerFileVirtFSTmp<VirtFile> {
    VirtFile *vfr;
public:
    DEFINE_GLOBAL_NEW(ServerFileVirtFSLocal);
    ServerFileVirtFSLocal():ServerFileVirtFSTmp<VirtFile>()
	{ /* empty body */ };
    virtual SysStatus init(FileInfoVirtFS *fsFile,
			   uval extraData = 0, uval tok = 0);

};

class ServerFileVirtFSRemote : public ServerFileVirtFSTmp<StubVirtFile> {
    StubVirtFile stub;
    StubVirtFile *stubPtr;

public:
    DEFINE_GLOBAL_NEW(ServerFileVirtFSRemote);
    ServerFileVirtFSRemote(): ServerFileVirtFSTmp<StubVirtFile>(),
			      stub(StubObj::UNINITIALIZED) { /* empty body */ };
    virtual SysStatus init(FileInfoVirtFS *fsFile,
			   uval extraData = 0, uval tok = 0);

};

class ServerFileVirtFSMerged : public ServerFileVirtFSTmp<VirtFSFile> {
    VirtFSFile* vfile;
public:
    DEFINE_GLOBAL_NEW(ServerFileVirtFSMerged);
    ServerFileVirtFSMerged(): ServerFileVirtFSTmp<VirtFSFile>() {};
    virtual SysStatus init(FileInfoVirtFS* finfo, uval extraData = 0,
			   uval tok = 0);
};



#endif /* #ifndef __SERVER_FILE_VIRT_FS_H_ */
