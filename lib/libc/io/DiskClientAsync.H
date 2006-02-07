#ifndef __DISK_CLIENT_ASYNC_H_
#define __DISK_CLIENT_ASYNC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClientAsync.H,v 1.7 2005/08/29 20:19:10 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to Disk Objects offering only async operations
 * **************************************************************************/

#include "DiskClient.H"

class DiskClientAsync;
typedef DiskClientAsync **DiskClientAsyncRef;

class DiskClientAsync : public DiskClient {
protected:
    typedef void (*CallBackOp)(uval rc, uval arg);

    CallBackOp cb;

#ifdef CREATE_THREAD_FOR_CALLBACK 
    static void RunCallBack(uval arg);
    class RunCallBackArg {
    public:
	CallBackOp cb;
	uval err, token;
	DEFINE_GLOBAL_NEW(RunCallBackArg);
	RunCallBackArg(CallBackOp op, uval e, uval t) : cb(op), err(e), token(t){}
    };
#endif // #ifdef CREATE_THREAD_FOR_CALLBACK

    SysStatus asyncRequest(StubBlockDev &stub,
			   uval type, uval addr, uval size, uval offset,
			   uval cont);

    /* Defines shared memory transport area to send I/O requests to the
     * disk object in the kernel */
    class DiskTransportProducer;
    typedef DiskTransportProducer **DiskTransportProducerRef;
    SysStatus initTransport(uval needPhysAddr);
    DiskTransportProducerRef diskTransportVirt, diskTransportPhys;

    virtual SysStatus aWriteBlockInternal(uval blockNumber, void *block, 
					  uval cont, uval tryOp = 0);

public:
    DEFINE_GLOBAL_NEW(DiskClientAsync);
    DEFINE_REFS(DiskClientAsync);
    DEFINE_ACCESS_SERVER(CallBackObj,MetaObj::none,MetaObj::none);
    DiskClientAsync(CallBackOp f) : cb(f), diskTransportVirt(NULL),
	diskTransportPhys(NULL) {}
    virtual SysStatus init(char *blockDev, uval needPhysAddr = 1);

    virtual SysStatus aReadBlock(uval blockNumber, void *block, uval cont);
    virtual SysStatus aWriteBlock(uval blockNumber, void *block, uval cont) {
	return aWriteBlockInternal(blockNumber, block, cont);
    }

    virtual SysStatus tryAsyncWriteBlock(uval blockNumber, void *block,
					 uval cont) {
	return aWriteBlockInternal(blockNumber, block, cont, 1/* tryOp */);
    }

    virtual SysStatus aReadBlockPhys(uval blockNumber, uval paddr, uval cont);
    virtual SysStatus aWriteBlockPhys(uval blockNumber, uval paddr,
				      uval cont);

    virtual SysStatus readBlock(uval blockNumber, void *block) {
	return _SERROR(2834, 0, ENOSYS);
    }
    virtual SysStatus writeBlock(uval blockNumber, void *block) {
	return _SERROR(2835, 0, ENOSYS);
    }
    virtual SysStatus readBlockPhys(uval blockNumber, uval paddr) {
	return _SERROR(2836, 0, ENOSYS);
    }
    virtual SysStatus writeBlockPhys(uval blockNumber, uval paddr) {
	return _SERROR(2837, 0, ENOSYS);
    }

    virtual __async SysStatusUval _processCallBacks(uval arg);

    static SysStatus Create(DiskClientAsyncRef &dcr, char *blockDev,
			    CallBackOp cbf, uval needPhysAddr = 1);
};
#endif /* #ifndef __DISK_CLIENT_ASYNCH_ */
