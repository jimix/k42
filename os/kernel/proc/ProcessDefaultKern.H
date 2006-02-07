#ifndef __PROCESS_DEFAULT_KERN_H_
#define __PROCESS_DEFAULT_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessDefaultKern.H,v 1.30 2004/03/01 21:09:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The implementation of the kernel's own process
 * **************************************************************************/

#include "proc/ProcessShared.H"

class ProcessDefaultKern : public ProcessShared<AllocPinnedGlobalPadded> {

    ProcessDefaultKern(HATRef hatRef, PMRef pm, const char *name)
	: ProcessShared<AllocPinnedGlobalPadded>(
	    hatRef, pm, /*userMode*/ 0, /*isKern*/ 1, name) {
	/* empty body */
    }

public:
    DEFINE_PINNEDGLOBALPADDED_NEW(ProcessDefaultKern);

    static void InitKern(VPNum vp, uval ppCount, HATRef hatRef);

    virtual SysStatus sendInterrupt(DispatcherID dspid, SoftIntr::IntrType i) {
	return vpList.sendInterruptKernel(dspid, i);
    }


    virtual SysStatus sendRemoteIPC(CommID target, RemoteIPCBuffer *ipcBuf) {
	return vpList.sendRemoteIPCKernel(target, ipcBuf);
    }

    virtual SysStatus sendRemoteAsyncMsg(CommID target, CommID source,
					 XHandle xhandle, uval methodnum,
					 uval length, uval *buf) {
	return vpList.sendRemoteAsyncMsgKernel(target, source,
					       xhandle, methodnum,
					       length, buf);
    }

    /*
     * Handle a page fault:
     *  < 0 error
     * == 0 in-core page fault handled
     *  > 0 key to be returned to user for subsequent up-call
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp);

    virtual SysStatus kill();
};


#endif /* #ifndef __PROCESS_DEFAULT_KERN_H_ */
