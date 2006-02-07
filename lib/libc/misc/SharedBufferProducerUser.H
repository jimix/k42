#ifndef __SHARED_BUFFER_PRODUCER_USER_H_
#define __SHARED_BUFFER_PRODUCER_USER_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferProducerUser.H,v 1.1 2005/05/20 19:44:03 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <misc/SharedBufferProducer.H>

class SharedBufferProducerUser : public SharedBufferProducer {
protected:
    SysStatus initFR(ProcessID pid, ObjectHandle &sfroh, uval &smAddr);
public:
    SharedBufferProducerUser() { passertMsg(0, "?"); }
    SharedBufferProducerUser(uval _size, uval _entryUvals, uval _numEntries)
	: SharedBufferProducer(_size, _entryUvals, _numEntries) {}
    virtual SysStatus init(ProcessID pid, ObjectHandle &sfroh) {
	passertMsg(0, "This shouldn't be called. You use the other init\n");
	return _SERROR(2918, 0, 0);
    }
    virtual SysStatus init(ObjectHandle &sfroh) {
	return SharedBufferProducer::init(0, // foo value for pid
					  sfroh);
    }
};

#endif // #ifndef __SHARED_BUFFER_PRODUCER_USER_H_

