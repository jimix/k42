#ifndef __CO_LIST_SERVER_H__
#define __CO_LIST_SERVER_H__
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COListServer.H,v 1.3 2005/03/02 05:27:55 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  This is a server object which can allow any
 * Address space to provide details about the clustered objects currently
 * Instantiated.  
 ****************************************************************************/

/***********************************
 *  FIXME / TODO
 *  1) Must support multiple vps  (eg memory region should be partitioned)
 *  2) Must add destruction logic so that if client disappears we
 *     clean things up correctly ... should be straight forward triggerd
 *     on XObject stuff.
 */

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <meta/MetaObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>

#include "meta/MetaCOListServer.H"
#include <cobj/sys/CODesc.H>
#include <stub/StubFR.H>
#include <cobj/Factory.H>

DEFINE_REFTYPE(COListServer);

class COListServer : public Obj
   __xbase(Obj) 
{
#ifndef _IN_STUBGEN
    BLock lock;
    CODesc *coDescArray;
    StubFR fr;
    ProcessID clientPid;

    SysStatus init(ProcessID pid);

    COListServer() : 
        coDescArray(0), fr(StubObj::UNINITIALIZED) {lock.init();}
    DEFINE_GLOBALPADDED_NEW(COListServer);
#endif
  public:
    DEFINE_ACCESS_SERVER(COListServer,MetaObj::none,MetaObj::none);
    DEFINE_REFS(COListServer);
    static SysStatus Create(ObjectHandle &oh, ProcessID pid);

__xpublic:
    enum {SizeOfRegion = PAGE_SIZE * 128};

    virtual SysStatus getCODescsFR(__out ObjectHandle & oh, __CALLER_PID pid);
    virtual SysStatus updateCODescs(__out uval & num);
    //FIXME: kludging using uvals ... please fix this to use correct types
    //       for stub
    virtual SysStatus hotSwapInstance(__in uval fac, 
                                      __in uval target);
    virtual SysStatus takeOverFromFac(__in uval newFac, __in uval oldFac);
    virtual SysStatus printInstances(__in uval fac);
    virtual SysStatus readMem(__in uval addr, __inout uval & len, 
                              __outbuf(len:2048) char *buf);
};
#endif /* #ifndef __CO_LIST_SERVER_H__ */
