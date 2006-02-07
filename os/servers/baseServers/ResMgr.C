/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ResMgr.C,v 1.44 2005/07/25 20:57:48 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Resource Manager
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/XHandleTrans.H>
#include <usr/ProgExec.H>
#include <mem/Access.H>
#include <stub/StubRegionDefault.H>
#include <sys/ProcessLinux.H>
#include "ResMgr.H"
#include <sys/ResMgrWrapper.H>
#include <stdio.h>
#include <trace/traceResMgr.h>

#define FIXMEVAL 68

#define TMP_DBG 0

/*static*/ uval ResMgr::PPCount = 0;

/*static*/ ResMgrRef ResMgr::TheResourceManager = NULL;

#define RES_SLEEP_TIME (Scheduler::TicksPerSecond()*1)
class ResMgr::TimerEventPreempt : public TimerEvent {
public:
    DEFINE_GLOBAL_NEW(TimerEventPreempt);

    virtual void handleEvent() {
	SysStatus rc;
	tassert(Scheduler::IsDisabled(),
		err_printf("Dispatcher not disabled.\n"));

	rc = DREF(TheResourceManager)->updateLoadEstimates();
	disabledScheduleEvent(RES_SLEEP_TIME, TimerEvent::relative);
    }
};

ResMgr::ResMgrRoot::ResMgrRoot()
{
  /* empty body */
}

ResMgr::ResMgrRoot::ResMgrRoot(RepRef ref)
    : CObjRootMultiRep(ref)
{
    /* empty body */
}

ResMgr::ResMgr()
{
}

CObjRep *
ResMgr::ResMgrRoot::createRep(VPNum vp)
{
#if TMP_DBG
    cprintf("in res mgr createRep\n");
#endif
    ResMgr *rep = new ResMgr;
    rep->init();

    return (CObjRep *) rep;
}

void
ResMgr::init()
{
#if TMP_DBG
    cprintf("in res mgr init\n");
#endif

    privilegedServiceRef = PrivilegedServiceWrapper::ThePrivilegedServiceRef();

    TimerEventPreempt *p = new TimerEventPreempt;
    p->scheduleEvent(RES_SLEEP_TIME, TimerEvent::relative);

}

/* static */ void
ResMgr::ClassInit()
{
    SysStatus rc;
    ObjectHandle statsFROH;
    uval statsSize, statsRegionSize, statsRegionAddr;
    ObjectHandle oh;

#if TMP_DBG
    cprintf("in res mgr class init\n");
#endif

    passertMsg(FIXME_MAXPROCS >= KernelInfo::MaxPhysProcs(),
	       "FIXME MAXPROCS broken fixme\n");

    PPCount = DREFGOBJ(TheProcessRef)->ppCount();

    ResMgrRoot *newResMgrRoot = new ResMgrRoot;

    newResMgrRoot->lastGlobalPP = 0;
    newResMgrRoot->ResMgrStatsFlag = 0;
    newResMgrRoot->lock.init();
    newResMgrRoot->startTime = Scheduler::SysTimeNow();

    // Map in the kernel's KernelSchedulerStats region.
    rc = DREF(PrivilegedServiceWrapper::ThePrivilegedServiceRef())->
	    accessKernelSchedulerStats(statsFROH, statsRegionSize, statsSize);
    passertMsg(_SUCCESS(rc),
	       "Failed to get access to KernelSchedulerStats.\n");

    rc = StubRegionDefault::_CreateFixedLenExt(
	statsRegionAddr, statsRegionSize, 0, statsFROH, 0,
	AccessMode::readUserReadSup, 0, RegionType::K42Region);
    passertMsg(_SUCCESS(rc), "Failed to map KernelSchedulerStats region.\n");
    newResMgrRoot->dispatchQueueStatsFROH = statsFROH;
    newResMgrRoot->dispatchQueueStatsRegionAddr = statsRegionAddr;
    newResMgrRoot->dispatchQueueStatsRegionSize = statsRegionSize;
    newResMgrRoot->dispatchQueueStatsSize = statsSize;
    for (VPNum pp = 0; pp < PPCount; pp++) {
	newResMgrRoot->ksStats[pp] = (KernelScheduler::Stats *)
					(statsRegionAddr + (pp * statsSize));
    }
    for (VPNum pp = PPCount; pp < Scheduler::VPLimit; pp++) {
	newResMgrRoot->ksStats[pp] = NULL;
    }

    newResMgrRoot->processorInfo = ProcessorInfo::createProcessorInfo();
    TheResourceManager = (ResMgrRef)newResMgrRoot->getRef();

    DREF(TheResourceManager)->finishInit();
    
    // Now we're ready for the world to know about us.
    MetaResMgr::init();

#if (TMP_DBG>=2)
    cprintf("resource manager initialized\n");
#endif

    // Now create an object handle for our own wrapper to use, and create
    // the wrapper.
    DREF(TheResourceManager)->registerFirstDispatcher(
	oh, DREFGOBJ(TheProcessRef)->getPID());
    ResMgrWrapper::Create(oh);

}

/*virtual*/ void
ResMgr::checkCreateCPUContainer(RDNum rd, UIDInfo *uidInfo)
{
    ObjectHandle lCPUContOH;
    SysStatus rc;


    TraceOSResMgrEnterCPUCont(rd);
#if (TMP_DBG>=2)
    err_printf("%s: rd %ld uid %ld\n", __func__, rd, uidInfo->uid);
#endif

    if ((uidInfo->resourceDomain[rd]->
	 CPUContainerOH[Scheduler::GetVP()]).valid()) {
	    TraceOSResMgrExitCPUCont(0, rd);
	    return;
    }

    // delay acquiring the lock until here since most cases we will
    // return in the above check

    uidInfo->checkLock.acquire();
    // now that we have lock, verify that this really doesn't exist

    if ((uidInfo->resourceDomain[rd]->
	 CPUContainerOH[Scheduler::GetVP()]).valid()) {
	    TraceOSResMgrExitCPUCont(1, rd);
	    return;
    }

#if (TMP_DBG>=3)
    err_printf("checkCreateA rd %ld uid %ld\n",rd, uidInfo->uid);
#endif

    rc = DREF(privilegedServiceRef)->
      createCPUContainer(lCPUContOH, uidInfo->resourceDomain[rd]->priorityClass,
			 uidInfo->resourceDomain[rd]->weight,
			 uidInfo->resourceDomain[rd]->quantumMicrosecs,
			 uidInfo->resourceDomain[rd]->pulseMicrosecs);
    uidInfo->resourceDomain[rd]->CPUContainerOH[Scheduler::GetVP()]=lCPUContOH;

    uidInfo->checkLock.release();

#if (TMP_DBG>=3)
    err_printf("checkCreating rd %ld for uid %ld on %ld oh %lx\n", rd,
	       uidInfo->uid,Scheduler::GetVP(), (uval)(&lCPUContOH));
#endif

    tassertMsg(_SUCCESS(rc), "createCPUContainer() failed.\n");
    TraceOSResMgrExitCPUCont(2, rd);
}

