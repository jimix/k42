#ifndef __MPMSG_MGR_DISABLED_H_
#define __MPMSG_MGR_DISABLED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgrDisabled.H,v 1.12 2003/09/30 11:46:07 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages that are processed disabled
 * **************************************************************************/

#include <sync/MPMsgMgr.H>

class MPMsgMgrDisabled : public MPMsgMgr {
public:
    static void ClassInit(DispatcherID dspid, MemoryMgrPrimitive *pa);
    static void PostFork();

private:
    SysStatus processSendQueue();
    static void ProcessSendQueue(SoftIntr::IntrType);
    static void ProcessReplyQueue(SoftIntr::IntrType);
};

#endif /* #ifndef __MPMSG_MGR_DISABLED_H_ */
