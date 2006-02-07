#ifndef __PROCESS_H_
#define __PROCESS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Process.H,v 1.99 2004/08/16 22:00:29 andrewb Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A process object keeps track of all other
 * objects associated with a process.
 * **************************************************************************/

#include "ProcessServer.H"
#include "mem/HAT.H"
#include "exception/ProcessAnnex.H"
#include "mem/Region.H"

class LazyState;
class PageFaultNotification;

class Process : public ProcessServer {
public:

    DEFINE_REFS(Process);

    virtual SysStatus destroy() = 0;

    virtual SysStatus reclaimSelf() = 0;

    virtual SysStatus vpnumToPpnum(VPNum vp, VPNum &pp) = 0;

    virtual SysStatus getUserMode(uval &user) = 0;

    virtual SysStatus getHATProcess(HATRef &hat) = 0;

    virtual SysStatus getPM(PMRef &pm) = 0;

    virtual SysStatus callBreakpoint();

    virtual SysStatus dumpCObjTable();

    virtual SysStatus dumpLeakInfo();

    virtual SysStatus resetLeakInfo();

    virtual SysStatus vaddrToRegion(uval vaddr, RegionRef &reg) = 0;

    virtual SysStatus Process::getVMapsRAddr(uval vaddr, uval& vMapRaddr);

    /*
     * Handle a page fault:
     *  < 0 error
     * == 0 in-core page fault handled
     *  > 0 key to be returned to user for subsequent up-call
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp)=0;

    virtual void kosher()=0;

    virtual SysStatus perfMon(uval action, uval ids);

    virtual SysStatus createDispatcher(CPUDomainAnnex *cda,
				       DispatcherID dspid,
				       EntryPointDesc entry,
				       uval dispatcherAddr,
				       uval initMsgLength,
				       char *initMsg)=0;
    virtual SysStatus detachDispatcher(CPUDomainAnnex *cda,
				       DispatcherID dspid)=0;
    virtual SysStatus attachDispatcher(CPUDomainAnnex *cda,
				       DispatcherID dspid)=0;

    // enqueues a remote IPC (call or return)
    virtual SysStatus sendRemoteIPC(CommID target,
				    RemoteIPCBuffer *ipcBuf)=0;

    // enqueues an asynchronous remote message
    virtual SysStatus sendRemoteAsyncMsg(CommID target, CommID source,
					 XHandle xhandle, uval methodnum,
					 uval length, uval *buf)=0;

    // all the lazy stuff
    virtual SysStatus getAddrLazyState(LazyState *&ls)=0;
    virtual SysStatus lazyCopyState(LazyState *ls)=0; // copy from
    virtual SysStatus _lazyReOpen(__CALLER_PID caller, __in sval file,
				  __out uval &type, __out ObjectHandle &oh,
				  __outbuf(dataLen:512) char *data,
				  __out uval &dataLen);
    virtual SysStatus _lazyClose(sval file) { return lazyClose(file); }
    virtual SysStatus _lazyGiveAccess(__in sval file, __in uval type,
				      __in ObjectHandle oh,
				      __in sval closeChain,
				      __in AccessRights match,
				      __in AccessRights nomatch,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen) {
	return lazyGiveAccess(file, type, oh, closeChain, match, nomatch,
			      data, dataLen);
    }

    // copy to target
    virtual SysStatus _lazyCopyState(__CALLER_PID caller, XHandle target);

    // -------------- region list operations ------------------
    virtual SysStatus printRegions() = 0;

    /*
     * Set range for region allocations for this process.
     */
    virtual SysStatus setRegionsBounds(uval start,
				       uval allocStart, uval end) = 0;

    /*
     * attach a region with address determined by regionList
     */
    virtual SysStatus attachDynamicRegion(uval &vaddr, uval size,
					  RegionRef reg, 
					  RegionType::Type regionType,
					  uval alignment=0)=0;

    /*
     * attach a region at fixed address
     */
    virtual SysStatus attachFixedRegion(uval vaddr, uval size,
					RegionRef reg,
					  RegionType::Type regionType)=0;

#ifdef PROTOTYPE_SUBLIST
    /*
     * attach a region at fixed address
     */
    virtual SysStatus attachFixedSubRegion(uval vaddr, uval size,
                                           RegionRef reg,
                                           RegionType::Type regionType)=0;
#endif

    /*
     * attach a region at within given bounds
     */
    virtual SysStatus attachWithinRangeRegion(uval &vaddr, uval vaddr2,
					      uval size, RegionRef reg,
					      RegionType::Type regionType,
					      uval alignment=0)=0;

    /*
     * deattach an existing region
     */
    virtual SysStatus detachRegion(RegionRef reg)=0;
    // -------------- end region list operations --------------

};

#endif /* #ifndef __PROCESS_H_ */