/*virtual*/ SysStatus
ResMgr::finishInit()
{
    UIDInfo *uidInfo;
    uval aval;

    // create uidInfo for userid 0 - init, root, etc.
    uidInfo = UIDInfo::createInitUIDInfo();

    // we may not need all these but since it's for root create them
    for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
	uidInfo->resourceDomain[rd] = ResourceDomain::createRD(
	    KernelScheduler::PRIORITY_CLASS_TIME_SHARING, 64, 10000, 10000);
    }
    uidInfo->uid = 0;

#if (TMP_DBG>=2)
    err_printf("about to add uid 0\n");
#endif

    aval = COGLOBAL(UIDSet).add(0, uidInfo);
    tassertMsg(aval != 0, "uid add (1) failed for %d\n", 0);

#if (TMP_DBG>=2)
    err_printf("added uid 0\n");
#endif
    return 0;
}

/*virtual*/ SysStatus
ResMgr::acceptVP(ProcessID pid, VPNum vp)
{
    SysStatus rc;
    uval fval;
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    UserID luid;

    TraceOSResMgrEnterAcceptVP(pid, vp);

    StubCPUContainer stub(StubBaseObj::UNINITIALIZED);

    fval = COGLOBAL(PIDSet).find(pid, pidInfo);
    if (fval == 0) {
        err_printf("acceptVP: lookup on PID %ld failed\n", pid);
	TraceOSResMgrExitAcceptVP(0, pid, vp);
	return _SERROR(2720, 0, ENOENT);
    }

    luid = pidInfo->uid;
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    if (fval == 0) {
        err_printf("error (1): ResMgr could not find uid for pid 0x%lx\n", pid);
	TraceOSResMgrExitAcceptVP(1, pid, vp);
	return _SERROR(2721, 0, ENOENT);
    }

#if (TMP_DBG>=2)
err_printf("%s (1): pid 0x%lx vp %ld on %ld\n", __func__, pid, vp, Scheduler::GetVP());
#endif
    for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
        if (pidInfo->queryDSPID(rd,vp) == pidInfo->EXIST) {
	    checkCreateCPUContainer(rd, uidInfo);
	    stub.setOH(uidInfo->resourceDomain[rd]->
		       CPUContainerOH[Scheduler::GetVP()]);
	    rc = stub._attachDispatcher(pid, SysTypes::DSPID(rd, vp));
#if (TMP_DBG>=2)
err_printf("%s (2): pid 0x%lx vp %ld rd %ld on %ld\n", __func__, pid, vp, rd, Scheduler::GetVP());
#endif
	    tassertMsg(_SUCCESS(rc) || (_SGENCD(rc) == ESRCH),
		       "attachDispatcher failed rd %ld vp %ld pp %ld.\n",
		       rd, vp, Scheduler::GetVP());
	}
    }

    TraceOSResMgrExitAcceptVP(2, pid, vp);
    return 0;
}

struct ResMgr::AcceptVPMsg : MPMsgMgr::MsgSync {
    ProcessID pid;
    VPNum vp;
    SysStatus rc;

    virtual void handle() {
	rc = DREF(TheResourceManager)-> acceptVP(pid, vp);
	reply();
    }
};


// FIXME this interface really needs to take in an acceptable set of
//       processors.  As it stands we could pass back a pp that the 
//       caller is not happy with
VPNum
ResMgr::findPP(ProcessID pid, uval uid)
{
    VPNum pp, prefPP;

    pp = Scheduler::GetVP();
    prefPP = pp;

    TraceOSResMgrEnterFindpp(pid, uid);
#if 0
    if (uid != 0) {
	/*
	 * FIXME:  We need a real policy here.  For now just use the low-order
	 *         bits of the uid.
	 *         this is still a hack to get sdet to work well
	 */
	prefPP = uid % PPCount;
    }
#endif



#if 0
    // by disp count
    uval minRunDispCount;
    minRunDispCount = COGLOBAL(processorInfo->runDispCount[pp]);

    for (pp = 0; pp < PPCount; pp++) {
	if (COGLOBAL(processorInfo->runDispCount[pp]) < minRunDispCount) {
	    minRunDispCount = COGLOBAL(processorInfo->runDispCount[pp]);
	    prefPP = pp;
	}
    }
#endif
#if 0
    // by runnable weight
    uval minRunWeightAccum;
    uval cl = KernelScheduler::PRIORITY_CLASS_TIME_SHARING;

    minRunWeightAccum = COGLOBAL(processorInfo->runWeightAccum[pp][cl]);

    for (pp = 0; pp < PPCount; pp++) {
	if (COGLOBAL(processorInfo->runWeightAccum[pp][cl])<minRunWeightAccum) {
	    minRunWeightAccum = COGLOBAL(processorInfo->runWeightAccum[pp][cl]);
	    prefPP = pp;
	}
    }
#endif

#if 1
    if (uid != 0) {
	// we've just created another userid find a good place for it

	// by assigned domains
	uval minAssignedDomains;

	minAssignedDomains = COGLOBAL(processorInfo->runDispCount[pp]);

	for (pp = 0; pp < PPCount; pp++) {
	    if (COGLOBAL(processorInfo->assignedDomains[pp]) < minAssignedDomains) {
		minAssignedDomains = COGLOBAL(processorInfo->assignedDomains[pp]);
		prefPP = pp;
	    }
	}

	if (PPCount == 4) {
	    TraceOSResMgrFindpp(prefPP,
			 COGLOBAL(processorInfo->assignedDomains[0]),
			 COGLOBAL(processorInfo->assignedDomains[1]),
			 COGLOBAL(processorInfo->assignedDomains[2]),
			 COGLOBAL(processorInfo->assignedDomains[3]));
	}

    } else {


	// by idle

	uval maxIdle;
	maxIdle = COGLOBAL(processorInfo->idleTimeAccum[pp]);

#if (TMP_DBG>=2)
err_printf("%s: idle time  ", __func__);
#endif
	for (pp = 0; pp < PPCount; pp++) {
#if (TMP_DBG==2)
err_printf("%ld  ",COGLOBAL(processorInfo->idleTimeAccum[pp]));
#endif
	    if (COGLOBAL(processorInfo->idleTimeAccum[pp]) > maxIdle) {
		maxIdle = COGLOBAL(processorInfo->idleTimeAccum[pp]);
		prefPP = pp;
	    }
	}
#if (TMP_DBG>=2)
err_printf("  pref pp %ld\n", prefPP);
#endif
#endif
    }

    TraceOSResMgrExitFindpp(0, pid, uid);
    return prefPP;
}

