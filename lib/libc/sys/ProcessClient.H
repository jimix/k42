#ifndef __PROCESS_CLIENT_H_
#define __PROCESS_CLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessClient.H,v 1.7 2001/10/08 12:47:11 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines interface for client side of process object
 * **************************************************************************/

#include <sys/BaseProcess.H>

class ProcessClient : public BaseProcess __xbase(Obj) __xadef(search:destroy){
public:
    static void ClassInit(VPNum vp);

    virtual SysStatus getType(TypeID &id);

__xpublic:
    virtual SysStatus _getMemTrans(__out ObjectHandle &oh,
				   __in  uval key,
				   __XHANDLE xhandle,
				   __CALLER_PID pid) __xa(search) = 0;
    virtual __async SysStatus _destructionCallback() __xa(destroy) = 0;
};

#endif /* #ifndef __PROCESS_CLIENT_H_ */
