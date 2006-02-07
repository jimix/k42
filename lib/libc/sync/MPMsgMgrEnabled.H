#ifndef __MPMSG_MGR_ENABLED_H_
#define __MPMSG_MGR_ENABLED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgrEnabled.H,v 1.12 2003/09/30 11:46:07 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages that are processed enabled
 * **************************************************************************/

#include <sync/MPMsgMgr.H>

class MPMsgMgrEnabled : public MPMsgMgr {
public:
    static void ClassInit(DispatcherID dspid, MemoryMgrPrimitive *pa);
    static void PostFork();

private:
    static void ProcessMsgList(uval listUval);
    SysStatus processSendQueue();
    static void ProcessSendQueue(SoftIntr::IntrType);
    static void ProcessReplyQueue(SoftIntr::IntrType);
};

#endif /* #ifndef __MPMSG_MGR_ENABLED_H_ */