void
ResMgr::checkAndMigrate()
{
    VPNum pp,pppo,ppmo;
    uval myWeight;

    pp = Scheduler::GetVP();
    pppo = (pp+1)%PPCount;
    if (pp == 0) {
        ppmo = PPCount;
    } else {
        ppmo = pp -1;
    }


    myWeight = COGLOBAL(processorInfo->
	runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_TIME_SHARING]);

    if ((myWeight+(FIXMEVAL*1.5)) <
	COGLOBAL(processorInfo->
	runWeightAccum[pppo][KernelScheduler::PRIORITY_CLASS_TIME_SHARING])) {

    } else if ((myWeight+(FIXMEVAL*1.5)) <
	COGLOBAL(processorInfo->
	runWeightAccum[ppmo][KernelScheduler::PRIORITY_CLASS_TIME_SHARING])) {

    }
}


/*virtual*/ SysStatus
ResMgr::updateLoadEstimates()
{
    VPNum pp, i;
    uval maxIdle;
    SysStatus rc;

    TraceOSResMgrEnterUpdate();
    pp = Scheduler::GetVP();

    COGLOBAL(processorInfo->runDispCount[pp]) =
	COGLOBAL(ksStats[pp]->runnableDispatcherCount);

    COGLOBAL(processorInfo->idleTimeAccum[pp]) =
	COGLOBAL(ksStats[pp]->
	    smooth[KernelScheduler::PRIORITY_CLASS_IDLE].dispatchTimeAccum);

    if (pp==0) {
	maxIdle = 0;
	for (i = 0; i < PPCount; i++) {
	    if (COGLOBAL(processorInfo->idleTimeAccum[i]) > maxIdle) {
		maxIdle = COGLOBAL(processorInfo->idleTimeAccum[i]);
	    }
	}
	COGLOBAL(processorInfo->maxIdle) = maxIdle;
    }

    COGLOBAL(processorInfo->
	runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_KERNEL]) =
        COGLOBAL(ksStats[pp]->
	   smooth[KernelScheduler::PRIORITY_CLASS_KERNEL].runnableWeightAccum);

    COGLOBAL(processorInfo->
	runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_GANG_SCHEDULING]) =
	    COGLOBAL(processorInfo->
	    runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_KERNEL]) +
        COGLOBAL(ksStats[pp]->
	    smooth[KernelScheduler::PRIORITY_CLASS_GANG_SCHEDULING].
		 runnableWeightAccum);

    COGLOBAL(processorInfo->
	runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_SOFT_REAL_TIME]) =
	  COGLOBAL(processorInfo->
	  runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_GANG_SCHEDULING])+
        COGLOBAL(ksStats[pp]->
	    smooth[KernelScheduler::PRIORITY_CLASS_SOFT_REAL_TIME].
		 runnableWeightAccum);

    COGLOBAL(processorInfo->
	runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_TIME_SHARING]) =
	  COGLOBAL(processorInfo->
	  runWeightAccum[pp][KernelScheduler::PRIORITY_CLASS_SOFT_REAL_TIME])+
        COGLOBAL(ksStats[pp]->
	    smooth[KernelScheduler::PRIORITY_CLASS_TIME_SHARING].
		 runnableWeightAccum);

    checkAndMigrate();

    if ((COGLOBAL(ResMgrStatsFlag) == 1) && (pp == 0)) {
	err_printf("ResMgr stats:\n");
	err_printf(" idle time  ");
	for (pp = 0; pp < PPCount; pp++) {
	    err_printf("%ld  ", COGLOBAL(processorInfo->idleTimeAccum[pp]));
	}
	err_printf("\n");
	
	err_printf(" assigned domains  ");
	for (pp = 0; pp < PPCount; pp++) {
	    err_printf("%ld  ", COGLOBAL(processorInfo->assignedDomains[pp]));
	}
	err_printf("\n");
    }
    if ((COGLOBAL(ResMgrStatsFlag) == 2) && (pp == 0)) {
	UIDInfo *uidInfo;
	UserID ind=0;

	rc = (COGLOBAL(UIDSet)).getFirst(ind, uidInfo);
	while (rc) {
	    err_printf("\nuid %ld:\n", uidInfo->uid);
	    err_printf(" pp count %ld  pp prim %ld\n", 
		       uidInfo->PPCount, uidInfo->PPPrim);
	    err_printf(" pp disp count ");
	    for (pp = 0; pp < PPCount; pp++) {
		err_printf("%ld ", uidInfo->PPDispCount[pp]);
	    }
	    err_printf("\n");
	    rc = COGLOBAL(UIDSet).getNext(ind, uidInfo);
	}
	err_printf("\n");
	COGLOBAL(ResMgrStatsFlag) = 0;
    }

    if (((COGLOBAL(ResMgrStatsFlag) == 3) || 
	 (COGLOBAL(ResMgrStatsFlag) == 4)) && 
	(pp == 0)) {
	PIDInfo *pidInfo;
	uval vp, rd, vpl, rdl;
	ProcessID ind=0;

	rc = COGLOBAL(PIDSet).getFirst(ind, pidInfo);
	while (rc) {
	    err_printf("\npid %ld:\n", pidInfo->pid);
	    err_printf(" uid %ld\n", pidInfo->uid);
		       
	    err_printf(" pp count ");
	    for (pp = 0; pp < PPCount; pp++) {
		err_printf("%ld ", pidInfo->PPCount[pp]);
	    }
	    err_printf("\n");

	    err_printf(" disp grid ");
	    if (COGLOBAL(ResMgrStatsFlag) == 3) {
		rdl = 2; 
		vpl = PPCount;
	    } else {
		rdl = Scheduler::RDLimit; 
		vpl = Scheduler::VPLimit;
	    }
	    for (rd=0; rd<rdl; rd++) {
		for (vp=0; vp<vpl; vp++) {
		    err_printf("%ld ", (uval)(pidInfo->dispGrid[vp][rd]));
		}
		err_printf("\n           ");
	    }
	    err_printf("\n");
	    rc = COGLOBAL(PIDSet).getNext(ind, pidInfo);
	}
	err_printf("\n");
	COGLOBAL(ResMgrStatsFlag) = 0;
    }

//FIXME add in sanity checking of the information I know against
//      information that the kernel is tracking - especially important
//      as mistracking this may well only cause performance bugs

    TraceOSResMgrExitUpdate();

    return 0;
}

void
ResMgr::checkAddAssignedDomains(VPNum pp, UIDInfo *uidInfo, uval loc)
{
    if (uidInfo->PPDispCount[pp] == 0) {
	uidInfo->ppLock.acquire();
	if (uidInfo->PPDispCount[pp] == 0) {
	    TraceOSResMgrAssignDOMAdd(pp, uidInfo->uid, loc);
	    if (uidInfo->uid !=0) {
		AtomicAdd(&(COGLOBAL(processorInfo->assignedDomains[pp])), 1);
	    }
	    AtomicAdd(&(uidInfo->PPCount), 1);
	}
	TraceOSResMgrAssignDOMAdd(pp, uidInfo->uid, loc+10);
	AtomicAdd(&(uidInfo->PPDispCount[pp]), 1);
	uidInfo->ppLock.release();
    } else {
	AtomicAdd(&(uidInfo->PPDispCount[pp]), 1);
	TraceOSResMgrAssignDOMAdd(pp, uidInfo->uid, loc+100);
    }
}

