#ifndef __INIT_SERVER_H_
#define __INIT_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InitServer.H,v 1.1 2003/10/14 17:56:07 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: An interface for registering and invoking a step of the
 *		       system initialization process.
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <sync/FairBLock.H>
#ifndef _IN_STUBGEN
#include <meta/MetaInitServer.H>
#else /* _IN_STUBGEN */
// FIXME: fix stub-compiler grammar so don't need to do this
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class InitServer: public Obj {
#ifndef _IN_STUBGEN
    struct InitCall {

	DEFINE_GLOBAL_NEW(InitCall);
	ObjectHandle oh;
	static const uval InitComplete = 1;
	static const uval InitInProgress = 2;
	static const uval InitRequested = 4;
	static const uval InitFailed = 8;
	uval status;
	InitCall(ObjectHandle h):oh(h), status(0){};
    };
    static const uval MAX_INIT_STEP = 64;
    InitCall *callStatus[MAX_INIT_STEP];
    FairBLock lock;
    uval currID;
    static InitServer* theServer;
#endif /* _IN_STUBGEN */
    InitServer();

    DEFINE_GLOBAL_NEW(InitServer);
    DEFINE_ACCESS_SERVER(InitServer, MetaObj::controlAccess|MetaObj::attach,
			 MetaObj::none);
    static void RunInit(uval id);
    virtual void runInit(uval id);
public:
    static void ClassInit(VPNum vp);
    virtual SysStatus defineAction(uval id, ObjectHandle oh);
__xpublic:
    static SysStatus _DefineAction(__in uval id, __in ObjectHandle oh);
    virtual SysStatus _complete(__in SysStatus rc, __XHANDLE xh);
};

#endif /* #ifndef __INIT_SERVER_H_ */
