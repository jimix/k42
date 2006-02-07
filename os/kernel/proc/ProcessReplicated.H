#ifndef __PROCESS_REPLICATED_H_
#define __PROCESS_REPLICATED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessReplicated.H,v 1.70 2004/12/16 23:09:04 awaterl Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic default process structure.  At the moment, this
 *                     is the more sophisticated of two subclasses of the
 *                     Process class, the other is ProcessShared.  This class
 *                     is a clustered object, but for now we are using it
 *                     for everything, including Linux applications that are
 *                     single threaded.
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
#include <cobj/CObjRootMultiRep.H>
#include <cobj/Factory.H>
#include <sys/SMTMgr.H>
#include <defines/experimental.H>
#include <bilge/LazyState.H>

class ProcessReplicated : public Process {
public:
    DEFINE_FACTORY_BEGIN(Factory)
        virtual SysStatus create(ProcessRef& outPref, HATRef h, PMRef pm,
                                 ProcessRef creator, const char *name);
        virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
        virtual SysStatus destroy(ProcessRef ref);
    DEFINE_FACTORY_END(Factory)
protected:
    class Root : public CObjRootMultiRep {
    public:
	BLock         lock;
	RegionList<AllocGlobal> rlst;	// list of regions in program
	LazyState     lazyState;	// state lazily passed on fork/exec
	HATRef	      hatRefProcess;	// reference to the per-process hat
	class ProcessVPList vpList;	// list of per-vp information
	XObjectListProc matched;        // list of x objects with match rights
					// when called by this process
	PMRef         pmRef;		// PM for this process
	uval          OSData;           // Personality data
	SMTMgr	      memTransMgr;
        FactoryRef    factoryRef;       // factory that created this instance
        bool updatedFlagDoNotDestroy; /* hack */

	virtual CObjRep * createRep(VPNum vp);
	Root();
	Root(RepRef ref);
        Root(RepRef ref, sval c=1,
             CObjRoot::InstallDirective idir=CObjRoot::Install);

	DEFINE_GLOBALPADDED_NEW(Root);
        virtual SysStatus getDataTransferExportSet(DTTypeSet *set);
        virtual SysStatus getDataTransferImportSet(DTTypeSet *set);
        virtual DataTransferObject *dataTransferExport(DTType dtt,
						       VPSet transferVPSet);
        virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					     DTType dtt, VPSet transferVPSet);
    };
    friend class Root;
    Root *root() { return (Root *)myRoot; }


    // Not localstrict because remote rlsts are freed in detachRegion
    //RegionList<AllocLocalStrict> rlst; // local list of regions in program
    RegionList<AllocGlobal> rlst; // local list of regions in program

    struct BaseProcess::ResourceUsage rusage;

    struct ObjectHandleHolder {
	DEFINE_GLOBALPADDED_NEW(ObjectHandleHolder);
	ObjectHandle oh;
    };

    /*
     * split an exiting region to fit the new region specified
     */
    SysStatus splitRegion(uval newVaddr, uval newSize,
                          RegionRef newReg, 
                          RegionType::Type regionType);

