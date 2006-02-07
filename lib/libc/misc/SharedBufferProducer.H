#ifndef __SHARED_BUFFER_PRODUCER_H_
#define __SHARED_BUFFER_PRODUCER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferProducer.H,v 1.2 2005/05/20 19:44:03 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <defines/mem_debug.H>
#include <cobj/ObjectHandle.H>
#include <sync/BLock.H>
#include <misc/SharedBufferProducer.H>

// stub compiler doesn't deal with volatile
#ifdef _IN_STUBGEN
#define volatile 
#endif

/* This is an abstract class; instances should be created with
 * with SharedBufferProducerKernel or SharedBufferProducerUser */
class SharedBufferProducer : public Obj {
protected:
    BLock lock;

    uval outstanding;       /* number of outstanding requests being processed
			     * by the consumer */

    uval size;              /* size of shared memory region to be
			     * allocated */
    uval entryUvals;        /* size (in sizeof(uval)) of request */
    uval numEntries;        /* Number of entries in the shared buffer.
			     * The maximum number of entries available for
			     * storing requests is numEntries - 1 */
    uval *baseAddr;         /* base address for shared memory region */

    /* Argument 'request' points to an area of size entryUvals*sizeof(uval)
     * Argument 'highPriority' == 0 indicates that this is not to be
     * considered a high Priority request.
     * shouldBlock != 0 indicates that the put will block until it succeeds,
     * if shouldBlock == 0 an error is returned.
     */
    virtual SysStatus locked_put(uval *request, uval highPriority,
				 uval shouldBlock);

protected:
    volatile uval* pidx_ptr;    /* pointer to pidx. pidx is the index of
				 * the position where next insertion by the
				 * producer will be done */
    volatile uval* cidx_ptr;    /* pointer to cidx, which stores the position
				 * where the last retrieval was done by the
				 * consumer (i.e., (cidx+1)% numEntries is
				 * the next element to be consumed) */

public:
#if 0
    // the following 2 methods are for debugging only
    virtual uval getPidx() { return *pidx_ptr; }
    virtual uval getCidx() { return *cidx_ptr; }
#endif

protected:

    /* Argument 'request' points to an area of size entryUvals*sizeof(uval)
     * Argument 'highPriority' == 0 indicates that this is not to be
     * considered a high Priority request
     */
    virtual SysStatus locked_putRequest(uval *request, uval highPriority = 0) {
	return locked_put(request, highPriority, 1);
    }

    /* This returns an error if the transport is full */
    virtual SysStatus locked_tryPutRequest(uval *request,
					   uval highPriority = 0) {
	return locked_put(request, highPriority, 0);
    }

    /* the services being implemented using this will know which async
     * interface to invoke to let the 
     * consumer know that there are requests available */
    virtual SysStatus kickConsumer() = 0;

    /* the specific service will know how it wants to do control flow:
     * it can either check if there is space on the transport, or check
     * the number of outstanding requests (not always possible).
     *
     * For example:
     * (1) when the shared buffer is used as a transport between the MM and FS
     * for starting IO operations, the control is done by checking the value
     * of variable 'outstanding', which is decremented when
     * locked_requestCompleted() is invoked;
     * (2) when the shared buffer is used between the disk layer and disk
     * client, there is no communication from the
     * disk client about having received the "ack" from the kernel about
     * the disk operation completion; in this case the following method
     * is implemented by checking availability in the transport.
     */
    virtual uval locked_isTransportFull() = 0;

private:
    // data structure for flow control
    struct BlockedPageableThreads {
	ThreadID    thread;		// blocked thread;
	struct BlockedPageableThreads *next;
	uval notified;
    };

    /* We keep a list of read requests and a list of write requests.
     * (Actually, we separate high priority requests).
     * When using this for I/O operations, we know that
     * when we get a read request, there is already a page
     * frame allocated for it, so we give priority to the read requests */
    BlockedPageableThreads *blockedThreadHighPriorityHead;
    BlockedPageableThreads *blockedThreadOthersHead;

    // highPriority == 0 means that the request to be blocked is not
    // to be considered high priority
    virtual SysStatus locked_block(uval highPriority);

protected:
    virtual SysStatus locked_requestCompleted();

    /* creates an FR for the shared memory. The derivations for this
     * class, SharedBufferProducerUser and SharedBufferProducerKernel,
     * define this method */
    virtual SysStatus initFR(ProcessID pid, ObjectHandle &sfroh,
			     uval &smAddr) = 0;

public:

    /* Returns available positions in the shared buffer. The
     * number is >= 0 and < nEntries */
    static uval Avail(uval pidx, uval cidx, uval nEntries) {
	/* pidx is the idx for the  position of next production;
	 * cidx is the idx for the last place we had a consume.
	 * pidx == cidx means that the buffer is full
	 */
	tassertMsg(pidx >= 0 && pidx < nEntries &&
		   cidx >= 0 && cidx < nEntries, "?");
	return (pidx <= cidx ? (cidx - pidx) 
		: (nEntries - pidx + cidx));
    }
    static uval IsEmpty(uval pidx, uval cidx, uval nEntries) {
	return (Avail(pidx, cidx, nEntries) == nEntries -  1);
    }

    SharedBufferProducer() {
	// This class shouldn't have instances
	passertMsg(0, "You should create objects with "
		   "SharedBufferProducerUser or SharedBufferProducerKernel");
    }

    SharedBufferProducer(uval sizea, uval entryUvalsArg,
			 uval numEntriesa) : outstanding(0),
	size(sizea), entryUvals(entryUvalsArg), numEntries(numEntriesa),
					     pidx_ptr(NULL),
					     cidx_ptr(NULL) {};

    virtual SysStatus init(ProcessID pid, ObjectHandle &sfroh);
};

#endif /* #ifndef __SHARED_BUFFER_PRODUCER_H_ */

