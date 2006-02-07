#ifndef __SHMEMCLNT_H_
#define __SHMEMCLNT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShMemClnt.H,v 1.1 2003/05/01 15:05:50 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines shared memory transport client interface
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <meta/MetaObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <sync/FairBLock.H>
#include <alloc/ObjCache.H>
#ifndef EXPORT_SHMEMCLNT
#define EXPORT_SHMEMCLNT
#include <cobj/ObjectHandle.H>
#include <misc/HashSimple.H>
#endif /* #ifndef EXPORT_SHMEMCLNT */


#include <stub/StubShMemBuf.H>
#ifndef _IN_STUBGEN
#include <stub/StubFR.H>

#include <misc/AutoList.H>
#include <scheduler/Scheduler.H>
#else /* #ifndef _IN_STUBGEN */

// FIXME: fix stub-compiler grammar so don't need to do this
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifndef _IN_STUBGEN */
class ShMemClnt;
typedef ShMemClnt** ShMemClntRef;

class ShMemClnt: public Obj {
protected:
    StubShMemBuf stub;
    ObjectHandle frOH;
    uval addr;
    uval size;
    DEFINE_REFS(ShMemClnt);

    typedef FairBLock LockType;
    LockType lock;

    ShMemClnt(): stub(StubObj::UNINITIALIZED) {};
    typedef HashSimpleLocked<ProcessID, ShMemClntRef, AllocGlobal, 0> ImpHash;
    static ImpHash* imports;
    virtual SysStatus init(ObjectHandle oh);
    static SysStatus Create(ObjectHandle oh, ShMemClntRef &ref);
    DEFINE_GLOBAL_NEW(ShMemClnt);
    virtual SysStatus forkDestroy();
public:
    static void PostFork();
    static void ClassInit(VPNum vp);
    static SysStatus Get(ObjectHandle oh, ShMemClntRef &smcRef);
    virtual SysStatus offsetToAddr(uval offset, uval &addr);

    virtual SysStatus unShare(uval addr);

};

#endif /* __SHMEMCLNT_H_ */
