/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockDevBase.C,v 1.54 2005/08/30 19:08:40 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of base block-device interface
 * **************************************************************************/
#include <kernIncs.H>
#include <mem/PageAllocatorKernPinned.H>
#include "BlockDevBase.H"
#include <io/FileLinux.H>
#include <cobj/CObjRootSingleRep.H>
#include <sync/Lock.H>
#include <time.h>
#include <mem/FRPA.H>
#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <trace/traceDisk.h>

#include "mem/SharedBufferProducerKernel.H"
#include "mem/SharedBufferConsumerKernel.H"

#include <io/PAPageServer.H>
#include <meta/MetaPAPageServer.H>

#include <stub/StubFRPA.H>
#include <stub/StubDevFSBlk.H>
#include <stub/StubRegionFSComm.H>

#include <meta/MetaBlockDev.H>
#include <xobj/XBlockDev.H>

#define INSTNAME BlockDevBase
#include <meta/TplMetaPAPageServer.H>
#include <xobj/TplXPAPageServer.H>
#include <tmpl/TplXPAPageServer.I>

#include <meta/TplMetaFRProvider.H>
#include <xobj/TplXFRProvider.H>
#include <tmpl/TplXFRProvider.I>

typedef TplMetaPAPageServer<BlockDevBase> MetaPageServerBlock;
typedef TplXPAPageServer<BlockDevBase> XPageServerBlock;

typedef TplMetaFRProvider<BlockDevBase> MetaBlockFRProvider;
typedef TplXFRProvider<BlockDevBase> XBlockFRProvider;

class BlockDevBase::DiskTransportProducer : public SharedBufferProducerKernel {
protected:
    uval addrMode;
    StubCallBackObj stubCB;
    virtual SysStatus kickConsumer();
    virtual uval locked_isTransportFull() {
	_ASSERT_HELD(lock);
	uval avail = Avail(*pidx_ptr, *cidx_ptr,
			   BlockDev::TransportNumEntries);
	tassertMsg(avail > 0 && avail < BlockDev::TransportNumEntries,
		   "avail %ld\n", avail);
	//tassertWrn(avail != 1, "Transport for acks kernel disk obj full!\n");
	return (avail == 1);
    }
public:
    DEFINE_GLOBAL_NEW(DiskTransportProducer);
    DiskTransportProducer()
	: SharedBufferProducerKernel(BlockDev::TRANSPORT_SIZE,
				     BlockDev::TRANSPORT_ENTRY_UVALS,
				     BlockDev::TransportNumEntries),
	  addrMode(BlockDev::None),
	  stubCB(StubObj::UNINITIALIZED) {
#if 0
	err_printf("BlockDevBase::DiskTransportProducer instantiated "
		   "with size %ld, "
		   "number uvals per entry %ld, numEntries %ld\n",
		   BlockDev::TRANSPORT_SIZE,
		   BlockDev::TRANSPORT_ENTRY_UVALS,
		   BlockDev::TransportNumEntries);
#endif
    }
    virtual SysStatus init(ObjectHandle cboh, ProcessID pid,
			   uval addrMode,
			   ObjectHandle &sfroh);
    virtual SysStatus sendCallBack(uval err, uval token);
    static SysStatus Create(ObjectHandle coboh, ProcessID pid,
			    uval addrMode,
			    DiskTransportProducerRef &tref,
			    ObjectHandle &transpFROH);
};

/* virtual */ SysStatus
BlockDevBase::DiskTransportProducer::init(ObjectHandle cboh, ProcessID pid,
					  uval amode,
					  ObjectHandle &sfroh)
{
    stubCB.setOH(cboh);
    addrMode = amode;
    SharedBufferProducerKernel::init(pid, sfroh);
    return 0;
}

/* virtual */ SysStatus
BlockDevBase::DiskTransportProducer::kickConsumer()
{
    SysStatus rc;
    uval count = 0;
    uval delay = 10000;
    // FIXME: deal properly with EBUSY
    
    do {
	rc = stubCB._processCallBacks(addrMode);
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) != EBUSY) {
		break;
	    } else { // try again
		tassertWrn(0, "Delay in BlockDevBase kickConsumer\n");
		Scheduler::DelayMicrosecs(delay);
		delay *= 2;
		count++;
	    }
	}
    } while (_FAILURE(rc) && count < 20);

    passertMsg(_SUCCESS(rc) || _SGENCD(rc) != EBUSY, "EBUSY problem\n");

    return rc;
}