public:
    DEFINE_GLOBALPADDED_NEW(ProcessReplicated);

    DEFINE_ACCESS_SERVER(
	ProcessServer,
	(MetaProcessServer::controlAccess|
	 MetaProcessServer::processOnly|
	 MetaProcessServer::attach|
	 MetaProcessServer::destroy),
	0);

    static SysStatus Create(ProcessRef& outPref, HATRef h, PMRef pm,
			    ProcessRef creator, const char *name);

    SysStatusProcessID getPID() {
	// LOCKING: read-only item accessed
	return _SRETPID(COGLOBAL(vpList).getPID());
    }

    virtual SysStatus lockMatchedXObjectList() {
	return COGLOBAL(matched).lockIfNotClosing();}
    virtual SysStatus unlockMatchedXObjectList() {
	COGLOBAL(matched).unlock(); return 0; }

    // attach x object to process for garbage collection
    virtual SysStatus addMatchedXObj(XHandle xhandle) {
	// LOCKING: see comment top of file
	COGLOBAL(matched).locked_add(xhandle); return 0;
    }

    virtual SysStatus removeMatchedXObj(XHandle xhandle) {
	// LOCKING: see comment top of file
	COGLOBAL(matched).locked_remove(xhandle); return 0;
    }

    virtual SysStatus closeMatchedXObjectList()
	{
	  return COGLOBAL(matched).close();
	}
    virtual SysStatus isClosedMatchedXObjectList()
	{
	  return COGLOBAL(matched).isClosed();
	}
    virtual SysStatus isEmptyMatchedXObjectList()
	{
	  return COGLOBAL(matched).isEmpty();
	}

    virtual SysStatus destroy();

    /* should only be called by Factory::destroy */
    virtual SysStatus reclaimSelf();

    virtual SysStatus getUserMode(uval& user) {
	user = COGLOBAL(vpList).getUserMode(); return 0;
    }

    virtual SysStatus vpnumToPpnum(VPNum vp, VPNum &pp) {
	return COGLOBAL(vpList).vpnumToPpnum(vp, pp);
    }

    virtual SysStatus getHATProcess(HATRef &ref) {
	// LOCKING: read-only data
	ref = COGLOBAL(hatRefProcess);  return 0;
    }

    virtual SysStatus getPM(PMRef &ref) {
	// LOCKING: read-only data
	ref = COGLOBAL(pmRef);  return 0;
    }

    /*
     * Set range for region allocations for this process.
     */
    virtual SysStatus setRegionsBounds(uval start, uval allocStart, uval end) {
	COGLOBAL(rlst).setRegionsBounds(start, allocStart, end);
	return 0;
    }

    /*
     * attach a region with address determined by regionList
     */
    virtual SysStatus attachDynamicRegion(
	uval &vaddr, uval size, RegionRef reg, 
	RegionType::Type regionType, uval alignment=0);


    /*
     * attach a region at fixed address
     */
    virtual SysStatus attachFixedRegion(
	uval vaddr, uval size, RegionRef reg, RegionType::Type regionType);

#ifdef PROTOTYPE_SUBLIST				
    /*
     * attach a region at fixed address
     */
    virtual SysStatus attachFixedSubRegion(uval vaddr, uval size,
                                           RegionRef reg,
                                           RegionType::Type regionType);
