#ifndef __SERVER_STATE_H_
#define __SERVER_STATE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerState.H,v 1.3 2002/10/10 13:08:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface for keeping track of server states
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/Obj.H>
#include <cobj/ObjectHandle.H>
#include <misc/ListSimpleKeyLocked.H>
#include <meta/MetaServerState.H>
class ServerState;
typedef ServerState** ServerStateRef;


class ServerState : public Obj {
  protected:
#ifndef _IN_STUBGEN
    DEFINE_ACCESS_SERVER(ServerState, MetaObj::none, MetaObj::none);
    static ListSimpleKeyLocked<uval, uval, AllocGlobal> list;
#endif
    static uval64 stateMask;
  public:
    static void ClassInit(VPNum vp);
    static SysStatus waitForNotification(uval bit);
__xpublic:
    static SysStatus _pollForNotification(uval bit);
    static SysStatus _notifyServerState(uval state);
};

#endif /* #ifndef __SERVER_STATE_H_ */
