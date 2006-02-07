#ifndef __DISK_CLIENT_ASYNC_RO_H_
#define __DISK_CLIENT_ASYNC_RO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskClientAsyncRO.H,v 1.2 2005/06/02 20:05:54 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include "DiskClientAsync.H"

class DiskClientAsyncRO;
typedef DiskClientAsyncRO **DiskClientAsyncRORef;

class DiskClientAsyncRO : public DiskClientAsync {
public:
    DEFINE_GLOBAL_NEW(DiskClientAsyncRO);
    DEFINE_REFS(DiskClientAsyncRO);
    DiskClientAsyncRO(CallBackOp f) : DiskClientAsync(f) {}

    virtual SysStatus aWriteBlock(uval blockNumber, void *block, uval cont) {
	return 0;
    }

    virtual SysStatus aWriteBlockPhys(uval blockNumber, uval paddr,
				      uval cont) {
	return 0;
    }

    static SysStatus Create(DiskClientAsyncRef &dcr, char *blockDev,
			    CallBackOp cbf) {

	DiskClientAsyncRO *dc = new DiskClientAsyncRO(cbf);
	
	SysStatus rc = dc->init(blockDev);
	if (_FAILURE(rc)) {
	    delete dc;
	    return rc;
	}

	dcr = (DiskClientAsyncRef) dc->getRef();

	return 0;
    }
};
#endif /* #ifndef __DISK_CLIENT_ASYNCH_ */
