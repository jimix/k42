/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClientAsync.C,v 1.21 2005/08/30 19:06:50 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Toy Disk class implementation
 * **************************************************************************/

#define USING_SHARED_MEM_TRANSPORT_FOR_REQS

#include <sys/sysIncs.H>
#include "DiskClientAsync.H"
#include <io/BlockDev.H>
#include <misc/SharedBufferProducerUser.H>
#include <cobj/CObjRootSingleRep.H>

#include <trace/traceDisk.h>

/* Defines shared memory buffer to pass i/o requests to the disk entity
 * in the kernel. We used to send the requests by async PPCs, but the
 * maximum number of async PPCs was not large enough to handle the
 * demand */
class DiskClientAsync::DiskTransportProducer : public SharedBufferProducerUser {
public:
    /* definitions required by the shared memory transport */
    static const uval TRANSP_SIZE = 4*PAGE_SIZE; /* allocate pages
						*  for the transport */
    static const uval TRANSP_ENTRY_UVALS = 5;  /* communication involves 
						* 5 uvals: type, addr, sz, offset,
						* continuation */
    static const uval TRANSP_NUM_ENTRIES = (TRANSP_SIZE-2*sizeof(uval))
	/(TRANSP_ENTRY_UVALS*sizeof(uval));

protected:

    StubBlockDev sb;
    virtual SysStatus kickConsumer() { 
	SysStatus rc = sb._startIO();
	tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	return 0;
    } 
    virtual uval locked_isTransportFull() {
	/* FIXME: we want to limit in terms of outstanding requests,
	 * something like return (outstanding == numEntries - 1) */
	_ASSERT_HELD(lock);
	uval avail = Avail(*pidx_ptr, *cidx_ptr, TRANSP_NUM_ENTRIES);
	tassertMsg(avail > 0 && avail < TRANSP_NUM_ENTRIES,
		   "avail %ld\n", avail);
	//tassertWrn(avail != 1, "SharedBufferProducer to send io reqs full!\n");
	return (avail == 1);
    }

public:
    DEFINE_GLOBAL_NEW(DiskTransportProducer);
    DiskTransportProducer(ObjectHandle oh)
	: SharedBufferProducerUser(TRANSP_SIZE, TRANSP_ENTRY_UVALS,
				   TRANSP_NUM_ENTRIES),
	  sb(StubObj::UNINITIALIZED) {
	passertMsg(sizeof(BlockDev::DiskOpRequest) 
		   == TRANSP_ENTRY_UVALS*sizeof(uval),
		   "mistake in request size");
#if 0
	err_printf("DiskClientAsync::DiskTransportProducer instantiated with "
		   " %ld entries\n", TRANSP_NUM_ENTRIES);
#endif
	sb.setOH(oh);
    }

    virtual SysStatus init(ObjectHandle &sfroh) {
	return SharedBufferProducerUser::init(sfroh);
    }

    virtual SysStatus sendRequest(uval type, uval addr, uval sz, uval offset,
				  uval continuation, uval tryOp = 0) {
	BlockDev::DiskOpRequest req = {type, addr, sz, offset, continuation};
	SysStatus rc;
	do {
	    lock.acquire();
	    rc = locked_tryPutRequest((uval*) &req, 0 /* not highPriority */);
	    lock.release();
	    if (_FAILURE(rc) && _SGENCD(rc) == EBUSY) {
		// transport was full
		if (tryOp) {
		    return _SERROR(2931, 0, EBUSY);
		} else {
		    TraceOSDiskAsyncSendReqToKernelDelay(10000);
		    Scheduler::DelayMicrosecs(10000);
		}
	    } else {
		tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
		break;
	    }
	} while (_FAILURE(rc));

	return rc;
    }
};