void
ResMgr::subAndCheckAssignedDomains(VPNum pp, UIDInfo *uidInfo, uval loc)
{
    FetchAndAddSigned((sval*)&uidInfo->PPDispCount[pp], -1);

    TraceOSResMgrAssignDOMSub(pp, uidInfo->uid, 
		 loc*100+10000+uidInfo->PPDispCount[pp]);

    if (uidInfo->PPDispCount[pp] == 0) {
	uidInfo->ppLock.acquire();
	if (uidInfo->PPDispCount[pp] == 0) {
	    TraceOSResMgrAssignDOMSub(pp, uidInfo->uid, loc);
	    if (uidInfo->uid !=0) {
		FetchAndAddSigned((sval*)(&(COGLOBAL(
	            processorInfo->assignedDomains[pp]))), -1);
	    }
	    FetchAndAddSigned((sval*)(&(uidInfo->PPCount)), -1);
	}
	uidInfo->ppLock.release();
    }
}

/*virtual*/ SysStatus
ResMgr::adjustLoadEstimates(VPNum fromPP, VPNum toPP)
{
    int FIXMECLASS = KernelScheduler::PRIORITY_CLASS_TIME_SHARING;

    if (fromPP != VPNum(-1)) {
        COGLOBAL(processorInfo->runDispCount[fromPP]) -= 1;
	//FIXME we need to know what priority class we're adjusting for
        COGLOBAL(processorInfo->runWeightAccum[fromPP][FIXMECLASS]) -= FIXMEVAL;
    }
    if (toPP != VPNum(-1)) {
        COGLOBAL(processorInfo->runDispCount[toPP]) += 1;
        COGLOBAL(processorInfo->runWeightAccum[toPP][FIXMECLASS]) -= FIXMEVAL;
    }
    return 0;
}

/*
 There is a very tricky issue here with accounting.  If we account inside
 of migrateVP then we run into a race with assignDomain for example.
 assignDomain decides on the processor to place something and calls
 migrateVP.  However, if in the meantime another userid calls assignDomain
 before the first migrateVP finishes, or at least gets to the accounting
 stage, then it is likely to pick the same physical processor that was just
 chosen and we will thereby overload that processor.  Alternatively, we can
 do the accounting outside of migrateVP, if we know all the information,
 but then if migrateVP fails we have the opposite problem, namely we had
 assumed we were going to move the dispatcher somewhere and we could not,
 and if in the meantime another findPP call occurs it will then get the
 wrong information for the opposite reason.  The only fully correct answer
 is to lock around the whole operation, but that is too expensive.  In
 reality, with lots of things going on in the system the perfectly
 *correct* answer is not obtainable.  However, for the current SDET mode of
 operation, making a mistake in the first direction causes a fifty percent
 loss of performance.  Therefore migrateVP has an accounting parameter
 allowing the caller to best determine on which side to error.
*/

SysStatus
ResMgr::migrateVP(ProcessID pid, VPNum vp, VPNum pp, uval accountingDone)
{
    SysStatus rc;
    VPNum fromPP;
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    UserID luid;
    uval fval;

    TraceOSResMgrEnterMigrate(pid, vp, pp);

    tassertMsg(pp < PPCount, "Bad processor number.\n");

    fromPP = Scheduler::GetVP();

#if (TMP_DBG>=1)
err_printf("%s: pid 0x%lx vp %ld from pp %ld to pp %ld\n", __func__, 
	       pid, vp, fromPP, pp);
#endif

    if (pp == fromPP) {
	TraceOSResMgrExitMigrate(0, pid, vp, pp);
	// Nothing to do.
	return 0;
    }

    fval = COGLOBAL(PIDSet).find(pid, pidInfo);
    if (fval == 0) {
        err_printf("migrateVP: lookup on PID %ld failed\n", pid);
	TraceOSResMgrExitMigrate(1, pid, vp, pp);
	return _SERROR(2722, 0, ENOENT);
    }

    tassertMsg(fromPP == pidInfo->VPtoPP[vp],
	       "ResMgr migrateVP: error not on correct physical processor\n");

    luid = pidInfo->uid;
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    if (fval == 0) {
        err_printf("error (2): ResMgr could not find uid for pid %ld\n", pid);
	TraceOSResMgrExitMigrate(2, pid, vp, pp);
	return _SERROR(2723, 0, ENOENT);
    }

    StubCPUContainer stub(StubBaseObj::UNINITIALIZED);

    for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
        if (pidInfo->queryDSPID(rd,vp) == pidInfo->EXIST) {
	    checkCreateCPUContainer(rd, uidInfo);
	    stub.setOH(uidInfo->resourceDomain[rd]->CPUContainerOH[fromPP]);
	    for (;;) {
		rc = stub._detachDispatcher(pid, SysTypes::DSPID(rd, vp));
		if (_SUCCESS(rc) || (_SGENCD(rc) == ESRCH)) break;
		passertMsg(_SGENCD(rc) == EAGAIN,
			   "detachDispatcher() failed.\n");
		passertWrn(0, "Retrying detachDispatcher()\n");
		Scheduler::DelayMicrosecs(5000);
	    }
	}
    }

    MPMsgMgr::MsgSpace msgSpace;
    AcceptVPMsg *const msg =
	new(Scheduler::GetEnabledMsgMgr(), msgSpace) AcceptVPMsg;
    tassertMsg(msg != NULL, "message allocate failed.\n");

    msg->pid = pid;
    msg->vp = vp;

    rc = msg->send(SysTypes::DSPID(0, pp));
    tassertMsg(_SUCCESS(rc), "send failed\n");

    pidInfo->VPtoPP[vp] = pp; // for baseservers vp == pp
    adjustLoadEstimates(fromPP, pp);
    pidInfo->PPCount[fromPP] -= 1; // remove from old
    pidInfo->PPCount[pp] += 1; // add to new

    if (! accountingDone) {
	checkAddAssignedDomains(pp, uidInfo, 1);
	subAndCheckAssignedDomains(fromPP, uidInfo, 1);
    }

    TraceOSResMgrExitMigrate(3, pid, vp, pp);
    return msg->rc;
}

/* static */ UserID
ResMgr::getUID(ProcessID pid)
{
    SysStatus rc;

    ProcessLinux::LinuxInfo linuxInfo;

    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoNativePid(pid, linuxInfo);

    if (_FAILURE(rc)) {
	err_printf("could not find uid for pid %ld, using 0\n", pid);
	return (UserID)0;
    }

#if (TMP_DBG>=2)
    err_printf("found uid %d for pid %ld\n", linuxInfo.creds.uid, pid);
#endif
    return (UserID)(linuxInfo.creds.uid);
}