/* virtual */ SysStatus
BlockDevBase::DiskTransportProducer::sendCallBack(uval err, uval token)
{
    SysStatus rc;

    BlockDev::CallBackRequest req = {err, token};
    tassertMsg(sizeof(req) == BlockDev::TRANSPORT_ENTRY_UVALS*sizeof(uval),
	       "??");

    uval counter = 0;
    do {
	lock.acquire();
	rc = locked_tryPutRequest((uval*) &req);
	lock.release();
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == EBUSY) { // transport full
		TraceOSDiskAsyncSendCBDelay(10000);
		Scheduler::DelayMicrosecs(10000);
		counter++;
	    } else if (_SGENCD(rc) == EINVAL) { // destination gone
		break;
	    }
	}
    } while (_FAILURE(rc) && counter < 200);
    return rc;
}

/* static */ SysStatus
BlockDevBase::DiskTransportProducer::Create(ObjectHandle coh,
					    ProcessID pid,
					    uval addrMode,
					    DiskTransportProducerRef &tref,
					    ObjectHandle &transpFROH)
{
    SysStatus rc;
    DiskTransportProducer *obj = new DiskTransportProducer();
    rc = obj->init(coh, pid, addrMode, transpFROH);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    tref = (DiskTransportProducerRef)CObjRootSingleRep::Create(obj);

    return 0;
}

class BlockDevBase::DiskTransportConsumer : public SharedBufferConsumerKernel {
    uval transpSize;
    uval entryUvals;
    uval numEntries;
public:
    DEFINE_GLOBAL_NEW(DiskTransportConsumer);
    DiskTransportConsumer(uval sz, uval eu, uval ne) : transpSize(sz),
	entryUvals(eu), numEntries(ne) {}
    static SysStatus Create(ObjectHandle sfrOH, ProcessID pidProducer,
			    uval addrMode,
			    uval transpSize, uval transpEntryUvals,
			    uval transpNumEntries,
			    DiskTransportConsumerRef &cref);
    virtual SysStatus init(ObjectHandle sfrOH, ProcessID pidProducer) {
#if 0
	err_printf("BlockDevBase::DiskTransportConsumer initialized "
		   "with size %ld, "
		   "number uvals per entry %ld, numEntries %ld\n",
		   transpSize, entryUvals, numEntries);
#endif
	return SharedBufferConsumer::init(sfrOH, pidProducer, transpSize,
					  entryUvals, numEntries);
    }
    virtual SysStatus getRequest(BlockDev::DiskOpRequest *req) {
	SysStatus rc;
	lock.acquire();
	rc = locked_getRequest((uval *) req);
	lock.release();
	return rc;
    }

};

/* static */ SysStatus
BlockDevBase::DiskTransportConsumer::Create(ObjectHandle sfrOH, 
					    ProcessID pidProducer,
					    uval addrMode, uval transpSize,
					    uval transpEntryUvals,
					    uval transpNumEntries,
					    DiskTransportConsumerRef &cref)
{
    passertMsg(sizeof(BlockDev::DiskOpRequest) == transpEntryUvals*sizeof(uval),
	       "mistake in request size");

    SysStatus rc;
    DiskTransportConsumer *obj = new DiskTransportConsumer
	(transpSize, transpEntryUvals, transpNumEntries);
    rc = obj->init(sfrOH, pidProducer);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    obj->lock.acquire();
#if 0
    uval pidx = obj->locked_getPidx();
    uval cidx = obj->locked_getCidx();
    err_printf("In DiskTransportConsumer::Create got pidx %ld cidx %ld\n",
	       pidx, cidx);
#endif
    obj->lock.release();
    cref = (DiskTransportConsumerRef)CObjRootSingleRep::Create(obj);

    return 0;
}

BlockDevBase::ClientData::~ClientData()
{
    if (stubFR) delete stubFR; 
    if (ackTransportRef) DREF(ackTransportRef)->destroy();
    if (reqTransportRef) DREF(reqTransportRef)->destroy();

}

