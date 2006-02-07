#ifndef __SAMPLE_SERVICE_WRAPPER_H_
#define __SAMPLE_SERVICE_WRAPPER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SampleServiceWrapper.H,v 1.11 2003/03/25 13:14:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for testing user-level service invocation.
 * **************************************************************************/

#include "SampleService.H"
#include "stub/StubSampleService.H"

class SampleServiceWrapper : public SampleService {
    StubSampleService stub;

public:
    DEFINE_GLOBAL_NEW(SampleServiceWrapper);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch){
	return stub._giveAccess(oh,toProcID,match,nomatch);
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh,
					 ProcessID toProcID) {
	return stub._giveAccess(oh,toProcID);
    }

    SampleServiceWrapper() : SampleService(), stub(StubObj::UNINITIALIZED) {
      /* empty body */
    }
 
    virtual SysStatus testRequest();
    virtual SysStatus testRequestWithIncrement();
    virtual SysStatus testRequestWithLock();
    virtual SysStatus testRequestWithFalseSharing();

    static SysStatus Create(SampleServiceRef &ref);
};

#endif /* #ifndef __SAMPLE_SERVICE_WRAPPER_H_ */