/*virtual*/ SysStatus
ResMgr::moveDomain(RDNum rd, ProcessID caller, PIDInfo *pidInfo, 
		   UIDInfo *uidInfo, UIDInfo *oldUIDInfo)
{
    VPNum pp;
    StubCPUContainer stub(StubBaseObj::UNINITIALIZED);
    SysStatus rc=0;

    for (VPNum vp = 0; vp < Scheduler::VPLimit; vp++) {
	if (pidInfo->queryDSPID(rd,vp) == pidInfo->EXIST) {

	    if (pidInfo->VPtoPP[vp] != Scheduler::GetVP()) {
		// yarbles what a pain - we may have vps on remote
		// pps we need to send messages to those remote pps
		// and get the resmgr there to do a detach and an
		// attach - and it needs to be synchronous

		passertMsg(0==1, "multiple vp setuid NYI\n");
	    }

	    pp = Scheduler::GetVP();
	    // detach dispatcher from old cpu container
	    checkCreateCPUContainer(rd, oldUIDInfo);
	    stub.setOH(oldUIDInfo->resourceDomain[rd]->CPUContainerOH[pp]);
	    for (;;) {
		rc = stub._detachDispatcher(caller, SysTypes::DSPID(rd, vp));
		if (_SUCCESS(rc) || (_SGENCD(rc) == ESRCH)) break;
		passertMsg(_SGENCD(rc) == EAGAIN,
			   "detachDispatcher() failed.\n");
		passertWrn(0, "Retrying detachDispatcher()\n");
		Scheduler::DelayMicrosecs(5000);
	    }

	    // attach dispatcher to new cpu container
	    checkCreateCPUContainer(rd, uidInfo);
	    stub.setOH(uidInfo->resourceDomain[rd]->CPUContainerOH[pp]);
	    rc = stub._attachDispatcher(caller, SysTypes::DSPID(rd,vp));
	    tassertMsg(_SUCCESS(rc) || (_SGENCD(rc) == ESRCH),
		       "attachDispatcher failed rd %ld vp %ld pp %ld.\n",
		       rd, vp, pp);
	}
    }
    return rc;
}

//FIXME we need to add a priority class parameter
//FIXME currently we trust the caller but we need to move that logic
//      into the process linux server or some such entity
/*virtual*/ SysStatus
ResMgr::_assignDomain(uval uid, ProcessID caller)
{
    uval fval, aval;
    UserID oldUID;
    UIDInfo *uidInfo;
    UIDInfo *oldUIDInfo;
    PIDInfo *pidInfo;
    VPNum preferredPP, fromPP;
    SysStatus rc;

    TraceOSResMgrEnterAssign(uid, caller);

#if (TMP_DBG>=2)
    err_printf("%s: uid %ld, caller 0x%lx\n", __func__, uid, caller);
#endif

    fval = COGLOBAL(PIDSet).find(caller, pidInfo);
    if (fval == 0) {
	err_printf("assignDomain: lookup on PID %ld failed\n", caller);
	TraceOSResMgrExitAssign(0, uid, caller);
	return _SERROR(2731, 0, EINVAL);
    }

    oldUID = pidInfo->uid;
    if (oldUID == uid) {
	tassertWrn(oldUID != uid,"warning: setuid called on own uid %ld\n",uid);
	return 0;
    }
    pidInfo->updateLock.acquire();
    pidInfo->uid = uid;

    fval = COGLOBAL(UIDSet).find(oldUID, oldUIDInfo);
    if (fval == 0) {
        err_printf("error: could not find info for old uid %ld\n", oldUID);
	TraceOSResMgrExitAssign(1, uid, caller);
	pidInfo->updateLock.release();
	return _SERROR(2724, 0, EEXIST);
    }

    COGLOBAL(UIDLock).acquire();
    fval = COGLOBAL(UIDSet).find(uid, uidInfo);

    if (fval != 0) {
	uidInfo->countPID(1);
	COGLOBAL(UIDLock).release();

	for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
	    if (oldUIDInfo->resourceDomain[rd] != NULL) {
#if (TMP_DBG>=2)
		err_printf("%s: pre exist %ld caller 0x%lx\n", __func__, rd, caller);
#endif
		moveDomain(rd, caller, pidInfo, uidInfo, oldUIDInfo);
	    }
	}

	pidInfo->updateLock.release();
	return 0;
    } else {
	// this is a new uid create a domain for it
	// initial use count is 1
	uidInfo = UIDInfo::createInitUIDInfo();
	// for now we assume it's time sharing need parameter

	for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
#if (TMP_DBG>=2)
	    err_printf("%s: rd %ld caller 0x%lx\n", __func__, rd, caller);
#endif

	    if (oldUIDInfo->resourceDomain[rd] != NULL) {
#if (TMP_DBG>=2)
		err_printf("in assign domain inner rd %ld caller %ld\n", rd, caller);
#endif

		uidInfo->resourceDomain[rd] = ResourceDomain::createRD(
		    KernelScheduler::PRIORITY_CLASS_TIME_SHARING,64,10000,10000);

		moveDomain(rd, caller, pidInfo, uidInfo, oldUIDInfo);
	    }
	}
	uidInfo->uid = uid;
	aval = COGLOBAL(UIDSet).add(uid, uidInfo);
	tassertMsg(aval != 0, "uid add (2) failed for %ld\n", uid);
    }

    //if (KernelInfo::ControlFlagIsSet(KernelInfo::UID_PROCESSOR_ASSIGNMENT)) {
    COGLOBAL(processorInfo->findPPLock.acquire());
    preferredPP = findPP(caller, uid);

    // adjust accounting - but see FIXME at end of this method and in moveDomain
    // also see comment at beginning of migrateVP
    subAndCheckAssignedDomains(Scheduler::GetVP(), oldUIDInfo, 2);
    checkAddAssignedDomains(preferredPP, uidInfo, 2);

    COGLOBAL(processorInfo->findPPLock.release());

    uidInfo->PPPrim = preferredPP;

    /* account process out of old uid */
    if(0 == oldUIDInfo->countPID(-1)) removeUID(oldUIDInfo);
    COGLOBAL(UIDLock).release();

    TraceOSResMgrExitAssign(3, uid, caller);
    pidInfo->updateLock.release();

    // FIXME there is a FIXME and passert in move domain as well, but the
    //       below statement needs to be fixed if this is a multi-vp uid 
    fromPP = Scheduler::GetVP();
    rc = (SysStatus)0;
    if (fromPP != preferredPP) {
#if (TMP_DBG>=2)
	err_printf("%s: pid 0x%lx uid %ld pp %ld\n", __func__, caller, uidInfo->uid, preferredPP);
#endif
	rc = migrateVP(caller, (VPNum)0, preferredPP, 1);
    }

    if (_FAILURE(rc)) {
	// back out accounting
	subAndCheckAssignedDomains(preferredPP, oldUIDInfo, 2);
	checkAddAssignedDomains(Scheduler::GetVP(), uidInfo, 2);
    }
    return rc;
}