/* virtual */ BlockDevBase::StubFRHolder*
BlockDevBase::getFRStubHolder(__XHANDLE xhandle)
{
    ClientData *cd = clnt(xhandle);
    if (!cd->stubFR) {
	return stubFR;
    }
    return cd->stubFR;
}

/* This interface is part of the ServerFile interface, uses FR OHs */
/* virtual */ SysStatus
BlockDevBase::_registerCallback(__in ObjectHandle oh, __XHANDLE xhandle)
{
    ClientData *cd = clnt(xhandle);
    if (!cd->stubFR) {
	cd->stubFR = new StubFRHolder;
    }
    cd->stubFR->stubFR.setOH(oh);
    return 0;
}

SysStatus
BlockDevBase::StubFRHolder::init(ObjectHandle myOH, uval size)
{
    ObjectHandle oh;
    SysStatus rc;

    /* We added name information in the interface to make dubugging FCMs
     * and FRs easier, but we don't need them here. Also, fileToken is
     * not relevant */
    char foo;
    rc = FRPA::_Create(oh, _KERNEL_PID, myOH, size, 0 /* fileToken */,
		       &foo, 0);
    tassert( _SUCCESS(rc), err_printf("woops\n"));
    stubFR.setOH(oh);

    // err_printf("constructing StubFRHolder %p\n", this);
    return 0;
};

void
BlockDevBase::ClassInit()
{
    MetaPageServerBlock::init();
    MetaBlockFRProvider::init();
    MetaBlockDev::init();
}

/* virtual */ SysStatus
BlockDevBase::_registerComplete(SysStatus rc, ObjectHandle oh,
				__XHANDLE xhandle)
{
    ClientData *cd = clnt(xhandle);
    ThreadID t = cd->waitingThread;
    cd->rc = rc;
    cd->replyOH = oh;
    cd->waitingThread = Scheduler::NullThreadID;
    Scheduler::Unblock(t);
    return 0;
}

/* virtual */ SysStatus
BlockDevBase::_getRegistration(__outbuf(*:buflen) char* name,
			       __in uval buflen,
			       __out uval &devID,
			       __out uval &mode,
			       __out ObjectHandle &parent,
			       __out uval &token,
			       __XHANDLE xhandle)
{
    ClientData *cd = clnt(xhandle);
    strncpy(name,cd->name, buflen-1);
    devID = cd->devID;
    mode = cd->mode;
    parent = cd->parent;
    token = cd->token;
    return 0;
}

SysStatus
BlockDevBase::init(const char* name, int deviceID,
		   int mode, ObjectHandle dir, ObjectHandle &deviceNode)

