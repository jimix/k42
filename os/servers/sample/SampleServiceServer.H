#ifndef __SAMPLE_SERVICE_SERVER_H_
#define __SAMPLE_SERVICE_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SampleServiceServer.H,v 1.9 2003/03/25 13:14:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for testing user-level service invocation.
 * **************************************************************************/

#include "SampleService.H"
#include "meta/MetaSampleService.H"
#include <cobj/TypeMgr.H>

class SampleServiceServer : public SampleService {
public:
    DEFINE_GLOBAL_NEW(SampleServiceServer);
    DEFINE_ACCESS_SERVER(SampleService, 0xff, 0);

    virtual SysStatus testRequest();
    virtual SysStatus testRequestWithIncrement();
    virtual SysStatus testRequestWithLock();
    virtual SysStatus testRequestWithFalseSharing();

    static void ClassInit();
    static void Block();

private:
    static SysStatus GetServerPID(__out uval &pid);
    static SysStatus Create(__out ObjectHandle &oh, __CALLER_PID caller);
    static SysStatus Die();
};

#endif /* #ifndef __SAMPLE_SERVICE_SERVER_H_ */
