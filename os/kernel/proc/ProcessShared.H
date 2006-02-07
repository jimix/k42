#ifndef __PROCESS_SHARED_H_
#define __PROCESS_SHARED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessShared.H,v 1.67 2004/12/16 23:09:04 awaterl Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic default process structure.  At the moment, this
 *                     is the simpler of two instances of the Process class,
 *                     the other is ProcessReplicated.  This class is used
 *                     for the kernel object only, everything else uses the
 *                     other class.
 * Locking strategy:
 *   - locking for calls to regions is done internal to the region list
 *   - locking for creating/destroying threads is done per process annex
 *     list in PerProcessor
 *   - locking for notifications done inside per-processor notificatoin mgr
 *   - all internal variables are read-only after initialization, no locking
 *   - the process is not made available for access until all initialization is
 *     complete, hence no locking on initialization
 *   - external object lists... locked independantly/internally
 *   - on destruction, calls to all sub-objects to destroy, they are internally
 *     responsible for rejecting new requests after they have been destroyed
 * **************************************************************************/

#include "proc/Process.H"
#include "mem/RegionList.H"
#include "sync/PtrBLock.H"
#include "proc/ProcessVPList.H"
#include <meta/MetaProcessServer.H>
#include <sync/MPMsgMgr.H>
#include <sys/SMTMgr.H>
#include <bilge/LazyState.H>
#include <cobj/CObjRootSingleRep.H>
#include <cobj/Factory.H>

template<class ALLOC>
class ProcessShared : public Process {
public:
    DEFINE_FACTORY_BEGIN(Factory)
        virtual SysStatus create(ProcessRef& outPref, HATRef h, PMRef pm,
                                 ProcessRef creator, const char *name);
        virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
        virtual SysStatus destroy(ProcessRef ref);
    DEFINE_FACTORY_END(Factory)

protected:
    class Root : public CObjRootSingleRep {
    public:
        Root(CObjRep *co) : CObjRootSingleRep(co) { /* empty body */ }
        Root(CObjRep *co, RepRef ref,
             CObjRoot::InstallDirective idir=CObjRoot::Install)
            : CObjRootSingleRep(co, ref, idir) { /* empty body */ }
        DEFINE_GLOBAL_NEW(Root);
        virtual SysStatus getDataTransferExportSet(DTTypeSet *set);
        virtual SysStatus getDataTransferImportSet(DTTypeSet *set);
        virtual DataTransferObject *dataTransferExport(DTType dtt,
						       VPSet transferVPSet);
        virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					     DTType dtt, VPSet transferVPSet);

    };

    RegionList<ALLOC> rlst;		// list of regions allocated to program
    LazyState         lazyState;	// state lazily passed on fork/exec
    HATRef	      hatRefProcess;	// reference to the per-process hat
    class ProcessVPList vpList;		// list of per-vp information
    FactoryRef        factoryRef;       // factory that created this instance
    bool updatedFlagDoNotDestroy; /* hack */

    ProcessShared() { /* empty body */ }
    ProcessShared(HATRef h, PMRef pm, uval userMode, uval isKern,
		  const char *name);

    XObjectListProc matched;
    PMRef           pmRef;
    uval            OSData;

    struct ObjectHandleHolder {
	DEFINE_ALLOC_NEW(ObjectHandleHolder);
	ObjectHandle oh;
    };
    SMTMgr memTransMgr;