{
    devID = deviceID;
    devSize = blkSize = 0;
    stubFR  = NULL;
    SysStatus rc =0;
    lock.init();
    stubDetachLock.init();
    CObjRootSingleRep::Create(this);

    ObjectHandle devfsServer;
    rc = DREFGOBJ(TheTypeMgrRef)->getTypeHdlr(StubDevFSBlk::typeID(),
					      devfsServer);

    _IF_FAILURE_RET(rc);
    ObjectHandle pageOH;
    rc = DREF(getRef())->giveAccessByServer(pageOH,devfsServer.pid(),
					    (AccessRights)~0ULL,
					    MetaObj::none,
					    MetaBlockFRProvider::typeID());

    _IF_FAILURE_RET(rc);

    const mode_t perm= S_IFBLK|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
    StubDevFSBlk devStub(StubBaseObj::UNINITIALIZED);

    ClientData *cd = clnt(pageOH.xhandle());
    cd->waitingThread = Scheduler::GetCurThread();
    cd->devID = devID;
    cd->name = (char*)name;
    cd->mode = perm;
    cd->parent = dir;
    cd->frProvider = pageOH;

    devStub._KickCreateNode(pageOH);
    while (cd->waitingThread!=Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    rc = cd->rc;
    tassertMsg(_SUCCESS(cd->rc), "CreateNode failure: %lx\n", rc);

    if (_SUCCESS(rc)) {
	devfsServer = cd->replyOH;
	deviceNode = devfsServer;
    }
    return rc;
}

/* virtual */ SysStatus
BlockDevBase::_useMode(__in uval addrMode, __in ObjectHandle oh,
		       __out ObjectHandle &transpFROH,
		       __XHANDLE xhandle, __CALLER_PID pid)
{
    AutoLock<LockType> al(&lock);
    ClientData *cd = clnt(xhandle);

    //FIXME: add a permissions mechanism here ---
    //       not everybody can switch to Physical mode
    if (pid > 0xb && addrMode == BlockDev::Physical) {
	tassertWrn(0, "BlockDevBase::_useMode denying useMode BlockDev::"
		   "Physical\n");
	return _SERROR(1980, 0, EPERM);
    }

    SysStatus rc = DiskTransportProducer::Create(oh, pid, addrMode,
						 cd->ackTransportRef,
						 transpFROH);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    cd->addrMode = addrMode;
    return 0;
}

/* virtual */ SysStatus
BlockDevBase::_setRequestTransport(__in uval addrMode, __in ObjectHandle sfrOH,
				    __in uval transpSize,
				   __in uval transpEntryUvals,
				   __in uval transpNumEntries,
				   __XHANDLE xhandle, __CALLER_PID pid)
{
    AutoLock<LockType> al(&lock);
    ClientData *cd = clnt(xhandle);

    SysStatus rc = DiskTransportConsumer::Create(
	sfrOH, pid, addrMode, transpSize, transpEntryUvals, transpNumEntries,
	cd->reqTransportRef);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    return 0;
}

/* Consume requests in the SharedBuffer area */
/* virtual __async */ SysStatus
BlockDevBase::_startIO(__XHANDLE xh)
{
    SysStatus rcget, rc = 0;
    ClientData *cd = clnt(xh);

    if (cd->reqTransportRef == NULL) { // transport not initialized
	passertMsg(0, "weird\n");
	return _SERROR(2921, 0, 0);
    }

    do {
	BlockDev::DiskOpRequest req;
	rcget = DREF(cd->reqTransportRef)->getRequest(&req);

	if (_SUCCESS(rcget)) {
	    rc = IORequest(req.type, req.addr, req.size,
			   req.offset, req.token, xh);
	}
    } while _SUCCESS(rcget);

    return rc;
}

/* virtual */ SysStatus
BlockDevBase::_open(__out uval &dSize, __out uval &bSize)
{
    SysStatus rc = 0;
    stubDetachLock.acquire();

    //devOpen() will set devSize and blkSize
    if (devSize==0) {
	rc = devOpen();
    }
    dSize = devSize;
    bSize = blkSize;
    stubDetachLock.release();
    return rc;

}

/*virtual*/ SysStatus
BlockDevBase::_getFROH(__out ObjectHandle &oh,
		       ProcessID pid, __XHANDLE xh)
{
    SysStatus rc=0;
    stubDetachLock.acquire();
    tassertMsg(devSize,
	       "Must do an _open on a block device before a _getFROH\n");
    if (!stubFR) {

	ObjectHandle pageOH;
	if (devSize==0) {
	    rc = devOpen();
	    if (_FAILURE(rc)) goto abort;
	}

	rc = DREF(getRef())->
	    giveAccessByServer(pageOH,_KERNEL_PID,
			       MetaPageServerBlock::typeID());

	if (_FAILURE(rc)) goto abort;

	ClientData *cd = clnt(pageOH.xhandle());
	cd->addrMode = BlockDev::Physical;
	stubFR = new StubFRHolder();
	stubFR->init(pageOH, devSize);
    }
    rc = stubFR->stubFR._giveAccess(oh, pid,
				    MetaFR::fileSystemAccess|
				    MetaObj::controlAccess|MetaObj::attach,
				    MetaObj::none);
  abort:
    stubDetachLock.release();
    return rc;

}

/* virtual */ SysStatus
BlockDevBase::_frIsNotInUse(uval fileToken) {
    SysStatus rc;
    /* destroy can't be invoked locked, since destroy may block for
     * i/o. stubDetachLock prevents more than one detach
     * from messing with stub at the same time
     * we must aquire it before the regular lock, since
     * while holding it, we do a destroyIfNotInuse which may
     * block in the FR waiting for the file system to complete some IO
     * and completing IO needs the regular lock.
     */
    stubDetachLock.acquire();
    if (stubFR == NULL) {
	stubDetachLock.release();
	return 1;
    } else {
	XHandle xh;
	rc = stubFR->stubFR._destroyIfNotInUse(xh);
	tassertMsg(_SUCCESS(rc)||_ISDELETED(rc),
		   "not dealing with error here yet\n");
	// non zero returns happen if either destroy can't be done
	// or has been done.  Remember, we don't hold the lock!
	if (_SGETUVAL(rc) != 0) {
	    stubDetachLock.release();
	    return 0;
	} else {
	    /* We'll invoke releaseAccess(), which triggers a method that
	     * if stubFR is available, invokes _remove. Since we're in
	     * the middle of doing detachFR, we don't want to invoke stubFR's
	     * remove
	     */
	    StubFRHolder *stubTmp = stubFR;
	    stubFR = NULL;
	    if (stubTmp) {
		rc = releaseAccess(xh);
		tassert(_SUCCESS(rc), err_printf("remove access failed\n"));
		delete stubTmp;
	    }
	    stubDetachLock.release();
	    return 1;
	}
    }
};

/* virtual */ SysStatus
BlockDevBase::getType(TypeID &id)
{
    id = MetaBlockDev::typeID();
    return 0;
}

/* virtual */ SysStatus
BlockDevBase::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
				      AccessRights match, AccessRights nomatch,
				      TypeID type)
{
    SysStatus retvalue;
    ClientData *clientData = new ClientData();

    BaseProcessRef bpref;
    retvalue = DREFGOBJ(TheProcessSetRef)->getRefFromPID(toProcID,bpref);
    _IF_FAILURE_RET(retvalue);

    clientData->pref = (ProcessRef)bpref;

    if (type==0 || MetaPageServerBlock::isChildOf(type)) {
	clientData->addrMode = BlockDev::Physical;
	type = MetaPageServerBlock::typeID();
    } else {
	clientData->addrMode = BlockDev::None;
    }

    SysStatusUval ptr = DREFGOBJ(TheTypeMgrRef)->getTypeLocalPtr(type);
    if (ptr == 0) return _SERROR(2797, 0, EAGAIN);

    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
			      type, (uval)clientData);
    return (retvalue);
}

