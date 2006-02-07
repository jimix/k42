#ifndef __PROCESS_SERVER_H_
#define __PROCESS_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessServer.H,v 1.27 2004/12/16 23:09:04 awaterl Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: interface for server side of process object
 * **************************************************************************/

#include <defines/experimental.H>
#include <sys/ProcessClient.H>

class ProcessServer : public ProcessClient
    __xadef(destroy)
{
protected:
    virtual SysStatus doPreFork(XHandle childXH, ProcessID callerPID)=0;

    virtual SysStatus preFork(XHandle childXH) {
#ifndef _IN_STUBGEN
	passertMsg(0, "client interface called in kernel\n");
#endif /* #ifndef _IN_STUBGEN */
	return 0;
    }
    
public:
    // overriding to get rid of client data
    static void BeingFreed(XHandle xhandle);

    // overriding to 1) do up-call, and 2) free client data
    virtual SysStatus handleXObjFree(XHandle xhandle);

#ifndef _IN_STUBGEN
    // class maintains per-client state
    class ClientData;
#endif /* #ifndef _IN_STUBGEN */

    // overriding to set up client data
    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch, TypeID type = 0);

    static void ClassInit(VPNum vp);

__xpublic:
    virtual SysStatus getType(TypeID &id);

    // call from other process to wait for process termination
    virtual SysStatus _waitForTermination() {
	return waitForTermination();
    }

    // Number of vp's currently allocated to this process,
    // (for the kernel, this is also the number of physical processors
    // available), and number of physical processors available.
    virtual SysStatusUval _vpCount() {
	return vpCount();
    }

    virtual SysStatusUval _ppCount() {
	return ppCount();
    }

    virtual SysStatus _userHandleFault(__in uval vaddr, __in VPNum vp) {
	return userHandleFault(vaddr, vp);
    }

    virtual SysStatus _sendInterrupt(__in DispatcherID dspid,
				     __in SoftIntr::IntrType i) {
	return sendInterrupt(dspid, i);
    }

    virtual SysStatus _regionDestroy(__in uval regionAddr) {
	return regionDestroy(regionAddr);
    }

    virtual SysStatus _regionTruncate(__in uval start, __in uval length) {
	return regionTruncate(start, length);
    }

    virtual SysStatus _preFork(__in XHandle childXH, __CALLER_PID callerPID) {
	return doPreFork(childXH, callerPID);
    }

    virtual SysStatus _preExec() {
	return preExec();
    }

    virtual SysStatus _findRegion(
	uval start, __out RegionType::RegionElementInfo& element) {
	return findRegion(start, element);
    }

    virtual SysStatus _unmapRange(__in uval start, __in uval size) {
	return unmapRange(start, size);
    }

    virtual SysStatus _createMIP(__inout uval &regionVaddr, 
                                 __in uval regionSize,
                                 __in uval chunkSize,  __in uval mipOptions) {
        return createMIP(regionVaddr, regionSize, chunkSize, mipOptions);
    }

    virtual SysStatus _destroyMIP(__in uval regionVaddr, __in uval mipOptions) {
        return destroyMIP(regionVaddr, mipOptions);
    }

    virtual SysStatus _flushMIP(__in uval regionVaddr, __in uval regionSize,
                                __in uval mipOptions) {
        return flushMIP(regionVaddr, regionSize, mipOptions);
    }

    virtual SysStatus _fetchMIP(__in uval regionVaddr, __in uval regionSize,
                                __in uval mipOptions) {
        return fetchMIP(regionVaddr, regionSize, mipOptions);
    }

    virtual SysStatus _kill(__CALLER_PID caller);

    virtual SysStatus _regress() {
	return regress();
    }

    virtual SysStatus _perfMon(__in uval action, __in uval ids) {
	return perfMon(action, ids);
    }

    virtual SysStatus _breakpoint() {
	return breakpoint();
    }

    // note, if making call on own process, cool to ignore error check
    virtual SysStatusProcessID _getPID() {
	return getPID();
    }

    // FIXME: a temporary test, delete when change test user-user
    virtual SysStatus _testUserIPC(__in ObjectHandle oh) {
	return testUserIPC(oh);
    }

    // dummy for testing base IPC performance
    virtual SysStatus _testIPCPerf() {
	return testIPCPerf();
    }

    virtual SysStatus _getRUsage(__out BaseProcess::ResourceUsage& usage) {
	return getRUsage(usage);
    }

__xprotected:

    // all the lazy stuff
    virtual SysStatus _lazyReOpen(__CALLER_PID caller, __in sval file, 
				  __out uval &type, __out ObjectHandle &oh, 
				  __outbuf(dataLen:512) char *data,
				  __out uval &dataLen)=0;
    virtual SysStatus _lazyClose(sval file)=0;
    virtual SysStatus _lazyGiveAccess(__in sval file, __in uval type, 
				      __in ObjectHandle oh, 
				      __in sval closeChain,
				      __in AccessRights match,
				      __in AccessRights nomatch,
				      __inbuf(dataLen) char *data, 
				      __in uval dataLen)=0;

    virtual SysStatus _lazyCopyState(__CALLER_PID caller, XHandle target)=0;

    static SysStatus _Create(__out ObjectHandle &oh, __in uval procType,
			     __inbuf(*) const char *name,
			     __out ProcessID &newProcID,
			     __CALLER_PID processID);

    virtual SysStatus _registerCallback(__in ObjectHandle callbackOH,
					__XHANDLE xhandle);

    virtual __async SysStatus _destructionCallback() { return 0; }
};

#endif /* #ifndef __PROCESS_SERVER_H_ */
