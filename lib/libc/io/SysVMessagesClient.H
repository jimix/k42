#ifndef __SYSV_MESSAGES_CLIENT_H_
#define __SYSV_MESSAGES_CLIENT_H_
/****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVMessagesClient.H,v 1.1 2003/11/12 02:27:21 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: client (user space) packet ring class
 ****************************************************************************/
#include <sys/sysTypes.H>
#include <cobj/CObjRoot.H>
#include <cobj/BaseObj.H>
#include <cobj/TypeMgr.H>
#include <meta/MetaSysVMessagesClient.H>
#include <stub/StubSysVMessagesClient.H>

class SysVMessagesClient;
typedef SysVMessagesClient **SysVMessagesClientRef;

class SysVMessagesClient : public Obj {
protected:
    //only one instance
    static SysVMessagesClient *obj;
    ObjectHandle callBackOH;
#ifndef _IN_STUBGEN
    DEFINE_GLOBAL_NEW(SysVMessagesClient);
    DEFINE_ACCESS_SERVER(SysVMessagesClient,
			 MetaObj::controlAccess,MetaObj::none);
#endif /* #ifdef _IN_STUBGEN */
    void init();
    SysStatus getCallBackOH(ObjectHandle& oh, void*& blockKey);
public:
    static SysStatus ClassInit();
    static SysStatus GetCallBackOH(ObjectHandle& oh, void*& blockKey);

__xprivate:
    // Asynchronous notification methods
    virtual __async SysStatus _notify();
};

#endif /* #ifndef __SYSV_MESSAGES_CLIENT_H_ */