/*static*/ void
BlockDevBase::BeingFreed(XHandle xhandle)
{
    ClientData *clientData;
    clientData = (ClientData*)(XHandleTrans::GetClientData(xhandle));
    delete clientData;
}

/*virtual*/ SysStatus
BlockDevBase::destroy()
{
    SysStatus rc;
    XHandle xh;

    if (stubFR) {
	stubFR->stubFR._fsync();

	rc = stubFR->stubFR._destroyIfNotInUse(xh);
	passert(rc != 1, err_printf("marc is an idiot\n"));
	delete stubFR;
	stubFR = NULL;

	rc = releaseAccess(xh);
	tassert(_SUCCESS(rc), err_printf("remove access failed\n"));
    }
    return 0;
}

/*virtual*/ SysStatus
BlockDevBase::_startWrite(__in uval srcAddr,
			  __in uval objOffset,
			  __in uval len,
			  __XHANDLE xhandle)
{
    SysStatus rc = _write(srcAddr, objOffset, len, xhandle);
    // This interface is for FR's only
    rc = getFRStubHolder(xhandle)->stubFR._ioComplete(srcAddr, objOffset,rc);
    tassertMsg(_SUCCESS(rc),"putPageComplete failed: %016lx\n",rc);
    return 0;
}

/*virtual*/ SysStatus
BlockDevBase::_startFillPage(__in uval srcAddr,
			     __in uval objOffset,
			     __XHANDLE xhandle)
{
    uval len = PAGE_SIZE;

    SysStatus rc = _getBlock(srcAddr, len, objOffset, xhandle);

    if (_SUCCESS(rc) && len <  PAGE_SIZE) {
	rc = _SERROR(1983, 0, ENOMEM);
    }

    // This interface is for FR's only
    rc = getFRStubHolder(xhandle)->stubFR._ioComplete(srcAddr, objOffset, rc);

    tassertMsg(_SUCCESS(rc),"fillPageComplete failed: %016lx\n",rc);
    return 0;
}

/* virtual */ SysStatus
BlockDevBase::_putBlock(__in uval srcAddr, __in uval size,
			 __in uval objOffset, __XHANDLE xhandle)
{
    return _write(srcAddr, objOffset, size, xhandle);
}

