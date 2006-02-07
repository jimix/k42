#ifndef __RES_MGR_H_
#define __RES_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ResMgr.H,v 1.26 2004/01/11 20:27:07 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to service for registering and
 * looking up mount points.
 * FIXME: we need a security model, i.e., what gives a file system the right
 * to register in some part of the name space, what happens when there is
 * something already there... for now, first come first serve, and error if
 * you try to register in a already existing part of the space.
 * **************************************************************************/

#include <meta/MetaResMgr.H>
#include <cobj/CObjRootMultiRep.H>
#include <sys/KernelScheduler.H>
#include <scheduler/Scheduler.H>
#include "PrivilegedServiceWrapper.H"
#include <stub/StubCPUContainer.H>
#include <misc/HashNonBlocking.H>
#include <sync/FairBLock.H>

// FIXME: fix stub-compiler grammar so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#define FIXME_MAXPROCS 24

// FIXME should this go in a more general place
typedef uval UserID;	

class ResMgr;
typedef ResMgr **ResMgrRef;

class ResMgr : public Obj {
    ResMgr();
    static ResMgrRef TheResourceManager;

#ifndef _IN_STUBGEN
    class ResourceDomain {
    public:
	uval priorityClass;
	uval weight;
	uval quantumMicrosecs;
	uval pulseMicrosecs;
	// FIXME this should be distributed
	ObjectHandle CPUContainerOH[FIXME_MAXPROCS];
	DEFINE_GLOBAL_NEW(ResourceDomain);

	static ResourceDomain *createRD(uval p, uval w, uval q, uval pu) {
	    ResourceDomain *rd;
	    uval i;
	    rd = new ResourceDomain();
	    rd->priorityClass = p;
	    rd->weight = w;
	    rd->quantumMicrosecs = q;
	    rd->pulseMicrosecs = pu;
	    for (i=0; i<FIXME_MAXPROCS; i++) {
		rd->CPUContainerOH[i].init();
	    }
	    return rd;
	}
    };

    class UIDInfo {
    private:
	sval PIDCount; // number of PIDs associated with this UID
    public:
	ResourceDomain *resourceDomain[Scheduler::RDLimit];
	FairBLock checkLock, ppLock;
  
	UserID uid;
 
	uval PPCount; // how many pps this uid is spread between
	uval PPPrim;  // primary pp - prob only one

	// represent whether this uid exist on this processor and how many
	// dispatchers it has on this processor
	uval PPDispCount[FIXME_MAXPROCS];

	DEFINE_GLOBAL_NEW(UIDInfo);

	static UIDInfo *createInitUIDInfo() {
	    UIDInfo *uidInfo;
	    uval i; 

	    uidInfo = new UIDInfo();
	    for (i=0; i<Scheduler::RDLimit; i++) {
		uidInfo->resourceDomain[i] = NULL;
	    }
	    uidInfo->PIDCount = 1;
	    uidInfo->uid = (uval)-1;
	    uidInfo->PPPrim = (uval)-1;
	    uidInfo->PPCount = 0;
	    for (i=0; i<FIXME_MAXPROCS; i++) {
		uidInfo->PPDispCount[i] = 0;
	    }
	    uidInfo->checkLock.init();
	    uidInfo->ppLock.init();
	    return uidInfo;
	}

	sval countPID(sval increment) {
	    sval old;
	    old = FetchAndAddSigned(&PIDCount, increment);
	    return old + increment;
	}
    };

    typedef UIDInfo *UIDInfoPtr;

    class TimerEventPreempt;

    // class maintains per-client state specifically pointer to processInfo
    class ClientData;

    //FIXME we really should have a UP class and MP class
    //      because most pids will be UP then hop swap to MP
    //      when we discover an additional vp creation
    class PIDInfo {
    public:
	enum {NOEXIST = 0, EXIST = 1};
	UserID uid;
	ProcessID pid;			// makes debugging easier
	ObjectHandle wrapperOH;
	VPNum VPtoPP[Scheduler::VPLimit];
	uval PPCount[FIXME_MAXPROCS];
	// FIXME is it worth making this a bit array?
	uval8 dispGrid[Scheduler::VPLimit][Scheduler::RDLimit];
	FairBLock updateLock;

	DEFINE_GLOBAL_NEW(PIDInfo);

