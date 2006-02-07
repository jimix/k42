#ifndef __SHARED_BUFFER_CONSUMER_KERNEL_H_
#define __SHARED_BUFFER_CONSUMER_KERNEL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferConsumerKernel.H,v 1.3 2005/05/24 02:59:30 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <misc/SharedBufferConsumer.H>

class SharedBufferConsumerKernel : public SharedBufferConsumer {
protected:
    virtual SysStatus initShMem(ObjectHandle sfrOH, ProcessID pidProducer,
				uval sz, uval &shAddr);
};
#endif // #ifndef __SHARED_BUFFER_CONSUMER_USER_H_
