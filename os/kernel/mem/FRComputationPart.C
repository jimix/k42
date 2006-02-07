/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRComputationPart.C,v 1.1 2004/11/01 19:07:56 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "kernIncs.H"
#include "FRComputationPart.H"
#include "FCMPartitioned.H"

/* virtual */ SysStatus
FRComputationPart::init(uval partitionSize)
{
    FRComputation::init(PageAllocator::LOCAL_NUMANODE);
    // Don't wait for get FCM create it now.
    pageable = 0;  // Ignore pagable state and Force paging off 

    FCMPartitioned::Create(fcmRef,partitionSize,
                           FCMPartitioned::CENTRALIZED, (FRRef)getRef());
    return 0;
}

/* static */ SysStatus
FRComputationPart::InternalCreate(ObjectHandle &frOH, uval partitionSize,
                                  ProcessID caller)
{
    SysStatus rc;
    FRComputationPart *frcomp = new FRComputationPart;

    if (frcomp == NULL) {
	return -1;
    }
    frcomp->init(partitionSize);

    rc = frcomp->giveAccessByServer(frOH, caller);
    if (_FAILURE(rc)) {
	// if we can't return an object handle with object can never
	// be referenced.  Most likely cause is that caller has terminated
	// while this create was happening.
	frcomp->destroy();
    }
    return rc;
}

/* static */ SysStatus
FRComputationPart::_Create(ObjectHandle &frOH, uval partitionSize,
                           __CALLER_PID caller)
{
    return (InternalCreate(frOH, partitionSize,  caller));
}

/*
 * Specify an initialisation function for this module. This is a temporary
 * solution to the 'specify an init function' idea.
 * @param fn the module's init function
 */
#define module_init(fn) extern "C" { void _init(void) { fn(); } }

module_init(MetaFRComputationPart::init);