	static PIDInfo *createInitPIDInfo() {
	    PIDInfo *pidInfo;
	    uval i,j; 

	    pidInfo = new PIDInfo();
	    for (i=0; i<Scheduler::VPLimit; i++) {
		pidInfo->VPtoPP[i] = VPNum(-1);
		pidInfo->PPCount[i] = 0;
		pidInfo->uid = (uval)-1;
		for (j=0; j<Scheduler::RDLimit; j++) {
		    pidInfo->dispGrid[i][j] = NOEXIST;
		}
		pidInfo->updateLock.init();
	    }
	    pidInfo->wrapperOH.init();
	    return pidInfo;
	}

	void enterDSPID(DispatcherID dspid) {
	    RDNum rd; VPNum vp; SysTypes::UNPACK_DSPID(dspid, rd, vp);
	    dispGrid[vp][rd] = EXIST;
	}
	void enterDSPID(RDNum rd, VPNum vp) {
	    dispGrid[vp][rd] = EXIST;
	}
	void removeDSPID(DispatcherID dspid) {
	    RDNum rd; VPNum vp; SysTypes::UNPACK_DSPID(dspid, rd, vp);
	    dispGrid[vp][rd] = NOEXIST;
	}
	void removeDSPID(RDNum rd, VPNum vp) {
	    dispGrid[vp][rd] = NOEXIST;
	}
	uval queryDSPID(DispatcherID dspid) {
	    RDNum rd; VPNum vp; SysTypes::UNPACK_DSPID(dspid, rd, vp);
	    return (dispGrid[vp][rd]);
	}
	uval queryDSPID(RDNum rd, VPNum vp) {
	    return (dispGrid[vp][rd]);
	}
    };


    typedef PIDInfo *PIDInfoPtr;

    class ProcessorInfo {
    public:
	uval runDispCount[FIXME_MAXPROCS];
	uval runWeightAccum[FIXME_MAXPROCS][KernelScheduler::NUM_PRIORITY_CLASSES];
	uval idleTimeAccum[FIXME_MAXPROCS];
	// how many uid domains have been put on this processor
	uval assignedDomains[FIXME_MAXPROCS];
	uval maxIdle;
	FairBLock findPPLock;

	DEFINE_GLOBAL_NEW(ProcessorInfo);

	static ProcessorInfo *createProcessorInfo() {
	    ProcessorInfo *procInfo;
	    uval i,j; 

	    procInfo = new ProcessorInfo();
	
	    for (i=0; i<FIXME_MAXPROCS; i++) {
		procInfo->runDispCount[i] = 0;
		procInfo->idleTimeAccum[i] = 0;
		procInfo->assignedDomains[i] = 0;
		for (j=0; j<KernelScheduler::NUM_PRIORITY_CLASSES; j++) {
		    procInfo->runWeightAccum[i][j] = 0;
		}
	    }
	    procInfo->findPPLock.init();
	    procInfo->maxIdle = 0;
	    return procInfo;
	}
    };

#else /* #ifndef _IN_STUBGEN */
    struct UIDInfo;
    struct PIDInfo;
    struct ProcessorInfo;
#endif /* #ifndef _IN_STUBGEN */

protected:
#ifndef _IN_STUBGEN
    class ResMgrRoot : public CObjRootMultiRep {
    public:
	typedef BLock LockType;
	LockType   lock;

	ProcessID lastGlobalPP;

	KernelScheduler::Stats *ksStats[Scheduler::VPLimit];
	ProcessorInfo *processorInfo;

	SysTime startTime;
	uval ResMgrStatsFlag;

	ObjectHandle dispatchQueueStatsFROH;
	uval dispatchQueueStatsRegionAddr;
	uval dispatchQueueStatsRegionSize;
	uval dispatchQueueStatsSize;
	
	//FIXME - this lock can be replaced with using the non-blocking
	//        hash support for atomic add/remove of locked elements
	LockType UIDLock;		// protect add/remove
        // FIXME why can't this be larger like 64 - causes compiler error
	enum {initSize = 8};
	enum {hashShift = 0};
	HashSimpleNonBlocking
	<ProcessID, PIDInfoPtr, AllocGlobal, hashShift, initSize> PIDSet;
	HashSimpleNonBlocking
	<UserID, UIDInfoPtr, AllocGlobal, hashShift, initSize> UIDSet;

	virtual CObjRep * createRep(VPNum vp);
	ResMgrRoot();
	ResMgrRoot(RepRef ref);

