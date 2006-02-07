#ifndef __PAGING_TRANSPORT_H_
#define __PAGING_TRANSPORT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PagingTransport.H,v 1.9 2005/05/22 23:24:10 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Abstract interfacre for object providing the
 *                     communication between memory manager and
 *                     file system for paging data in/out.         
 * **************************************************************************/

#include <misc/SharedBufferConsumerUser.H>

#include "ServerFile.H"

class PagingTransport;
typedef PagingTransport **PagingTransportRef;

class PagingTransport : public SharedBufferConsumerUser __xbase(Obj) {
protected:
    ObjectHandle kptoh;
public:
    DEFINE_REFS(PagingTransport);
    virtual void setKernelPagingOH(ObjectHandle oh, ObjectHandle soh);
    virtual SysStatus getKptoh(ObjectHandle &oh) { oh = kptoh; return 0; }
__xpublic:
    virtual __async SysStatus _startWrite(uval physAddr, uval objOffset,
					  uval len, __XHANDLE xhandle);

    virtual __async SysStatus _startFillPage(uval physAddr, uval objOffset,
					     __XHANDLE xhandle);

    virtual __async SysStatus _frIsNotInUse(uval fileToken);

    virtual __async SysStatus _startIO(__XHANDLE xhandle);

    virtual SysStatus destroy() {
	err_printf("PagingTransport::destroy()\n");
	return 0;
    }
};
#endif // #ifndef __PAGING_TRANSPORT_H_