public:
    DEFINE_ALLOC_NEW(ProcessShared);

    DEFINE_ACCESS_SERVER(
	ProcessServer,
	(MetaProcessServer::controlAccess|
	 MetaProcessServer::processOnly|
	 MetaProcessServer::attach|
	 MetaProcessServer::destroy),
	0);

    static SysStatus Create(ProcessRef& outPref, HATRef h, PMRef pm,
                            ProcessRef creator, const char *name);

    virtual SysStatusProcessID getPID() {
	// LOCKING: read-only item accessed
	return _SRETPID(vpList.getPID());
    }

    virtual SysStatus lockMatchedXObjectList() {
	return matched.lockIfNotClosing();}
    virtual SysStatus unlockMatchedXObjectList() {
	matched.unlock(); return 0; }

    // attach x object to process for garbage collection
    virtual SysStatus addMatchedXObj(XHandle xhandle) {
	// LOCKING: see comment top of file
	matched.locked_add(xhandle); return 0;
    }

    virtual SysStatus removeMatchedXObj(XHandle xhandle) {
	// LOCKING: see comment top of file
	matched.locked_remove(xhandle); return 0;
    }

    virtual SysStatus closeMatchedXObjectList() {
	return matched.close(); }
    virtual SysStatus isClosedMatchedXObjectList() {
	return matched.isClosed(); }
    virtual SysStatus isEmptyMatchedXObjectList() {
	return matched.isEmpty(); }

    virtual SysStatus destroy();

    /* should only be called by Factory::destroy */
    virtual SysStatus reclaimSelf();

    virtual SysStatus getUserMode(uval& user) {
	user = vpList.getUserMode(); return 0;
    }

    virtual SysStatus vpnumToPpnum(VPNum vp, VPNum &pp) {
	return vpList.vpnumToPpnum(vp, pp);
    }

    virtual SysStatus getHATProcess(HATRef &ref) {
	// LOCKING: read-only data
	ref = hatRefProcess;  return 0;
    }

    virtual SysStatus getPM(PMRef &ref) {
	// LOCKING: read-only data
	ref = pmRef;  return 0;
    }

    /*
     * Set range for region allocations for this process.
     */
    virtual SysStatus setRegionsBounds(uval start, uval allocStart, uval end) {
	rlst.setRegionsBounds(start, allocStart, end);
	return 0;
    }

    /*
     * attach a region with address determined by regionList
     */
    virtual SysStatus attachDynamicRegion(
	uval &vaddr, uval size, RegionRef reg, RegionType::Type regionType,
	uval alignment=0) {
	// LOCKING: internal to region list
	return rlst.attachDynamicRegion(
	    vaddr, size, reg, regionType, alignment);
    }


    /*
     * attach a region at fixed address
     */
    virtual SysStatus attachFixedRegion(
	uval vaddr, uval size, RegionRef reg, RegionType::Type regionType) {
	// LOCKING: internal to region list
	return rlst.attachFixedRegion(vaddr, size, reg, regionType);
    }

#ifdef PROTOTYPE_SUBLIST    
    /*
     * attach a region at fixed address
     */
    virtual SysStatus attachFixedSubRegion(uval vaddr, uval size,
                                           RegionRef reg,
                                           RegionType::Type regionType) {
        passertMsg(0, "NYI");
        return -1;
    }
