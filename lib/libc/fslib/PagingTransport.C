/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PagingTransport.C,v 1.8 2005/05/24 02:59:22 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Abstract interfacre for object providing the
 *                     communication between memory manager and
 *                     file system for paging data in/out.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "PagingTransport.H"
#include <mem/KernelPagingTransport.H>

/* virtual */ void
PagingTransport::setKernelPagingOH(ObjectHandle oh, ObjectHandle soh)
{
    kptoh.initWithOH(oh);
    
    init(soh, _KERNEL_PID, KernelPagingTransport::SIZE,
	 KernelPagingTransport::ENTRY_UVALS,
	 KernelPagingTransport::NUM_ENTRIES);
}

/* virtual __async */ SysStatus
PagingTransport:: _startWrite(uval physAddr, uval objOffset,
			      uval len,
			      __XHANDLE xhandle)
{
    passertMsg(0, "this should not be called\n");
    return 0;
}

/* virtual __async */ SysStatus
PagingTransport::_startFillPage(uval physAddr, uval objOffset,
				__XHANDLE xhandle)
{
    passertMsg(0, "this should not be called\n");
    return 0;
}

/* virtual __async */ SysStatus
PagingTransport::_startIO(__XHANDLE xhandle)
{
    /* FIXME: this is horrible, we want to have a pool of threads to
     * do this work, instead of consuming this incoming thread to do
     * it (thereby holding up the position in the async message queue */

    SysStatus rcget, rc = 0;

    do {
	lock.acquire();
	KernelPagingTransport::Request req;
	rcget = locked_getRequest((uval *) &req);
	lock.release();
	if (_SUCCESS(rcget)) {
	    ServerFileRef sfref = (ServerFileRef) req.fileToken;
	    tassertMsg(sfref != NULL, "sref null\n");
	    if (req.type == KernelPagingTransport::START_WRITE) {
		rc = DREF(sfref)->startWrite(req.addr, req.objOffset,
					     req.size, xhandle);
	    } else if (req.type == KernelPagingTransport::START_FILL) {
		rc = DREF(sfref)->startFillPage(req.addr, req.objOffset,
						xhandle);
	    } else {
		passertMsg(0, "invalid request type?\n");
	    }
	    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
	}
    } while _SUCCESS(rcget);

    return rc;
}

/* virtual __async */ SysStatus
PagingTransport::_frIsNotInUse(uval fileToken)
{
    ServerFileRef sfref = (ServerFileRef) fileToken;
    tassertMsg(sfref != NULL, "sref null\n");
    return DREF(sfref)->frIsNotInUse();
}
