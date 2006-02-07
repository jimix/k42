#ifndef __PROCESS_WRAPPER_H_
#define __PROCESS_WRAPPER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessWrapper.H,v 1.98 2005/08/11 20:20:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for a kernel process
 * **************************************************************************/

#include <sys/ProcessClient.H>
#include <stub/StubProcessServer.H>
#include <sys/ProcessServer.H>
#include <meta/MetaProcessClient.H>
#include <sync/BLock.H>
#include <scheduler/Scheduler.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <io/MemTrans.H>
#include <sys/SMTMgr.H>
#include <defines/experimental.H>

class ProcessWrapper : public ProcessClient {
    ProcessID pid;
    StubProcessServer stub;

    XObjectListProc matched;
    uval            OSData;
    SMTMgr	    memTransMgr;
    SysStatus init(ObjectHandle oh, ProcessID myPID);
    SysStatus init(ObjectHandle oh);
    SysStatus registerCallback();



protected:
    DEFINE_PRIMITIVE_NEW(ProcessWrapper);
    DEFINE_GLOBAL_NEW(ProcessWrapper);

    ProcessWrapper() : stub(StubObj::UNINITIALIZED) { /* empty body */ }

public:
    DEFINE_ACCESS_SERVER(ProcessClient,
			 MetaObj::controlAccess|MetaProcessClient::destroy,
			 MetaProcessClient::search);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch){
	return stub._giveAccess(oh, toProcID, match, nomatch);
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh,
					 ProcessID toProcID) {
	return stub._giveAccess(oh, toProcID);
    }

    static void InitMyProcess(VPNum vp, ObjectHandle oh, ProcessID myPID,
	MemoryMgrPrimitive *pa);

    // values have changed in fork child
    virtual SysStatus setOH(ObjectHandle oh);
    virtual SysStatus postFork(ProcessID myPID);

    static SysStatus Create(BaseProcessRef &ref, ProcessID processID);

    virtual SysStatus destroy();

    // creates a wrapper object to represent the kernel's process
    static void CreateKernelWrapper(VPNum vp);

    //Create a Wrapper for a new process, and create the process
    static SysStatus Create(BaseProcessRef &ref, uval procType,
			    const char *name);

    // creates a wrapper for a specified process object handle
    static SysStatus Create(BaseProcessRef &ref, uval procType,
			    ObjectHandle oh);

    SysStatusProcessID getPID() { return pid; }

    virtual SysStatus lockMatchedXObjectList() {
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
	tassert((pid != _KERNEL_PID), 
		err_printf("registering kernel: this results in a "
			   " performance bottleneck, so we removed for now\n"));
#endif
	return matched.lockIfNotClosing();
    }

    virtual SysStatus unlockMatchedXObjectList() {matched.unlock(); return 0; }

    // attach x object to process for garbage collection
    virtual SysStatus addMatchedXObj(XHandle xhandle);
    virtual SysStatus removeMatchedXObj(XHandle xhandle);
    virtual SysStatus closeMatchedXObjectList() {
	return matched.close(); }
    virtual SysStatus isClosedMatchedXObjectList() {
	return matched.isClosed(); }
    virtual SysStatus isEmptyMatchedXObjectList()  {
	return matched.isEmpty(); }

    // call from other process to wait for process termination
    SysStatus waitForTermination();

    SysStatus kill();

    SysStatus regress();

    virtual SysStatus perfMon(uval action, uval ids);

    SysStatus breakpoint();

    SysStatus userHandleFault(uval vaddr, VPNum vp);

    SysStatus unmapRange(uval start, uval size);

    virtual SysStatus getProcessHandle(ObjectHandle& oh) {
	oh = stub.getOH();
	return 0;
    }

    virtual SysStatus sendInterrupt(DispatcherID dspid, SoftIntr::IntrType i);

    virtual SysStatusUval vpCount() { return stub._vpCount();}
    virtual SysStatusUval ppCount() { return stub._ppCount();}

    // FIXME: a temporary test, delete when change test user-user
    virtual SysStatus testUserIPC(ObjectHandle oh) {
	return stub._testUserIPC(oh);
    }
    // dummy for testing base IPC performance
    virtual SysStatus testIPCPerf() { return stub._testIPCPerf(); }

    virtual SysStatus getRUsage(BaseProcess::ResourceUsage& usage) {
        return stub._getRUsage(usage);
    }

    // called by kernel when process this represents is going away
    virtual SysStatus _destructionCallback();

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
	passert(0, err_printf("NYI\n"));
	return 0;
    }

    // Retrieves local SMT object for specified pid/key
    // Returns xhandle used by remote end
    // remoteSMT is also an input --- it sets the xhandle associated
    // with the pid/key if it is currently not valid.
    virtual SysStatus getLocalSMT(MemTransRef &mtr, XHandle &remoteSMT,
				  uval key){
	return memTransMgr.getLocalSMT(mtr, remoteSMT, key);
    }

    // all the lazy stuff
    virtual SysStatus lazyReOpen(sval file, uval &type, ObjectHandle &oh, 
				 char *data, uval &dataLen);
    virtual SysStatus lazyClose(sval file);
    virtual SysStatus lazyGiveAccess(sval file, uval type, ObjectHandle oh, 
				     sval closeChain,
				     AccessRights match, AccessRights nomatch,
				     char *data, uval dataLen);
    virtual SysStatus lazyCopyState(XHandle target); // copy to target

    // -------------- region list operations ------------------
    virtual SysStatus preFork(ProcessID childPID);
    virtual SysStatus preExec();

    virtual SysStatus findRegion(uval start,
				 RegionType::RegionElementInfo& element);

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
        return stub._createMIP(regionVaddr, regionSize, chunkSize, mipOptions);
    }
    virtual SysStatus destroyMIP(uval regionVaddr, uval mipOptions) {
        return stub._destroyMIP(regionVaddr, mipOptions);
    }
    virtual SysStatus flushMIP(uval regionVaddr, uval regionSize,
                               uval mipOptions) {
        return stub._flushMIP(regionVaddr, regionSize, mipOptions);
    }
    virtual SysStatus fetchMIP(uval regionVaddr, uval regionSize,  
                               uval mipOptions) {
        return stub._fetchMIP(regionVaddr, regionSize, mipOptions);
    }
    // -------------- end region list operations --------------

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
				   __CALLER_PID pid){
	return memTransMgr.getMemTrans(oh, key, xhandle, pid);
    }
};
#endif /* #ifndef __PROCESS_WRAPPER_H_ */