/*virtual*/ SysStatus
ResMgr::_createFirstDispatcher(ObjectHandle childOH,
			       EntryPointDesc entry, uval dispatcherAddr,
			       uval initMsgLength, char *initMsg,
			       ProcessID caller)
{
    SysStatus rc;
    VPNum pp;
    ProcessID childPID;
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    UserID luid;
    RDNum rd;
    VPNum vp;
    uval fval, aval;

#if (TMP_DBG>=2)
err_printf("%s start for caller 0x%lx\n", __func__, caller);
#endif

    TraceOSResMgrEnterCreate(caller);
    pp = Scheduler::GetVP(); // for baseservers vp == pp

    adjustLoadEstimates(VPNum(-1), pp);

    // since the child isn't fully set up yet we'll use the uid of parent
    // they'll be the same until a setuid call at which point we'll
    // switch domain

    fval = COGLOBAL(PIDSet).find(caller, pidInfo);
    if (fval == 0) {
	err_printf("%s: lookup on PID 0x%lx failed\n", __func__, caller);
	TraceOSResMgrExitCreate(0, caller);
	return _SERROR(2732, 0, EINVAL);
    }

    /*
     * the uid entry must exist because we are using the uid of the
     * caller, who is known to the resource manager already.
     * Unless the caller process dies while this call is in flight,
     * in which case we give up.
     */
    luid = pidInfo->uid;
    COGLOBAL(UIDLock).acquire();
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    if (fval == 0) {
        err_printf("error (3): ResMgr could not find uid for pid 0x%lx\n",caller);
	TraceOSResMgrExitCreate(1, caller);
	COGLOBAL(UIDLock).release();
	return _SERROR(2726, 0, ENOENT);
    }
    uidInfo->countPID(1);
    COGLOBAL(UIDLock).release();

    StubCPUContainer stub(StubBaseObj::UNINITIALIZED);
    checkCreateCPUContainer(0, uidInfo);
    stub.setOH(uidInfo->resourceDomain[0]->CPUContainerOH[pp]);

    rc = DREF(privilegedServiceRef)->pidFromProcOH(childOH, caller, childPID);

    pidInfo = PIDInfo::createInitPIDInfo();
    pidInfo->pid = childPID;

    rd = 0; vp = 0; // first dispatcher is always vp,rd == 0,0
    pidInfo->VPtoPP[vp] = pp; // for baseservers vp == pp
    pidInfo->PPCount[pp] = 1; // first one on this pp
    pidInfo->enterDSPID(rd, vp);
    pidInfo->uid = luid;
    aval = COGLOBAL(PIDSet).add(childPID, pidInfo);
    TraceOSResMgrPIDAdd(childPID, aval);
    tassertMsg(aval != 0, "pid add (1) failed for %ld\n", childPID);
    checkAddAssignedDomains(pp, uidInfo, 3);
    rc = giveAccessByServer(pidInfo->wrapperOH, childPID);
    if (_FAILURE(rc)) {
	// something bad happened here someone blew away the process
	// as it was in the process of being created no one is going
	// to tell us about it going away so we need to delete pidinfo
	subAndCheckAssignedDomains(pp, uidInfo, 3);
	COGLOBAL(PIDSet).remove(childPID, pidInfo);
	TraceOSResMgrPIDRemove(childPID, 0xFFFF);
	COGLOBAL(UIDLock).acquire();
	uidInfo->countPID(-1);
	COGLOBAL(UIDLock).release();
	delete pidInfo;
	return rc;
    }
    
#if (TMP_DBG>=2)
err_printf("%s: end for child 0x%lx caller 0x%lx\n", __func__,
	       childPID, caller);
#endif

    rc = stub._createFirstDispatcher(childOH, caller, entry, dispatcherAddr,
				     initMsgLength, initMsg);

    tassertWrn(_SUCCESS(rc), "create first disptacher failed\n");
    // we do not need a remove from the PID set here because
    // the above give access by server was successful and thus the
    // remove will be done in handleXObjFree

    TraceOSResMgrExitCreate(2, caller);
    return rc;
}

/*virtual*/ SysStatus
ResMgr::registerFirstDispatcher(ObjectHandle& oh, ProcessID caller)
{
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    VPNum pp;
    uval aval;

    TraceOSResMgrEnterRegister(caller);
    pp = Scheduler::GetVP(); // for baseservers vp == pp

    pidInfo = PIDInfo::createInitPIDInfo();
    pidInfo->pid = caller;
    pidInfo->VPtoPP[0] = pp;  // for baseservers vp == pp
    pidInfo->PPCount[pp] = 1; // first one on this pp
    pidInfo->enterDSPID(0, 0);
    pidInfo->uid = getUID(caller);
    aval = COGLOBAL(PIDSet).add(caller, pidInfo);
    TraceOSResMgrPIDAdd(caller, aval);
    tassertMsg(aval != 0, "pid add (2) failed for %ld\n", caller);

    UserID luid;
    uval fval;
    luid = pidInfo->uid;
    COGLOBAL(UIDLock).acquire();
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    if (fval == 0) {
        err_printf("error (3): ResMgr could not find uid for pid %ld\n",caller);
	TraceOSResMgrExitCreate(1, caller);
	COGLOBAL(UIDLock).release();
	return _SERROR(2726, 0, ENOENT);
    }
    uidInfo->countPID(1);
    COGLOBAL(UIDLock).release();
    
    giveAccessByServer(pidInfo->wrapperOH, caller);
    oh.initWithOH(pidInfo->wrapperOH);
    
#if (TMP_DBG>=2)
    err_printf("register first dispatcher for caller %ld\n",caller);
#endif

    TraceOSResMgrExitRegister(caller);
    return 0;
}

/*virtual*/ SysStatus
ResMgr::createDispatcherLocal(DispatcherID dspid, EntryPointDesc entry,
			      UIDInfo *uidInfo, uval dispatcherAddr,
			      uval initMsgLength, char *initMsg,
			      ProcessID caller)
{
    SysStatus rc;
    RDNum rd;
    VPNum vp, pp;

    pp = Scheduler::GetVP(); // for baseservers vp == pp

    adjustLoadEstimates(VPNum(-1), pp);

    StubCPUContainer stub(StubBaseObj::UNINITIALIZED);
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    checkCreateCPUContainer(rd, uidInfo);
    stub.setOH(uidInfo->resourceDomain[rd]->CPUContainerOH[Scheduler::GetVP()]);
    rc = stub._createDispatcher(caller, dspid, entry, dispatcherAddr,
				initMsgLength, initMsg);
    if (_SUCCESS(rc)) {
	checkAddAssignedDomains(pp, uidInfo, 4);
    }
    return rc;
}

struct ResMgr::CreateDispatcherMsg : MPMsgMgr::MsgSync {
    DispatcherID dspid;
    EntryPointDesc entry;
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    uval dispatcherAddr;
    uval initMsgLength;
    char *initMsg;
    ProcessID caller;
    SysStatus rc;

