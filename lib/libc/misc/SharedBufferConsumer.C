/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferConsumer.C,v 1.5 2005/05/24 02:59:29 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SharedBufferConsumer.H"
#include <misc/SharedBufferProducer.H>

/* virtual */ SysStatus
SharedBufferConsumer::init(ObjectHandle soh, ProcessID pidProducer,
			   uval sz, uval eSize,
			   uval nEntries)
{
    SysStatus rc;

    sfroh.initWithOH(soh);
    entrySize = eSize;
    numEntries = nEntries;

    uval shAddr;
    rc = initShMem(soh, pidProducer, sz, shAddr);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    baseAddr = (uval*) shAddr;
    pidx_ptr = baseAddr + numEntries * entrySize;
    cidx_ptr = pidx_ptr + 1;

    return 0;
}

/* virtual */ SysStatus
SharedBufferConsumer::locked_getRequest(uval *req)
{
    tassertMsg(pidx_ptr != NULL && pidx_ptr > baseAddr, "?");
    tassertMsg(cidx_ptr != NULL && cidx_ptr == pidx_ptr + 1, "?");
    tassertMsg(*pidx_ptr <  numEntries, "?");
    tassertMsg(*cidx_ptr <  numEntries, "?");

    if (SharedBufferProducer::IsEmpty(*pidx_ptr, *cidx_ptr, numEntries)) {
	return _SERROR(2855, 0, 0);
    }

    uval new_cidx;
    uval *ptr;
    if (*cidx_ptr + 1 == numEntries) {
	ptr = baseAddr;
	new_cidx = 0;
    } else {
	new_cidx = *cidx_ptr + 1;
	ptr = (uval*) (new_cidx*entrySize + baseAddr);

    }

    memcpy(req, ptr, entrySize * sizeof(uval));

    // update cidx
    *cidx_ptr = new_cidx;

    return 0;
}
