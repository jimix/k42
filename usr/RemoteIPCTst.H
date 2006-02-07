#ifndef __USR_TST_H_
#define __USR_TST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RemoteIPCTst.H,v 1.1 2002/08/15 20:13:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides simple test interface to a user program
 * **************************************************************************/

#include "meta/MetaRemoteIPCTst.H"
#include <cobj/TypeMgr.H>

class RemoteIPCTst : public Obj {
__xpublic:
    static SysStatus TestAlive();
    static __async SysStatus DisableMicrosecs(__in uval us);
    static __async SysStatus RefuseMicrosecs(__in uval us);
};

#endif /* #ifndef __USR_TST_H_ */