    virtual void handle() {
	rc = DREF(TheResourceManager)->
		    createDispatcherLocal(dspid, entry, uidInfo, dispatcherAddr,
					  initMsgLength, initMsg, caller);
	reply();
    }
};

/*virtual*/ SysStatus
ResMgr::_createDispatcher(DispatcherID dspid,
			  EntryPointDesc entry, uval dispatcherAddr,
			  uval initMsgLength, char *initMsg,
			  ProcessID caller)
{
    SysStatus rc;
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    UserID luid;
    VPNum pp;
    uval fval;
    RDNum rd;
    VPNum vp;

    TraceOSResMgrEnterCreateDisp(caller);

    if (caller == _SGETPID(DREFGOBJ(TheProcessRef)->getPID())) {
	return DREF(privilegedServiceRef)->
			createServerDispatcher(dspid, entry, dispatcherAddr,
					       initMsgLength, initMsg);
    }

    SysTypes::UNPACK_DSPID(dspid, rd, vp);

#if (TMP_DBG>=2)
    err_printf("%s: start for caller 0x%lx rd %ld, vp %ld\n", __func__, caller, rd, vp);
#endif


    if (rd >= Scheduler::RDLimit) {
        TraceOSResMgrExitCreateDisp(0, caller);
	return _SERROR(2658, 0, EINVAL);
    }

    fval = COGLOBAL(PIDSet).find(caller, pidInfo);
    if (fval == 0) {
	err_printf("createDispatcher: lookup on PID %ld failed\n", caller);
        TraceOSResMgrExitCreateDisp(1, caller);
	return _SERROR(2730, 0, EINVAL);
    }

    pidInfo->updateLock.acquire();

    if (pidInfo->queryDSPID(dspid) == pidInfo->EXIST) {
	pidInfo->updateLock.release();
	return _SERROR(2771, 0, EEXIST);
    }


#if (TMP_DBG>=2)
    err_printf("%s: rd %ld mypp %ld caller 0x%lx vp %ld\n", __func__,
	     rd, Scheduler::GetVP(), caller, vp);
#endif

    if (rd == 0) {
	// we are creating another VP choose a good processor
	tassertMsg(pidInfo->VPtoPP[vp] == VPNum(-1),
		   "error: attempted creation of rd 0 on existing vp\n");
	pp = findPP(caller, 0);

#if (TMP_DBG>=2)
	err_printf("%s: findpp pid 0x%lx vp %ld rd %ld pp %ld\n",i
			 __func__, caller, vp, rd, pp);
#endif

	if (pidInfo->PPCount[pp] > 0) {
	    /*
	     * This PID already has a VP on the selected processor.  See if
	     * there's a processor on which this PID has fewer VPs.
	     */
	    VPNum startPP = pp;
	    for (;;) {
		pp = (pp + 1) % PPCount;
		if (pp == startPP) break;
		if (pidInfo->PPCount[pp] < pidInfo->PPCount[startPP]) break;
	    };
	}
	pidInfo->VPtoPP[vp] = pp;
	AtomicAdd(&(pidInfo->PPCount[pp]), 1);  // just added a vp to this pp
    } else {
	// we're adding another disptacher to an existing vp
	// verify vp exists
	if (pidInfo->VPtoPP[vp] == VPNum(-1)) {
	    tassertMsg(pidInfo->VPtoPP[vp] != VPNum(-1), "what's up\n");
	    TraceOSResMgrExitCreateDisp(2, caller);
	    pidInfo->updateLock.release();
	    return _SERROR(2718, 0, ENOENT);
	}
	pp = pidInfo->VPtoPP[vp];
    }

    pidInfo->enterDSPID(rd, vp);


#if (TMP_DBG>=2)
    err_printf("%s pp %ld rd %ld mypp %ld caller 0x%lx vp %ld\n", __func__,
	     pp, rd, Scheduler::GetVP(), caller, vp);
#endif

//pp = (Scheduler::GetVP() + ((rd == 0) ? vp : 0)) % PPCount;

// err_printf("in ___create disp pp %ld rd %ld mypp %ld caller %ld vp %ld\n",
//	     pp, rd, Scheduler::GetVP(), caller, vp);

    luid = pidInfo->uid;
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    if (fval == 0) {
        err_printf("error (4): ResMgr could not find uid for pid %ld\n",caller);
	return _SERROR(2727, 0, ENOENT);
    }

    if (pp == Scheduler::GetVP()) {
	rc = createDispatcherLocal(dspid, entry, uidInfo, dispatcherAddr,
				   initMsgLength, initMsg, caller);
        TraceOSResMgrExitCreateDisp(3, caller);
	pidInfo->updateLock.release();
	return rc;
    } else {
#if (TMP_DBG>=2)
	err_printf("%s: msg to pp %ld rd %ld mypp %ld caller 0x%lx vp %ld\n",
		 __func__, pp, rd, Scheduler::GetVP(), caller, vp);
#endif
	MPMsgMgr::MsgSpace msgSpace;
	CreateDispatcherMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr(), msgSpace) CreateDispatcherMsg;
	tassertMsg(msg != NULL, "message allocate failed.\n");

	msg->dspid = dspid;
	msg->entry = entry;
	msg->uidInfo = uidInfo;
	msg->dispatcherAddr = dispatcherAddr;
	msg->initMsgLength = initMsgLength;
	msg->initMsg = initMsg;
	msg->caller = caller;

	rc = msg->send(SysTypes::DSPID(0, pp));
	tassertMsg(_SUCCESS(rc), "send failed\n");

        TraceOSResMgrExitCreateDisp(4, caller);
	pidInfo->updateLock.release();
	return msg->rc;
    }
}

/* virtual */ SysStatus 
ResMgr::_setStatsFlag(uval val, ProcessID caller)
{
    COGLOBAL(ResMgrStatsFlag) = val;
    return 0;
}

/* virtual */ SysStatus
ResMgr::_toggleStatsFlag(ProcessID caller)
{
    COGLOBAL(ResMgrStatsFlag) = 1 - COGLOBAL(ResMgrStatsFlag);
    return 0;
}

