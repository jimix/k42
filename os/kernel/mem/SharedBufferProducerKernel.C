/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferProducerKernel.C,v 1.3 2005/05/22 23:23:14 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include "kernIncs.H"
#include "SharedBufferProducerKernel.H"
#include "mem/FRComputation.H"
#include "mem/RegionDefault.H"

SysStatus 
SharedBufferProducerKernel::initFR(ProcessID pid, ObjectHandle &sfroh,
				   uval &smAddr)
{
    SysStatus rc;
    FRRef frRef;
    // create FR for shared memory to be used by this transport producer
    rc = FRComputation::Create(frRef, PAGE_SIZE);
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    
    // create region to be used as shared
    RegionRef rref;
    rc = RegionDefaultKernel::CreateFixedLen(
	rref, GOBJK(TheProcessRef), smAddr, size, 0/*aligmentreq*/,
	frRef, 1, 0/*fileOffset*/, AccessMode::writeUserWriteSup);
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    
    /* we need to give an oh to the FR to the caller of
     * KernelPagingTransport::Create */
    rc = DREF(frRef)->giveAccessByServer(sfroh, pid);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    return 0;
}
