#ifndef __PROCESS_VPLIST_H_
#define __PROCESS_VPLIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessVPList.H,v 1.34 2003/06/04 14:18:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Maintains per-vp information for a process
 * **************************************************************************/

#include "exception/ProcessAnnex.H"
#include "exception/CPUDomainAnnex.H"
#include "mem/HAT.H"
#include <sync/MPMsgMgr.H>
#include "sync/RequestCount.H"
#include <scheduler/Scheduler.H>

class ProcessVPList {
private:

    // local forward declarations of structures

    struct DeleteVPMsg;

    uval userMode;			// process runs in user mode?
    uval isKern;			// process is kernel?
    static uval const MAX_NAME_LEN = 64;
    char name[MAX_NAME_LEN];		// string name of program
    ProcessID processID;		// process ID for this process

    /*
     * Locking:
     *     This object maintains a VPInfo structure to represent each VP that
     *     has been created.  Pointers to these structures are kept in an
     *     array that is expanded dynamically as necessary.  A
     *     RequestCountWithStop guards all accesses to this array and the
     *     structures to which it points.  Changes to the array itself (which
     *     happen only when new VPs are added) are made with the requests
     *     count stop()'d.  Lookups using the array are made with the requests
     *     count enter()'d.  The counter guards both types of access against
     *     destruction.
     *
     *     Each VPInfo structure has its own lock, which is used to serialize
     *     the creation of dispatchers on the VP.  It is also used during VP
     *     migration ({detach,attach}Dispatcher) and to serialize access to a
     *     dispatcher's remote async IPC buffer.  (A finer-grained lock could
     *     be used for the latter purpose, but probably isn't necessary.)
     *     Operations that use the VPInfo structure simply to map a
     *     DispatcherID to a ProcessAnnex pointer need not acquire the VPInfo
     *     lock.  The requests count is sufficient to guarantee that the
     *     target ProcessAnnex will not go away.
     *
     *     Dispatchers (and associated ProcessAnnexes) are in general bound to
     *     particular physical processors.  (The only exception occurs during
     *     VP migration.)  A dispatcher/ProcessAnnex may be manipulated at
     *     exception level (interrupts disabled) on its processor without
     *     locking or other constraint.  Non-exception-level code can
     *     manipulate a local ProcessAnnex if it first disables interrupts.
     *     Such code must either hold the VPInfo lock to ensure that the
     *     ProcessAnnex stays local, or it must re-check the location after
     *     disabling interrupts.
     *
     *     Certain atomic operations (setting soft-interrupt bits, attaching
     *     a remote IPC buffer, etc.) can be applied to a
     *     ProcessAnnex/dispatcher whether it's local or remote, or even if
     *     it's in limbo during migration.  All that's required is a guarantee
     *     that the dispatcher isn't disappearing.  Several mechanisms provide
     *     such a guarantee:  1) A successful enter() or stop() operation on
     *     the ProcessVPList requests count guarantees that no existing
     *     dispatchers in the process will disappear.  2) The existence of a
     *     reserved thread for a ProcessAnnex guarantees that that annex will
     *     not disappear before the reserved thread terminates.  3) The
     *     existence of active PageFaultNotification objects for a ProcessAnnex
     *     guarantees that the annex will not disappear before the outstanding
     *     page faults are resolved.
     */

    RequestCountWithStop requests;

    struct DspInfo {
	ProcessAnnex *pa;
    };

    struct VPInfo {
	BLock lock;
	VPNum pp;
	uval dspCounter;
	DspInfo dspInfo[Scheduler::RDLimit];

	DEFINE_GLOBALPADDED_NEW(VPInfo);

	void init(VPNum ppArg) {
	    lock.init();
	    pp = ppArg;
	    dspCounter = 0;
	    for (uval rd = 0; rd < Scheduler::RDLimit; rd++) {
		dspInfo[rd].pa = NULL;
	    }
	}

	void deleteDispatchers();
    };

    struct DspTable {
	VPInfo *vpInfo[1];
    };

    DspTable *dspTable;
    uval vpLimit;
    uval vpCounter;			// number of vp's allocated

    // Embed space for a 1-VP process directly in this object.
    DspTable dspTableInitial;
    VPInfo vpInfo0;

    SysStatus findProcessAnnex(RDNum rd, VPNum vp,
			       VPInfo* &vpInfo, ProcessAnnex* &pa);

    SysStatus archAllocDispatcherPage(uval dspAddr, uval &dspAddrKern);

    // The kernel process uses specialized interfaces.
    friend class ProcessDefaultKern;
    SysStatus sendInterruptKernel(DispatcherID dspid, SoftIntr::IntrType i);
    SysStatus sendRemoteIPCKernel(CommID target, RemoteIPCBuffer *ipcBuf);
    SysStatus sendRemoteAsyncMsgKernel(CommID target, CommID source,
				       XHandle xhandle, uval methodnum,
				       uval length, uval *buf);

public:
    char *getName() { return name; }
    ProcessID getPID() { return processID; }
    uval getUserMode() { return userMode; }

    SysStatus vpnumToPpnum(VPNum vp, VPNum &pp);

    SysStatus createDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
			       EntryPointDesc entry, uval dispatcherAddr,
			       uval initMsgLength, char *initMsg,
			       ProcessRef procRef, HATRef hatRef);
    SysStatus detachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
			       HATRef hatRef);
    SysStatus attachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
			       HATRef hatRef);

    SysStatus sendInterrupt(DispatcherID dspid, SoftIntr::IntrType i);

    SysStatus sendRemoteIPC(CommID target, RemoteIPCBuffer *ipcBuf);

    SysStatus sendRemoteAsyncMsg(CommID target, CommID source,
				 XHandle xhandle, uval methodnum,
				 uval length, uval *buf);

    void deleteAll();

    void init(uval userMode, uval isKern, const char *name, ProcessID pid);
    void initKern(uval ppCount);

    SysStatusUval vpCount() { return _SRETUVAL(vpCounter); }
};

#endif /* #ifndef __PROCESS_VPLIST_H_ */