struct BlockDevBase::RemoteIOCompletion :
    public BlockDevBase::IOCompletion {
    XHandle xh;
    uval token;
    uval offset, addr, srcAddr, size;
    PinnedMapping *pm;

    DEFINE_PINNEDGLOBALPADDED_NEW(RemoteIOCompletion);
    RemoteIOCompletion(uval _offset, uval _addr, uval _srcAddr,
		       uval _size, uval _token, XHandle _xh,
		       PinnedMapping *_pm):
	xh(_xh), token(_token), offset(_offset),
	srcAddr(_srcAddr), size(_size), pm(_pm) {};

    virtual void complete(BlockDevRef ref, SysStatus err) {

	SysStatus rc;
	ClientData *cd = BlockDevBase::clnt(xh);
	if (cd->stubFR) {
	    do {
		rc = cd->stubFR->stubFR._ioComplete(offset, srcAddr, err);
		if (_FAILURE(rc)) {
		    /* if not a problem with the async msg buffer full, let's
		     * examine the failure for now */
		    tassertMsg(_SGENCD(rc) == EBUSY, "rc is 0x%lx\n", rc);
		    Scheduler::DelayMicrosecs(10);
		}
	    } while (_FAILURE(rc));
	} else { // invoke call back object using transport
	    rc = DREF(cd->ackTransportRef)->sendCallBack(err, token);
	    tassertWrn(_SUCCESS(rc), "sendCallback returned rc 0x%lx\n", rc);
	}
	delete this;
    }
    virtual ~RemoteIOCompletion() { delete pm; };
};

struct BlockDevBase::RemoteReadIOCompletion :
    public BlockDevBase::RemoteIOCompletion {
    RemoteReadIOCompletion(uval _offset, uval _addr, uval _srcAddr,
			   uval _size, uval _token, XHandle _xh,
			   PinnedMapping *_pm):
	RemoteIOCompletion(_offset, _addr, _srcAddr,
	    _size, _token, _xh, _pm) {}
    virtual void complete(BlockDevRef ref, SysStatus err) {
	ClientData *cd = BlockDevBase::clnt(xh);
	SysStatus rc;
	rc = DREF((BlockDevBaseRef)ref)->fixupAddrPostRead(pm, addr, srcAddr,
							   size, cd);
	tassertMsg(_SUCCESS(rc),"RemoteReadIOCompletion failure: %lx\n",rc);
	RemoteIOCompletion::complete(ref, err);
    }

    virtual ~RemoteReadIOCompletion() {};
};

struct BlockDevBase::RemoteWriteIOCompletion :
    public BlockDevBase::RemoteIOCompletion {
    DEFINE_PINNEDGLOBALPADDED_NEW(RemoteWriteIOCompletion);
    RemoteWriteIOCompletion(uval _offset, uval _addr, uval _srcAddr,
			    uval _size, uval _token, XHandle _xh,
			    PinnedMapping *_pm):
	RemoteIOCompletion(_offset, _addr, _srcAddr,
	    _size, _token, _xh, _pm) {}
    virtual void complete(BlockDevRef ref, SysStatus err) {
	ClientData *cd = BlockDevBase::clnt(xh);
	SysStatus rc;
	rc = DREF((BlockDevBaseRef)ref)->fixupAddrPostWrite(pm, addr, srcAddr,
							    size, cd);
	tassertMsg(_SUCCESS(rc),"RemoteReadIOCompletion failure: %lx\n",rc);
	RemoteIOCompletion::complete(ref,err);
    }

    virtual ~RemoteWriteIOCompletion() {};
};

