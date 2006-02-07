#ifndef __MPMSG_MGR_EXCEPTION_H_
#define __MPMSG_MGR_EXCEPTION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MPMsgMgrException.H,v 1.13 2001/10/05 21:49:28 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Handles messages that are processed at exception level
 * **************************************************************************/

#include <sync/MPMsgMgr.H>

class MPMsgMgrException : public MPMsgMgr {
public:
    static void ProcessMsgs();
    static void ClassInit(DispatcherID dspid, MemoryMgrPrimitive *pa);

private:
    SysStatus processSendQueue();
};

#endif /* #ifndef __MPMSG_MGR_EXCEPTION_H_ */