#endif

    /*
     * attach a region at within given bounds
     */
    virtual SysStatus attachWithinRangeRegion(
	uval &vaddr, uval vaddr2, uval size, RegionRef reg,
	RegionType::Type regionType, uval alignment=0);

    /*
     * deattach an existing region
     */
    virtual SysStatus detachRegion(RegionRef reg);

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
	cprintf("%s regions: ", COGLOBAL(vpList).getName());
	COGLOBAL(rlst).printRegions();
	return 0;
    }

    virtual SysStatus vaddrToRegion(uval a, RegionRef &reg) {
	// LOCKING: internal to region list
        //FIXME:  For MIP support added region spliting
        //        which means that region list and region can get out
        //        of sync with respect to address space coverage
        //        when a split is in progress so we must add retries :-(
        //        In general it is not safe to cache this info as a truncate
        //        or remap would make it stale.
        return COGLOBAL(rlst).vaddrToRegion(a, reg);
    }

    virtual void kosher();

    virtual SysStatus kill();

    virtual SysStatus regress();

    //FIXME - only in debug kernel
    //::breakpoint is the global scope routine, not the one in ProcessDefault
    virtual SysStatus breakpoint() {::breakpoint(); return 0;};

    virtual SysStatus userHandleFault(__in uval vaddr, __in VPNum vp);

    /**********************************************************************
      below are functions pulled into process to do user-level loading
      some of them maybe should be first class calls of their own while
      others will remain here
    ***********************************************************************/

    virtual SysStatus doPreFork(XHandle childXH, ProcessID callerPID) {
	return COGLOBAL(rlst).preFork(childXH, callerPID);
    }
    
    virtual SysStatus preExec() {
	// purge lookaside entries so preExec detachRegion
	// doesn't have to remove them one at a time
	// performance heuristic based on assumed use of preExec
	rlst.purge();
	return COGLOBAL(rlst).preExec();
    }

    virtual SysStatus findRegion(
	uval start, RegionType::RegionElementInfo& element) {
	return COGLOBAL(rlst).findRegion(start, element);
    }

    virtual SysStatus regionDestroy(__in uval regAddr);

    virtual SysStatus regionTruncate(__in uval start, __in uval length);


    virtual SysStatus createMIP(uval &regionVaddr, uval regionSize, 
                                uval chunkSize,   uval mipOptions);

    virtual SysStatus destroyMIP(uval regionVaddr, uval mipOptions);

    virtual SysStatus flushMIP(uval regionVaddr, uval regionSize,
                               uval mipOptions);

    virtual SysStatus fetchMIP(uval regionVaddr, uval regionSize,  
                               uval mipOptions); 

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
	return COGLOBAL(vpList).sendInterrupt(dspid, i);
    }

    virtual SysStatus sendRemoteIPC(CommID target, RemoteIPCBuffer *ipcBuf) {
	return COGLOBAL(vpList).sendRemoteIPC(target, ipcBuf);
    }

    virtual SysStatus sendRemoteAsyncMsg(CommID target, CommID source,
					 XHandle xhandle, uval methodnum,
					 uval length, uval *buf) {
	return COGLOBAL(vpList).sendRemoteAsyncMsg(target, source,
						   xhandle, methodnum,
						   length, buf);
    }

    virtual SysStatusUval vpCount() { return COGLOBAL(vpList).vpCount(); }
    virtual SysStatusUval ppCount();

    virtual SysStatus testUserIPC(ObjectHandle oh);

    // dummy for testing base IPC performance
    virtual SysStatus testIPCPerf() { return 0; }

    virtual SysStatus getRUsage(BaseProcess::ResourceUsage& usage);

    // FIXME: a temporary test, delete when change test user-user
    static void DoUserIPC(uval ohArg);

    //The OS personality can use these functions to maintain a link
    //to personality data associated with the process
    virtual SysStatus setOSData(uval data) {
	COGLOBAL(OSData) = data;
	return 0;
    }

    virtual SysStatus getOSData(uval& data) {
	data = COGLOBAL(OSData);
	return 0;
    }

    virtual SysStatus getNextRegion(uval& vaddr, RegionRef& regionRef) {
	return COGLOBAL(rlst).getNextRegion(vaddr, regionRef);
    }

    // Retrieves local SMT object for specified pid/key
    // Returns xhandle used by remote end
    // remoteSMT is also an input --- it sets the xhandle associated
    // with the pid/key if it is currently not valid.
    virtual SysStatus getLocalSMT(MemTransRef &mtr, XHandle &remoteSMT,
				  uval key) {
	return COGLOBAL(memTransMgr).getLocalSMT(mtr, remoteSMT, key);
    }

    virtual SysStatus getAddrLazyState(LazyState *&ls) {
	ls = &COGLOBAL(lazyState); return 0;
    }
    virtual SysStatus lazyReOpen(sval file, uval &fileType, ObjectHandle &oh,
				 char *data, uval &dataLen) {
	return COGLOBAL(lazyState).lazyReOpen(getPID(), file, fileType, oh,
					      data, dataLen);
    }
    virtual SysStatus lazyClose(sval file) {
	return COGLOBAL(lazyState).lazyClose(file);
    }
    virtual SysStatus lazyGiveAccess(sval file, uval type, ObjectHandle oh,
				     sval closeChain,
				     AccessRights match, AccessRights nomatch,
				     char *data, uval dataLen) {
	return COGLOBAL(lazyState).lazyGiveAccess(file, type, oh, closeChain,
						  match, nomatch,
						  data, dataLen);
    }
    virtual SysStatus lazyCopyState(XHandle target) {
	passertMsg(0, "called kernel\n");
	return 0;
    }
    virtual SysStatus lazyCopyState(LazyState *ls) {
	return COGLOBAL(lazyState).copyState(ls);
    }

protected:
    friend class MemTrans;
    virtual SysStatus removeSMT(MemTransRef mtr) {
	return COGLOBAL(memTransMgr).removeSMT(mtr);
    }

    virtual SysStatus addSMT(MemTransRef mtr, XHandle remoteSMT,
			     uval key) {
	return COGLOBAL(memTransMgr).addSMT(mtr, remoteSMT, key);
    }


__xpublic:
    virtual SysStatus _getMemTrans(__out ObjectHandle &oh,
				   __in  uval key,
				   __XHANDLE xhandle,
				   __CALLER_PID pid) {
	return COGLOBAL(memTransMgr).getMemTrans(oh, key, xhandle, pid);
    }

};

#endif /* #ifndef __PROCESS_REPLICATED_H_ */
