/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferProducerUser.C,v 1.2 2005/05/23 17:47:51 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SharedBufferProducerUser.H"
#include <stub/StubFRComputation.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

SysStatus 
SharedBufferProducerUser::initFR(ProcessID pid, // ignored
				 ObjectHandle &sfroh,
				 uval &smAddr)
{
    SysStatus rc;

    // create FR for shared memory to be used by this transport producer
    rc = StubFRComputation::_Create(sfroh);
    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    
    // create region to be used as shared
    rc = StubRegionDefault::_CreateFixedLenExt(
	    smAddr, size, 0/*alignmentreq*/, sfroh, 0/*fileOffset*/,
	    AccessMode::writeUserWriteSup, 0/*target*/,
	    RegionType::K42Region);

    passertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    
    return 0;
}
