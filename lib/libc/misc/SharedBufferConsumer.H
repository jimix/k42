#ifndef __SHARED_BUFFER_CONSUMER_H_
#define __SHARED_BUFFER_CONSUMER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferConsumer.H,v 1.4 2005/05/24 02:59:29 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <sync/BLock.H>

class SharedBufferConsumer : public Obj {
protected:
    BLock lock;
    ObjectHandle sfroh;
    
    uval size;
    uval entrySize;
    uval numEntries;

private:
    uval *baseAddr;
    volatile uval* pidx_ptr;
    volatile uval* cidx_ptr;

protected:
    uval locked_getPidx() { return *pidx_ptr; }
    uval locked_getCidx() { return *cidx_ptr; }
    // argument 'request' points to an area large enough to receive request
    virtual SysStatus locked_getRequest(uval *request);
    virtual SysStatus init(ObjectHandle soh, ProcessID pidProducer,
			   uval size, uval eSize,
			   uval nEntries);

    /* the scheme for setting up the shared memory area differs for kernel/user
     * level, so subclasses SharedBufferConsumerKernel and
     * SharedBufferConsumerUser define the following method, which is used
     * by init */
    virtual SysStatus initShMem(ObjectHandle soh, ProcessID pidProducer,
				uval sz, uval &shAddr)=0;
};
#endif // #ifndef __SHARED_BUFFER_CONSUMER_H_
