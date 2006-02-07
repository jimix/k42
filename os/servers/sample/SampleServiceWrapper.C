/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SampleServiceWrapper.C,v 1.11 2003/03/25 13:14:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for testing user-level service invocation.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SampleServiceWrapper.H"
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>

SysStatus
SampleServiceWrapper::testRequest()
{
    return stub.testRequest();
}

SysStatus
SampleServiceWrapper::testRequestWithIncrement()
{
    return stub.testRequestWithIncrement();
}

SysStatus
SampleServiceWrapper::testRequestWithLock()
{
    return stub.testRequestWithLock();
}

SysStatus
SampleServiceWrapper::testRequestWithFalseSharing()
{
    return stub.testRequestWithFalseSharing();
}

/*static*/ SysStatus
SampleServiceWrapper::Create(SampleServiceRef &ref)
{
    SysStatus rc;
    ObjectHandle oh;

    do {
	rc = StubSampleService::Create(oh);
	if (!_SUCCESS(rc)) {
	    cprintf("waiting for sample server %lx\n", rc);
	    Scheduler::DelaySecs(1);
	    StubSampleService::__Reset__metaoh();
	}
    } while(_FAILURE(rc));

    SampleServiceWrapper *wrapper = new SampleServiceWrapper;
    if (wrapper == NULL) return _SERROR(1247, 0, ENOSPC);
    wrapper->stub.setOH(oh);
    ref = (SampleServiceRef)
	CObjRootSingleRep::Create(wrapper, NULL);

    if (ref == NULL) {
	delete wrapper;
	return _SERROR(1248, 0, ENOSPC);
    }

    return 0;
}