	DEFINE_GLOBALPADDED_NEW(ResMgrRoot);
    };
    friend class ResMgr::ResMgrRoot;
    ResMgrRoot *root() { return (ResMgrRoot *)myRoot; }
#else /* #ifndef _IN_STUBGEN */
    struct ResMgrRoot;  // needed for declarations later in this file
#endif /* #ifndef _IN_STUBGEN */

    struct AcceptVPMsg;
    struct CreateDispatcherMsg;

    virtual SysStatus acceptVP(ProcessID pid, VPNum vp);
    SysStatus migrateVP(ProcessID pid, VPNum vp, VPNum pp, uval accountingDone);
    VPNum findPP(ProcessID pid, uval uid);
    void checkAddAssignedDomains(VPNum pp, UIDInfo *uidInfo, uval loc);
    void subAndCheckAssignedDomains(VPNum pp, UIDInfo *uidInfo, uval loc);

    static UserID getUID(ProcessID pid);
    void checkAndMigrate();

    virtual SysStatus createDispatcherLocal(DispatcherID dspid,
					    EntryPointDesc entry,
					    UIDInfo *uidInfo,
					    uval dispatcherAddr,
					    uval initMsgLength, char *initMsg,
					    ProcessID caller);

private:
#ifndef _IN_STUBGEN
    PrivilegedServiceRef privilegedServiceRef;
#endif

    static uval PPCount;
    static uval NextPP;

    virtual SysStatus moveDomain(RDNum rd, ProcessID caller, PIDInfo *pidInfo,
				 UIDInfo *uidInfo, UIDInfo *oldUIDInfo);
    virtual SysStatus finishInit();
    virtual void checkCreateCPUContainer(RDNum rd, UIDInfo *uidInfo);
    virtual void removeUID(UIDInfo* uidInfo);
    virtual SysStatus registerFirstDispatcher(
	ObjectHandle &oh, ProcessID caller);
    virtual SysStatus getPidOH(ObjectHandle &oh, ProcessID caller);

public:
    DEFINE_GLOBALPADDED_NEW(ResMgr);

    /*
     * do not allow giveAccess/releaseAccess by
     * not giving controlAccess rights.  Correct use counting
     * depends on this.
     */
    DEFINE_ACCESS_SERVER(
	ResMgr,MetaObj::processOnly,MetaObj::none);

    // overriding to free process Info associated with client data
    virtual SysStatus handleXObjFree(XHandle xhandle);
    static void BeingFreed(XHandle xhandle);
    virtual SysStatus beingFreed(XHandle xhandle);
    virtual SysStatus updateLoadEstimates();
    virtual SysStatus adjustLoadEstimates(VPNum fromPP, VPNum toPP);

    void init();

    static void ClassInit();


__xpublic:
    static SysStatus _Create(__out ObjectHandle &trmOH, __CALLER_PID caller);

    static SysStatus _CreateAndRegisterFirstDispatcher(
	__out ObjectHandle &trmOH, __CALLER_PID caller);

    virtual SysStatus _assignDomain(__in uval uid, __CALLER_PID caller);

    virtual SysStatus _createFirstDispatcher(__in ObjectHandle childOH,
					     __in EntryPointDesc entry,
					     __in uval dispatcherAddr,
					     __in uval initMsgLength,
					     __inbuf(initMsgLength)
							 char *initMsg,
					     __CALLER_PID caller);

    virtual SysStatus _createDispatcher(__in DispatcherID dspid,
				        __in EntryPointDesc entry,
				        __in uval dispatcherAddr,
				        __in uval initMsgLength,
				        __inbuf(initMsgLength)
						    char *initMsg,
				        __CALLER_PID caller);

    virtual SysStatus _execNotify(__CALLER_PID caller);

    // FIXME:  this interface to the res mgr is for testing and should go away.
    virtual SysStatus _migrateVP(__in VPNum vpNum,
				 __in VPNum suggestedPP,
				 __CALLER_PID caller);

    // FIXME:  restrict this call to privileged clients.
    virtual SysStatus _accessKernelSchedulerStats(__out ObjectHandle &statsFROH,
						  __out uval &statsRegionSize,
						  __out uval &statsSize,
						  __CALLER_PID caller);
    virtual SysStatus _setStatsFlag(__in uval val, __CALLER_PID caller);
    virtual SysStatus _toggleStatsFlag(__CALLER_PID caller);
};

#endif /* #ifndef __RES_MGR_H_ */