SysStatus
DiskClientAsync::initTransport(uval needPhysAddr)
{
    DiskTransportProducer *dtvirt, *dtphys;
    ObjectHandle frohv, frohp;

    dtvirt = new DiskTransportProducer(sbd.getOH());
    SysStatus rc = dtvirt->init(frohv);
    if (_FAILURE(rc)) { goto failure_virt; }

    /* pass oh to the FR for this shared transport to the disk object 
     * in the kernel */
    rc = sbd._setRequestTransport(BlockDev::Virtual, frohv,
				  DiskTransportProducer::TRANSP_SIZE,
				  DiskTransportProducer::TRANSP_ENTRY_UVALS,
				  DiskTransportProducer::TRANSP_NUM_ENTRIES);
    if (_FAILURE(rc)) { goto failure_virt; }

    diskTransportVirt = (DiskTransportProducerRef)
	CObjRootSingleRep::Create(dtvirt);

    if (needPhysAddr) {  // same thing, now to object dealing with phys addr
	dtphys = new DiskTransportProducer(sbdPhys.getOH());
	rc = dtphys->init(frohp);
	if (_FAILURE(rc)) { goto failure_phys; }
	rc = sbdPhys._setRequestTransport
	    (BlockDev::Physical, frohp,
	     DiskTransportProducer::TRANSP_SIZE,
	     DiskTransportProducer::TRANSP_ENTRY_UVALS,
	     DiskTransportProducer::TRANSP_NUM_ENTRIES);

	if (_FAILURE(rc)) { goto failure_phys; }
	diskTransportPhys = (DiskTransportProducerRef)
		CObjRootSingleRep::Create(dtphys);
    }


    return 0;

  failure_virt:
    tassertMsg(0, "rc %lx\n", rc);
    delete dtvirt;
    return rc;
    
  failure_phys:
    tassertMsg(0, "rc %lx\n", rc);
    delete dtvirt;
    delete dtphys;
    return rc;
}

SysStatus
DiskClientAsync::asyncRequest(StubBlockDev &stub,
			      uval type, uval addr, uval sz, uval offset,
			      uval cont)
{
    SysStatus rc;

    do {
	rc = stub._IORequest(type, addr, sz, offset, (uval)cont);
	if (_FAILURE(rc)) {
	    // retry only if EBUSY
	    if (_SGENCD(rc) == EBUSY) {
		Scheduler::DelayMicrosecs(10000);
	    } else {
		tassertWrn(0, "DiskClientAsync::asyncRequest: error calling "
			   "_IORequest rc 0x%lx\n", rc);
		break;
	    }
	}
    } while (_FAILURE(rc));

    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    return rc;
}

SysStatus
DiskClientAsync::aReadBlock(uval blockNumber, void *block, uval cont)
{
    TraceOSDiskAsyncRead((uval) this, blockNumber, cont);

    uval offset = blockNumber * BLOCKSIZE;
    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);

    if ((BLOCKSIZE-1) & (uval)block) {
	passertMsg(0, "%s: Not yet supporting unaligned block read: %p(%ld)\n",
		   __PRETTY_FUNCTION__, block, blockNumber);
	return _SERROR(2838, 0, EINVAL);
    }

#ifdef USING_SHARED_MEM_TRANSPORT_FOR_REQS
    return DREF(diskTransportVirt)->sendRequest
	(BlockDev::DevRead, (uval)block, BLOCKSIZE, offset, cont);
#else    
    return asyncRequest(sbd, BlockDev::DevRead, (uval)block, BLOCKSIZE,
			offset, cont);
#endif
}

SysStatus
DiskClientAsync::aWriteBlockInternal(uval blockNumber, void *block, uval cont,
				     uval tryOp /* = 0 */)
{
    uval offset = blockNumber * BLOCKSIZE;
    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);

    if ((BLOCKSIZE-1) & (uval)block) {
	passertMsg(0, "%s: Not yet supporting unaligned block read: %p(%ld)\n",
		   __PRETTY_FUNCTION__, block, blockNumber);
	return _SERROR(2839, 0, EINVAL);
    }

    TraceOSDiskAsyncWrite((uval) this, blockNumber, cont);

#ifdef USING_SHARED_MEM_TRANSPORT_FOR_REQS
    SysStatus rc;
    rc =  DREF(diskTransportVirt)->sendRequest
	(BlockDev::DevWrite,(uval)block, BLOCKSIZE, offset, cont, tryOp);
    if (_FAILURE(rc)) {
	TraceOSDiskTryAsyncWriteFail((uval) this, blockNumber, cont);
    }
    return rc;
#else
    return asyncRequest(sbd, BlockDev::DevWrite,(uval)block, BLOCKSIZE,
			offset, cont);
#endif
}

SysStatus
DiskClientAsync::aReadBlockPhys(uval blockNumber, uval paddr, uval cont)
{
    TraceOSDiskAsyncReadPhys((uval) this, blockNumber, paddr, cont);

    tassertMsg(diskTransportPhys != NULL, "need diskTransportPhys (ref %p)\n",
	       getRef());

    uval offset = blockNumber * BLOCKSIZE;
    uval sz = BLOCKSIZE;

    if (sbdPhys.getOH().invalid()) {
	return _SERROR(2840, 0, EPERM);
    }

    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);

    passertMsg(!((BLOCKSIZE-1) & paddr),
	       "%s: Unaligned phys block read: %lx\n", __PRETTY_FUNCTION__,
	       paddr);

