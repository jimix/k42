#ifndef __SHARED_BUFFER_PRODUCER_KERNEL_H_
#define __SHARED_BUFFER_PRODUCER_KERNEL_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferProducerKernel.H,v 1.2 2005/05/20 19:45:20 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <misc/SharedBufferProducer.H>

class SharedBufferProducerKernel : public SharedBufferProducer {
protected:
    SysStatus initFR(ProcessID pid, ObjectHandle &sfroh, uval &smAddr);
public:
    SharedBufferProducerKernel() { passertMsg(0, "?"); }
    SharedBufferProducerKernel(uval _size, uval _entryUvals, uval _numEntries)
	: SharedBufferProducer(_size, _entryUvals, _numEntries) {}
    virtual SysStatus init(ProcessID pid, ObjectHandle &sfroh) {
	return SharedBufferProducer::init(pid, sfroh);
    }
    virtual SysStatus init(ObjectHandle &sfroh) {
	passertMsg(0, "This shouldn't be called. You use the other init\n");
	return _SERROR(2919, 0, 0);
    }
};

#endif // #ifndef __SHARED_BUFFER_PRODUCER_KERNEL_H_