/* virtual */ SysStatus
BlockDevBase::IORequest(uval opType, uval srcAddr, uval size,
			uval objOffset, uval token, XHandle xh)
{
    TraceOSDiskIORequest(srcAddr, objOffset);

    uval addr;
    SysStatus rc = 0;
    ClientData *cd = clnt(xh);
    PinnedMapping *pm = new PinnedMapping();
    IOCompletion *ioc = NULL;
    switch (opType) {
    case DevRead:
	rc = fixupAddrPreRead(pm, addr, srcAddr, size, cd);
	if (_SUCCESS(rc)) {
	    ioc = new RemoteReadIOCompletion(objOffset, addr, srcAddr,
					     size, token, xh, pm);
	}
	break;
    case DevWrite:
	rc = fixupAddrPreWrite(pm, addr, srcAddr, size, cd);
	if (_SUCCESS(rc)) {
	    ioc = new RemoteWriteIOCompletion(objOffset, addr, srcAddr,
					      size, token, xh, pm);
	}
	break;
	}
    if (_FAILURE(rc)) {
	tassertWrn(_SUCCESS(rc),"Bad pre-fixup: %lx\n",rc);
	delete pm;
	return rc;
    }
    asyncOp(opType, addr, size, objOffset, ioc);
    return 0;
}

// We have several potential raddressing modes.  When we receive a call
// to perform an IO operation we have to take the address given to us,
// figure out the addressing mode used and convert the address into a
// virtual, pinned address in our address space.  The methods below do
// all the nasty work to do all of that.

/*virtual*/ SysStatus
BlockDevBase::fixupAddrPreWrite(PinnedMapping *pm,
				uval &addr, uval srcAddr, uval &len,
				ClientData *cd)
{
    SysStatus rc = 0;
    // FIXME: must we do a read in case of somebody trying to do a
    // partial block write?
    if (len%blkSize) {
	len += blkSize - len%blkSize;
    }

    tassertMsg((srcAddr & 0xfff) == 0, "value is 0x%lx\n",
	       srcAddr&0xfff);

    switch (cd->addrMode) {
    case BlockDev::Virtual:
	//Must be on one page only
	if (((srcAddr+len-1) & ~(PAGE_SIZE-1))!= (srcAddr & ~(PAGE_SIZE-1))) {
	    return _SERROR(2488, 0, EINVAL);
	}
	len = PAGE_SIZE;
	rc = pm->pinAddr(cd->pref, srcAddr, addr, 0,
			 PinnedMapping::FullPagesOnly);
	passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	_IF_FAILURE_RET(rc);
	break;
    case BlockDev::Physical:
	//FIXME: we are assuming a pinned address here
	//       this is fine right now, because we know we are in
	//       the kernel adn so is the caller (because they could
	//       set cd->addrMode to BlockDev::Physical

	addr = PageAllocatorKernPinned::realToVirt(srcAddr);
	break;
    case BlockDev::None:
    default:
	tassertMsg(0, "%s used in invalid addressing mode\n",__func__);
    }
    return 0;
}

/*virtual*/ SysStatus
BlockDevBase::fixupAddrPreRead(PinnedMapping *pm,
			       uval &addr, uval &dstAddr, uval &len,
			       ClientData *cd)
{
    SysStatus rc = 0;
    switch (cd->addrMode) {
    case BlockDev::Virtual:
	//Must be on one page only
	if (((dstAddr+len-1) & ~(PAGE_SIZE-1))!= (dstAddr & ~(PAGE_SIZE-1))) {
	    return _SERROR(2251, 0, EINVAL);
	}
	len = PAGE_SIZE;
	rc = pm->pinAddr(cd->pref, dstAddr, addr, 1,
			    PinnedMapping::FullPagesOnly);

	break;
    case BlockDev::Physical:
	//FIXME: we are assuming a pinned address here
	//       this is fine right now, because we know we are in
	//       the kernel adn so is the caller (because they could
	//       set cd->addrMode to BlockDev::Physical

	addr = (uval) PageAllocatorKernPinned::realToVirt(dstAddr);
	break;
    case BlockDev::None:
    default:
	tassertMsg(0, "%s used in invalid addressing mode\n",__func__);
    }
    return rc;
}

/*virtual*/ SysStatus
BlockDevBase::fixupAddrPostWrite(PinnedMapping *pm,
				 uval addr, uval srcAddr, uval len,
				 ClientData *cd)
{
    switch (cd->addrMode) {
    case BlockDev::Virtual:
	pm->unpin();
	break;
    }
    return 0;
}

/*virtual*/ SysStatus
BlockDevBase::fixupAddrPostRead(PinnedMapping *pm,
				uval addr, uval &dstAddr, uval len,
				ClientData *cd)
{
    switch (cd->addrMode) {
    case BlockDev::Virtual:
	pm->unpin();
	break;
    }
    return 0;
}
