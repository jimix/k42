#ifndef __SHARED_BUFFER_CONSUMER_USER_H_
#define __SHARED_BUFFER_CONSUMER_USER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferConsumerUser.H,v 1.2 2005/05/24 02:59:29 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include "SharedBufferConsumer.H"
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

class SharedBufferConsumerUser : public SharedBufferConsumer {
protected:
    virtual SysStatus initShMem(ObjectHandle soh, ProcessID pidProducer,
				uval sz, uval &shAddr) {
	SysStatus rc;
	rc = StubRegionDefault::_CreateFixedLenExt(
	    shAddr, sz, 0/*aligmentreq*/, soh, 0/*fileOffset*/,
	    AccessMode::writeUserWriteSup, 0/*target*/, RegionType::K42Region);
	tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	return rc;
    }
};
#endif // #ifndef __SHARED_BUFFER_CONSUMER_USER_H_