/*virtual*/ SysStatus
ResMgr::_execNotify(ProcessID caller)
{
    VPNum physProc;
    UIDInfo *uidInfo;
    PIDInfo *pidInfo;
    UserID luid;
    uval fval, pp;
    SysStatus rc;


#if (TMP_DBG>=2)
    err_printf("%s: start caller 0x%lx\n", __func__, caller);
#endif
    TraceOSResMgrEnterExec(caller);

#if 0
    if (KernelInfo::ControlFlagIsSet(KernelInfo::UID_PROCESSOR_ASSIGNMENT)) {
	// FIXME still a hack to make sure we don't goof up sdet
	TraceOSResMgrExitExec(0, caller);
	return 0;
    }
#endif

    fval = COGLOBAL(PIDSet).find(caller, pidInfo);
    if (fval == 0) {
	err_printf("execNotify: lookup on PID %ld failed\n", caller);
	TraceOSResMgrExitExec(1, caller);
	return _SERROR(2736, 0, EINVAL);
    }

    luid = pidInfo->uid;
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    if (fval == 0) {
        err_printf("error (5): ResMgr could not find uid for pid %ld\n",caller);
	TraceOSResMgrExitExec(2, caller);
	return _SERROR(2737, 0, ENOENT);
    }

    pp = Scheduler::GetVP();

    TraceOSResMgrExitExec(3, caller);
return 0;

    // FIXME lots more work needed here, for now make exec resonably sticky
    if (COGLOBAL(processorInfo->idleTimeAccum[pp]) <
        (COGLOBAL(processorInfo->maxIdle) +
	 (COGLOBAL(ksStats[pp]->smoothingInterval) / 2))) {

        physProc = findPP(caller, 0);

#if (TMP_DBG>=2)
	err_printf("%s migrating pid 0x%lx from pp %ld to to pp %ld\n\n",
		 __func__, caller, Scheduler::GetVP(), physProc);
#endif

	rc = migrateVP(caller, VPNum(0), physProc, 0);
	TraceOSResMgrExitExec(4, caller);
	return rc;
    } else {
	// not migrating
	TraceOSResMgrExitExec(5, caller);
	return 0;
    }

}

// FIXME:  this interface to the res mgr is for testing and should go away.
/*virtual*/ SysStatus
ResMgr::_migrateVP(VPNum vpNum, VPNum suggestedPP, ProcessID caller)
{
    if (suggestedPP >= PPCount) {
	return _SERROR(2664, 0, EINVAL);
    }
    return migrateVP(caller, vpNum, suggestedPP, 0);
}

/*virtual*/ SysStatus
ResMgr::_accessKernelSchedulerStats(ObjectHandle &statsFROH,
				    uval &statsRegionSize,
				    uval &statsSize,
				    __CALLER_PID caller)
{
    // FIXME:  Must authenticate the caller.  Frequent access to the stats
    //         structure will cause cache misses in the kernel on other
    //         processors.
    statsRegionSize = COGLOBAL(dispatchQueueStatsRegionSize);
    statsSize = COGLOBAL(dispatchQueueStatsSize);
    return Obj::GiveAccessByClient(COGLOBAL(dispatchQueueStatsFROH),
				   statsFROH, caller);
}

/* static */ void
ResMgr::BeingFreed(XHandle xhandle) {
    DREF(TheResourceManager)->beingFreed(xhandle);
}

SysStatus
ResMgr::beingFreed(XHandle xhandle)
{
    PIDInfo *pidInfo;
    UIDInfo *uidInfo;
    uval fval;
    UserID luid;

    TraceOSResMgrEnterFreed();

    pidInfo = (PIDInfo*)(XHandleTrans::GetClientData(xhandle));
    luid = pidInfo->uid;
#if (TMP_DBG>=2)
    err_printf("delete pid 0x%lx\n", pidInfo->pid);
#endif
    delete pidInfo;
    /*
     * any uses of uidInfo associated with this pid use count are
     * now complete because we waited for beingFreed call.  If there
     * are other uses in flight, then the use count will not go to
     * zero here.
     */
    COGLOBAL(UIDLock).acquire();
    fval = COGLOBAL(UIDSet).find(luid, uidInfo);
    tassertMsg(fval, "where did the uid data for %ld go?\n", luid);
    if(fval) {
	if(0 == uidInfo->countPID(-1)) {
	    removeUID(uidInfo);
	}
    }
    COGLOBAL(UIDLock).release();
    TraceOSResMgrExitFreed();
    return 0;
}

/* virtual */ SysStatus
ResMgr::handleXObjFree(XHandle xhandle)
{
    PIDInfo *pidInfo;
    uval fval;
    ProcessID pid;

    TraceOSResMgrEnterXobjFree();

    pid = XHandleTrans::GetOwnerProcessID(xhandle);

    fval = COGLOBAL(PIDSet).find(pid, pidInfo);
    TraceOSResMgrPIDRemove(pid, fval);

    tassertMsg(fval != 0, "XobjFre: lookup on PID %ld failed\n", pid);

    /*
     * because we only allow one XHandle per process, we know that when
     * it is freed, the process is gone.  See rights settings on the
     * object handle we make - it does not allow giveAccess or releaseAccess
     */
    COGLOBAL(PIDSet).remove(pid, pidInfo);
    /*
     * because we don't have global locks, it is possible that another
     * thread is already executing a request and is referring to
     * procInfo and or uidInfo.  So here, we have removed it from the
     * hash table (no new threads can find it) but defer freeing the
     * storage until BeingFreed - at which time all other threads now runing
     * are known to have finished.
     */
    XHandleTrans::SetClientData(xhandle, (uval)pidInfo);
    XHandleTrans::SetBeingFreed(xhandle, BeingFreed);

    TraceOSResMgrExitXobjFree();
    return 0;
}

void
ResMgr::removeUID(UIDInfo *uidInfo)
{
    _ASSERT_HELD(COGLOBAL(UIDLock));
    VPNum pp;
    COGLOBAL(UIDSet).remove(uidInfo->uid, uidInfo);

    /* we took it to 0 */
#if (TMP_DBG>=2)
    cprintf("\nresource mgr freeing uid data for uid %ld\n\n",uidInfo->uid);
#endif

    for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
	if (uidInfo->resourceDomain[rd] != NULL) {
	    delete uidInfo->resourceDomain[rd];
	}
    }

    for (pp=0; pp < FIXME_MAXPROCS; pp++) {
	if ((uidInfo->PPDispCount[pp] > 0) && (uidInfo->uid !=0)) {
	    TraceOSResMgrAssignDOMSub(pp, uidInfo->uid , // 5);
			 1000+uidInfo->PPDispCount[pp]);
	    FetchAndAddSigned((sval*)&COGLOBAL(
		processorInfo->assignedDomains[pp]),-1);
	}
    }
#if (TMP_DBG>=2)
    err_printf("delete uid %ld\n", uidInfo->uid);
#endif
    delete uidInfo;
}

/* static */ SysStatus
ResMgr::_Create(ObjectHandle &trmOH, __CALLER_PID caller)
{
    return DREF(TheResourceManager)->getPidOH(trmOH, caller);
}

SysStatus
ResMgr::getPidOH(ObjectHandle &oh, ProcessID caller)
{
    uval fval;
    // isn't standard create because we have only one instance of
    //  the resource manager just give out access to it.
    PIDInfo *pidInfo;
    fval = COGLOBAL(PIDSet).find(caller, pidInfo);
    if(fval && pidInfo->wrapperOH.valid()) {
	oh.initWithOH(pidInfo->wrapperOH);
	return 0;
    } else {
	return _SERROR(2769, 0, ENOENT);
    }
}

/* static */ SysStatus
ResMgr::_CreateAndRegisterFirstDispatcher(
    ObjectHandle &trmOH, __CALLER_PID caller)
{
    return DREF(TheResourceManager)->registerFirstDispatcher(trmOH, caller);
}
