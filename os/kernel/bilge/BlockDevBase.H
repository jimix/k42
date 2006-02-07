#ifndef __BLOCK_DEV_BASE_H_
#define __BLOCK_DEV_BASE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockDevBase.H,v 1.45 2005/05/25 18:47:27 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Block Device Interface
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <stub/StubFRPA.H>
#include <meta/MetaFileLinuxServer.H>
#include <io/BlockDev.H>
#include <stub/StubCallBackObj.H>
#include "mem/Pin.H"

class BlockDevBase;
typedef BlockDevBase** BlockDevBaseRef;
class BlockDevBase : public BlockDev {
public:
#ifdef _IN_STUBGEN
    struct StubFRHolder;
#else /* #ifdef _IN_STUBGEN */
    struct StubFRHolder {
	StubFRPA stubFR;  // fr in memory manager for this file
	StubFRHolder() : stubFR(StubObj::UNINITIALIZED) { /* empty body */ }

	SysStatus init(ObjectHandle myOH, uval size);
	~StubFRHolder() {
	}
	DEFINE_GLOBAL_NEW(StubFRHolder);
    };
#endif /* #ifdef _IN_STUBGEN */
    StubFRHolder *stubFR;
    struct ClientData;
protected:
    typedef BLock LockType;
    LockType    lock;			// lock on object
    LockType	stubDetachLock;		// lock on FR stub
    uval devID;
    DEFINE_REFS(BlockDevBase);
    DEFINE_ACCESS_SERVER(FileLinuxServer,
			 MetaObj::controlAccess|MetaObj::read|MetaObj::write,
			 MetaObj::none);
    static const uval NOPRESSURE=~0ULL;

public:
    /* Defines shared memory transport area to send completion call backs to the
     * clients */
    class DiskTransportProducer;
    typedef DiskTransportProducer **DiskTransportProducerRef;
    /* Defines shared memory transport area to receive requests from clients;
     * only clients doing async requests use this */
    class DiskTransportConsumer;
    typedef DiskTransportConsumer **DiskTransportConsumerRef;
    // class maintains per-client state
    struct ClientData {
	StubFRHolder *stubFR;
	DiskTransportProducerRef ackTransportRef;
	DiskTransportConsumerRef reqTransportRef;
	union {
	    struct {
		ProcessRef pref;
		uval    count;			// opens
		uval    flags;			// open flags
	    };
	    // When calling server to register in devfs, we use this thread
	    // id to block on, _registerComplete will wake us.
	    struct {
		ThreadID	waitingThread;
		char*		name;
		dev_t		devID;
		mode_t		mode;
		ObjectHandle	parent;
		ObjectHandle	frProvider;
		uval		token;
		SysStatus	rc;
		ObjectHandle	replyOH;
	    };
	};
	uval addrMode;
	DEFINE_GLOBAL_NEW(ClientData);
	ClientData() {
	    ackTransportRef = NULL;
	    reqTransportRef = NULL;
	    flags = count = 0;
	    stubFR = NULL;
	    addrMode = BlockDev::None;
	}
	~ClientData();
    };

    virtual SysStatus asyncOp(uval type, uval physAddr, uval len,
			      uval objOffset, IOCompletion *ioc) {
	tassertMsg(0,"Object does not support async IO\n");
	return _SERROR(2287, 0, EOPNOTSUPP);
    }
    struct RemoteIOCompletion;
    struct RemoteReadIOCompletion;
    struct RemoteWriteIOCompletion;

protected:

    uval blkSize;
    uval devSize;
    friend struct RemoteIOCompletion;
    friend struct RemoteReadIOCompletion;
    friend struct RemoteWriteIOCompletion;
    virtual StubFRHolder* getFRStubHolder(__XHANDLE xhandle);

    virtual SysStatus fixupAddrPreWrite(PinnedMapping *pm,
					uval &addr, uval srcAddr, uval &len,
					ClientData *cd);
    virtual SysStatus fixupAddrPostWrite(PinnedMapping *pm,
					 uval addr, uval srcAddr, uval len,
					 ClientData *cd);
    virtual SysStatus fixupAddrPreRead(PinnedMapping *pm,
				       uval &addr, uval &dstAddr, uval &len,
				       ClientData *cd);
    virtual SysStatus fixupAddrPostRead(PinnedMapping *pm,
					uval addr, uval &dstAddr, uval len,
					ClientData *cd);

