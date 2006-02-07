#ifndef __SAMPLE_SERVICE_H_
#define __SAMPLE_SERVICE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SampleService.H,v 1.10 2003/03/25 13:14:34 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for testing user-level service invocation.
 * **************************************************************************/

class SampleService : public Obj {
protected:
    BLock lock;
    sval counter;
    sval *counterArray;

    void init();

protected:
    SampleService() { /* empty body */ }

__xpublic:
    virtual SysStatus testRequest() = 0;
    virtual SysStatus testRequestWithIncrement() = 0;
    virtual SysStatus testRequestWithLock() = 0;
    virtual SysStatus testRequestWithFalseSharing() = 0;

__xprivate:
    static SysStatus GetServerPID(__out uval &pid);
    static SysStatus Create(__out ObjectHandle &oh, __CALLER_PID caller);
    static SysStatus Die();
};

typedef SampleService **SampleServiceRef;

#endif /* #ifndef __SAMPLE_SERVICE_H_ */
