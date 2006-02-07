#ifndef __KERNEL_PAGING_TRANSPORT_H_
#define __KERNEL_PAGING_TRANSPORT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelPagingTransport.H,v 1.13 2005/08/30 19:09:25 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: kernel paging transport object, which talks to a
 *                     paging transport in the file system, providing
 *                     flow control
 * **************************************************************************/

#include <mem/SharedBufferProducerKernel.H>
#include <mem/FR.H>

// stub compiler doesn't deal with volatile
#ifdef _IN_STUBGEN
#define volatile 
#endif

class StubFileHolder;

class KernelPagingTransport;
class IORestartRequests;
typedef KernelPagingTransport **KernelPagingTransportRef;

class KernelPagingTransport : public SharedBufferProducerKernel __xbase(Obj) {
protected:
    StubFileHolder *stubTrans;

    virtual SysStatus kickConsumer();

    IORestartRequests *restartRequests;
    virtual uval locked_isTransportFull() {
	_ASSERT_HELD(lock);
	return (outstanding == numEntries - 1);
    }

public:
    struct Request {
	uval type, fileToken, addr, objOffset, size;
    };
	
#ifndef _IN_STUBGEN
    enum {START_FILL = 0, START_WRITE = 1};
    static const uval ENTRY_UVALS = 5;
    static const uval SIZE = PAGE_SIZE;
#if 0 // this is the value we have been using
    static const uval NUM_ENTRIES = 32;
#else // this should work now
    static const uval NUM_ENTRIES = 64;
#endif
    KernelPagingTransport() : SharedBufferProducerKernel(SIZE, ENTRY_UVALS,
							 NUM_ENTRIES),
	stubTrans(NULL), restartRequests(NULL) {err_printf(">");};
#endif // #ifndef _IN_STUBGEN

    virtual SysStatus startWrite(uval fileToken, uval addr, uval objOffset,
				 uval size);

    /* writeReq is the data structure that the FR got from the FCM invocation;
     * addr is the new address computed using convertAddressWriteTo based on
     * writeReq->physAddr */
    virtual SysStatus tryStartWrite(uval fileToken, uval addr, uval objOffset,
				    uval size, IORestartRequests *rr);

    virtual SysStatus startFillPage(uval fileToken, uval addr,
				    uval objOffset);
    virtual SysStatus frIsNotInUse(uval fileToken);

    virtual SysStatus ioComplete();

__xprivate:
    /*
     *  Functions called by the file system
     */

    virtual SysStatus _createFRPA(__out ObjectHandle &oh,
				  __CALLER_PID processID,	
				  __in ObjectHandle file,
				  __in uval len,
				   __in uval filetoken,
				   __inbuf(namelen) char *name,
				   __in uval namelen) __xa(none);
     virtual SysStatus _createFRVA(__out ObjectHandle &oh,
				   __CALLER_PID processID,
				   __in uval transferAddr,
				   __in ObjectHandle file,
				   __in uval len,
				   __in uval filetoken,
				   __inbuf(namelen) char *name,
				   __in uval namelen) __xa(none);
     virtual SysStatus _createFRPANonPageable(
	     __out ObjectHandle &oh,
	     __CALLER_PID processID,	
	     __in ObjectHandle file,
	     __in uval len,
	     __in uval filetoken,
	     __inbuf(namelen) char *name,
	     __in uval namelen) __xa(none);
     virtual SysStatus _createFRPANonPageableRamOnly(
	     __out ObjectHandle &oh,
	     __CALLER_PID processID,	
	     __in ObjectHandle file,
	     __in uval len,
	     __in uval filetoken,
	     __inbuf(namelen) char *name,
	     __in uval namelen) __xa(none);

  public:
    virtual SysStatus init(ProcessID pid, ObjectHandle toh,
			   ObjectHandle &kptoh, ObjectHandle &sfroh) = 0;
};

#endif /* #ifndef __KERNEL_PAGING_TRANSPORT_H__ */