#endif

    /*
     * attach a region at within given bounds
     */
    virtual SysStatus attachWithinRangeRegion(
	uval &vaddr, uval vaddr2, uval size, RegionRef reg,
	RegionType::Type regionType, uval alignment=0) {
        SysStatus retvalue;
	// LOCKING: internal to region list
	retvalue = rlst.attachWithinRangeRegion(
	    vaddr, vaddr2, size, reg, regionType, alignment);
	return (retvalue);
    }



    /*
     * deattach an existing region
     */
    virtual SysStatus detachRegion(RegionRef reg) {
	// LOCKING: internal to region list
	return rlst.detachRegion(reg);
    }

    /*
     * split an exiting region to fit the new region specified
     */
    virtual SysStatus splitRegion(uval newVaddr, uval newSize,
                                  RegionRef newReg, 
                                  RegionType::Type regionType) {
        passertMsg(0,"NYI\n");
        return 0;
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

    virtual SysStatus printRegions() {
	// LOCKING: internal to region list
	cprintf("%s regions: ", vpList.getName());
	rlst.printRegions(); return 0;
    }

    virtual SysStatus vaddrToRegion(uval a, RegionRef &reg) {
	// LOCKING: internal to region list
	return rlst.vaddrToRegion(a, reg);
    }

    virtual void kosher();

#if 0 //MAA
    /*
     * Called by user level page allocator when it needs (more) space,
     * address of new region returned.
     */
    virtual SysStatus getMemory(__in uval size, __inout uval & vaddr);
#endif /* #if 0 //MAA */

    virtual SysStatus kill();

    virtual SysStatus regress();

    //FIXME - only in debug kernel
    //::breakpoint is the global scope routine, not the one in ProcessShared
    virtual SysStatus breakpoint() {::breakpoint(); return 0;};

    virtual SysStatus userHandleFault(__in uval vaddr, __in VPNum vp);


    virtual SysStatus unmapRange(__in uval start, __in uval size);

    virtual SysStatus createDispatcher(CPUDomainAnnex *cda,
				       DispatcherID dspid,
				       EntryPointDesc entry,
				       uval dispatcherAddr,
				       uval initMsgLength,
				       char *initMsg);
    virtual SysStatus detachDispatcher(CPUDomainAnnex *cda,
				       DispatcherID dspid);
    virtual SysStatus attachDispatcher(CPUDomainAnnex *cda,
				       DispatcherID dspid);

    virtual SysStatus waitForTermination();

    virtual SysStatus sendInterrupt(DispatcherID dspid, SoftIntr::IntrType i) {
	return vpList.sendInterrupt(dspid, i);
    }

    virtual SysStatus sendRemoteIPC(CommID target, RemoteIPCBuffer *ipcBuf) {
	return vpList.sendRemoteIPC(target, ipcBuf);
    }

    virtual SysStatus sendRemoteAsyncMsg(CommID target, CommID source,
					 XHandle xhandle, uval methodnum,
					 uval length, uval *buf) {
	return vpList.sendRemoteAsyncMsg(target, source, xhandle, methodnum,
					 length, buf);
    }

    virtual SysStatusUval vpCount() { return vpList.vpCount(); }
    virtual SysStatusUval ppCount();

    virtual SysStatus testUserIPC(ObjectHandle oh);

    // dummy for testing base IPC performance
    virtual SysStatus testIPCPerf() { return 0; }

    virtual SysStatus getRUsage(BaseProcess::ResourceUsage& usage) {
         passertMsg(0, "not supported!");
         return 0;
    }

    // FIXME: a temporary test, delete when change test user-user
    static void DoUserIPC(uval ohArg);

    //The OS personality can use these functions to maintain a link
    //to personality data associated with the process
    virtual SysStatus setOSData(uval data) {
	OSData = data;
	return 0;
    }

    virtual SysStatus getOSData(uval& data) {
	data = OSData;
	return 0;
    }

    virtual SysStatus getNextRegion(uval& vaddr, RegionRef& regionRef) {
	return rlst.getNextRegion(vaddr, regionRef);
    }

    virtual SysStatus doPreFork(XHandle childXH, ProcessID callerPID) {
	return rlst.preFork(childXH, callerPID);
    }
    virtual SysStatus preExec() {
	return rlst.preExec();
    }

    virtual SysStatus findRegion(
	uval start, RegionType::RegionElementInfo& element) {
	return rlst.findRegion(start, element);
    }

    // was there before
    virtual SysStatus regionDestroy(uval regionAddr);
    // virtual SysStatus removeRegion(uval start); <- check above covers
    virtual SysStatus regionTruncate(uval start, uval length);
    // virtual SysStatus truncateRegion(uval start, uval length); <- check above


    // For the moment we use very restricted interface for MIP regions
    //  Only creates mip region in callerPID, 
    //  alignment must be on a page boundary, 
    //  fr is created internally
    //  offset is calculated from pre-existing region
    //  access is assumed to be read-write
    //  region type is hard coded will
    // will probably export more functionality to user space later.
    virtual SysStatus createMIP(uval &regionVaddr, uval regionSize, 
                                uval chunkSize, uval mipOptions) {
        return _SERROR(0,0,EINVAL);
    }
    virtual SysStatus destroyMIP(uval regionVaddr, uval mipOptions) {
        return _SERROR(0,0,EINVAL);
    }
    virtual SysStatus flushMIP(uval regionVaddr, uval regionSize,
                               uval mipOptions) {
        return _SERROR(0,0,EINVAL);
    }
    virtual SysStatus fetchMIP(uval regionVaddr, uval regionSize,  
                               uval mipOptions) {
        return _SERROR(0,0,EINVAL);
    }

    // Retrieves local SMT object for specified pid/key
    // Returns xhandle used by remote end
    // remoteSMT is also an input --- it sets the xhandle associated
    // with the pid/key if it is currently not valid.
    virtual SysStatus getLocalSMT(MemTransRef &mtr, XHandle &remoteSMT,
				  uval key) {
	return memTransMgr.getLocalSMT(mtr, remoteSMT, key);
    }

    virtual SysStatus getAddrLazyState(LazyState *&ls) {
	ls = &lazyState; return 0;
    }
    virtual SysStatus lazyReOpen(sval file, uval &fileType, ObjectHandle &oh,
				 char *data, uval &dataLen) {
	return lazyState.lazyReOpen(getPID(), file, fileType, oh, data,
				    dataLen);
    }
    virtual SysStatus lazyClose(sval file) {
	return lazyState.lazyClose(file);
    }
    virtual SysStatus lazyGiveAccess(sval file, uval type, ObjectHandle oh,
				     sval closeChain,
				     AccessRights match, AccessRights nomatch,
				     char *data, uval dataLen) {
	return lazyState.lazyGiveAccess(file, type, oh, closeChain, match,
					nomatch, data, dataLen);
    }
    virtual SysStatus lazyCopyState(XHandle target) {
	passertMsg(0, "called kernel\n");
	return 0;
    }
    virtual SysStatus lazyCopyState(LazyState *ls) {
	return lazyState.copyState(ls);
    }

protected:
    friend class MemTrans;
    virtual SysStatus removeSMT(MemTransRef mtr) {
	return memTransMgr.removeSMT(mtr);
    }

    virtual SysStatus addSMT(MemTransRef mtr, XHandle remoteSMT,
			     uval key) {
	return memTransMgr.addSMT(mtr, remoteSMT, key);
    }


__xpublic:
    virtual SysStatus _getMemTrans(__out ObjectHandle &oh,
				   __in  uval key,
				   __XHANDLE xhandle,
				   __CALLER_PID pid) {
	return memTransMgr.getMemTrans(oh, key, xhandle, pid);
    }

};

#endif /* #ifndef __PROCESS_SHARED_H_ */