#ifdef USING_SHARED_MEM_TRANSPORT_FOR_REQS
    return DREF(diskTransportPhys)->sendRequest
	(BlockDev::DevRead, paddr, sz, offset, cont);
#else
    return asyncRequest(sbdPhys, BlockDev::DevRead, paddr, sz, offset,
			cont);
#endif
}

SysStatus
DiskClientAsync::aWriteBlockPhys(uval blockNumber, uval paddr, uval cont)
{
    TraceOSDiskAsyncWritePhys((uval) this, blockNumber, paddr, cont);

    tassertMsg(diskTransportPhys != NULL, "need diskTransportPhys\n");
    uval offset = blockNumber * BLOCKSIZE;
    uval sz = BLOCKSIZE;

    if (sbdPhys.getOH().invalid()) {
	return _SERROR(2841, 0, EPERM);
    }

    tassertMsg(blockNumber < numBlocks, "Bad disk op: %ld\n",blockNumber);
    tassertMsg(!((BLOCKSIZE-1) & paddr),
	       "%s: Unaligned phys block write: %lx\n",
	       __PRETTY_FUNCTION__, paddr);

#ifdef USING_SHARED_MEM_TRANSPORT_FOR_REQS
    return DREF(diskTransportPhys)->sendRequest
	(BlockDev::DevWrite, paddr, sz, offset, cont);
#else
    return asyncRequest(sbdPhys, BlockDev::DevWrite, paddr, sz, offset,
			cont);
#endif
}

/* virtual*/ SysStatus
DiskClientAsync::init(char* blockDev, uval needPhysAddr /* = 1 */)
{
    SysStatus rc;

    rc = DiskClient::init(blockDev, needPhysAddr);
    _IF_FAILURE_RET(rc);

    /* create and initialize shared buffer to be used for transporting
     * requests to the disk object in the kernel */
    rc = initTransport(needPhysAddr);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    return rc;
}

/* static */ SysStatus
DiskClientAsync::Create(DiskClientAsyncRef &dcr, char* blockDev,
			CallBackOp cbf, uval needPhysAddr /* = 1 */)
{
    DiskClientAsync *dc = new DiskClientAsync(cbf);

    SysStatus rc = dc->init(blockDev, needPhysAddr);
    if (_FAILURE(rc)) {
	delete dc;
	return rc;
    }

    dcr = dc->getRef();

    return 0;
}


/* virtual __async */ SysStatusUval
DiskClientAsync::_processCallBacks(uval arg)
{
    TraceOSDiskAsyncProcessCallBacks((uval) this, arg);

    DiskTransportConsumer *transp = NULL;
    switch (arg) {
    case BlockDev::Physical:
	transp = transportPhys;
	break;
    case BlockDev::Virtual:
	transp = transportVirt;
	break;
    default:
	passertMsg(0, "bug\n");
    }

    tassertMsg(transp != NULL, "?");
    SysStatus rc;
    uval token, err;
    do {
	rc = transp->getRequest(token, err);
	if (_SUCCESS(rc)) {
#ifdef CREATE_THREAD_FOR_CALLBACK // This was a experiment; we don't run with this
	    // FIXME: we can make things lighter than this!
	    RunCallBackArg *arg = new RunCallBackArg(cb, err, token);
	    Scheduler::ScheduleFunction(&RunCallBack, (uval) arg);
#else
	    TraceOSDiskAsyncCB((uval) this, err, token);
	    cb(err, token);
#endif //#ifdef CREATE_THREAD_FOR_CALLBACK
	} else {
	    //err_printf("transport now empty\n");
	}
    } while (_SUCCESS(rc));
    return 0;

}

#ifdef CREATE_THREAD_FOR_CALLBACK // This was a experiment; we don't run with this
void
DiskClientAsync::RunCallBack(uval arg)
{
    RunCallBackArg *rarg = (RunCallBackArg*) arg;
    (rarg->cb)(rarg->err, rarg->token);
    delete rarg;
    err_printf("-");

}
#endif // #ifdef CREATE_THREAD_FOR_CALLBACK