    virtual SysStatus IORequest(uval opType, uval srcAddr, uval size,
				uval objOffset, uval token, XHandle xh);

    virtual SysStatus devOpen() = 0;
public:
    virtual SysStatus getType(TypeID &id);
__xpublic:

    virtual SysStatus _registerCallback(__in ObjectHandle oh,
					__XHANDLE xhandle);

// FR interface
    virtual __async SysStatus _startWrite(__in uval virtAddr,
					  __in uval objOffset,
					  __in uval len,
					  __XHANDLE xhandle);

    virtual __async SysStatus _startFillPage(__in uval virtAddr,
					     __in uval objOffset,
					     __XHANDLE xhandle);

    virtual SysStatus _frIsNotInUse(__in uval fileToken);

    virtual __async SysStatus _startIO(__XHANDLE xhandle);

    // Block device interface extension

    // FIXME: get rid of these, can't have synchronous calls from kernel
    //        to server
    virtual SysStatus _write(__in uval srcAddr, __in uval objOffset,
			     __in uval len, __XHANDLE xhandle) = 0;
    virtual SysStatus _registerComplete(SysStatus rc, ObjectHandle oh,
					__XHANDLE xhandle);
    virtual SysStatus _getRegistration(__outbuf(*:buflen) char* name,
				       __in uval buflen,
				       __out uval &devID,
				       __out uval &mode,
				       __out ObjectHandle &parent,
				       __out uval &token,
				       __XHANDLE xhandle);
    virtual SysStatus _open(__out uval &devSize, __out uval &blkSize);

    // Specify a CallBackObj object handle, for callbacks to the
    // _callback2 method
    virtual SysStatus _useMode(__in uval addrMode, __in ObjectHandle oh,
			       __out ObjectHandle &transpFROH,
			       __XHANDLE xhandle, __CALLER_PID pid);

    virtual SysStatus _setRequestTransport(__in uval addrMode,
					   __in ObjectHandle oh,
					   __in uval transpSize,
					   __in uval transpEntryUvals,
					   __in uval transpNumEntries,
					   __XHANDLE xhandle, __CALLER_PID pid);

    virtual SysStatus _getBlock(__inout uval srcAddr, __inout uval len,
				__in uval objOffset, __XHANDLE xhandle)=0;

    virtual SysStatus _putBlock(__in uval srcAddr, __in uval size,
				__in uval objOffset, __XHANDLE xhandle);

    virtual SysStatus __async _IORequest(__in uval opType, __in uval srcAddr,
					 __in uval size, __in uval objOffset,
					 __in uval token, __XHANDLE xh) {
	return IORequest(opType, srcAddr, size, objOffset, token, xh);
    }

// ServerFile interface

    virtual SysStatus init(const char* name, int deviceID,
			   int mode, ObjectHandle dir,
			   ObjectHandle &deviceNode);

    static void ClassInit();
    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch, TypeID type = 0);

    static void BeingFreed(XHandle xhandle);

    virtual SysStatus handleXObjFree(XHandle xhandle) {
	XHandleTrans::SetBeingFreed(xhandle, BeingFreed);
	return 0;
    }

    // A kludge to type cheat XBaseObj clientData to (protected struct)
    // ClientData
    static ClientData *clnt(XHandle xhandle) {
	return (ClientData *) (XHandleTrans::GetClientData(xhandle));
    }
__xpublic:

    //FRProvider interface
    virtual SysStatus _getFROH(__out ObjectHandle &oh,
			       ProcessID pid, __XHANDLE xh);

    virtual SysStatusUval _ioctl(__in uval req,
				 __inoutbuf(size:size:size) char* buf,
				 __inout uval &size) {
	return _SERROR(2250, 0, EINVAL);
    }

    virtual SysStatus destroy();
    // call from kernel to re-open this file, eventually change to giveAccess
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	return _giveAccess(oh, toProcID, xhandle);
    }
};
#endif /* #ifndef __BLOCK_DEV_BASE_H_ */

