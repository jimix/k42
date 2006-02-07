/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootMediator.C,v 1.21 2005/08/09 12:03:06 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of the mediator root for dyn-switching
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <misc/ListSimpleKey.H>
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include "CObjRootMultiRep.H"
#include "CObjRep.H"
#include "CObjRepMediator.H"
#include "CObjRootMediator.H"
#include "MediatedThreadTable.H"
#include "sys/COSMgrObject.H"

#define GENWAIT_DEBUG 0

struct WorkerThreadMsg : public MPMsgMgr::MsgAsync {
    struct Args {
	CObjRootMediator *root;
	CObjRepMediator::Data *medData;
	uval fullWorker;

	DEFINE_GLOBAL_NEW(Args);
	Args(CObjRootMediator *r, CObjRepMediator::Data *md, uval f) :
	    root(r), medData(md), fullWorker(f) {
	    /* empty body */
	}
    };

    Args *args;

    virtual void handle() {
	Args *a = args;
	free();
	CObjRootMediator::workerThreadStub((uval)a);
    }
};

inline void
CObjRootMediator::init()
{
    switchPhase = NORMAL;
    blockPhaseExitTicket = 1;
    numMediatorsWithInFlightCalls = 0;
    numWorkerThreads = 0;
    //blockPhaseFiniCompletion = new ConditionObject;
    // this should be the very first use of this lock!
    tassert(!phaseLock.isLocked(), err_printf("lock init: someone has it!\n"));
    rootPhaseLock();
}

CObjRootMediator::CObjRootMediator(CObjRoot *const ort, CObjRoot *const nrt,
                                   DTType dtt, COSMgr::SwitchDispDir dir,
                                   Callback *cb, sval c):
    CObjRootMultiRep(ort->getRef(), c, CObjRoot::skipInstall),
    oldRoot(ort), newRoot(nrt), dataTransferType(dtt), dtBarrier(1),
    dispDir(dir), callback(cb)
{
    init();
}

// Spawns a worker thread on the VP if necessary. It also returns the medData
// associated with the vp if a thread is spawned.
CObjRepMediator::Data *
CObjRootMediator::spawnWorkerIfNeeded()
{
    VPNum vp = Scheduler::GetVP();
    CObjRepMediator::Data *medData = 0;

    if (!workerThreadsVPSet.isSet(vp)) {
	// Handling a miss on a new VP that doesn't have a VP worker thread.
	// We will create one for it right now.
	workerThreadsVPSet.addVP(vp);
	if (!medDataFind(vp, medData)) {
	    // a new mediator with no existing worker thread will be created.
	    // So I can either initialize the mediator phase as the
	    // root phase, or always initialize it as BLOCK.
	    // Will set it to BLOCK always... the argument is that we want to
	    // perform the data transfer asap. Leaving it at FORWARD is only
	    // going to slow this down.
	    medData = new CObjRepMediator::Data(SWITCH_MEDIATE_BLOCK, dispDir);
	    medDataAdd(vp, medData);
	    err_tprintf("initializing medData as BLOCK\n");
	}
	switch (medData->switchPhase) {
	case SWITCH_MEDIATE_FORWARD:
	    // NOTE: if we indeed choose to start the mediator at FORWARD phase,
	    // this needs to be changed. See comments in workerThreadEntry().
	    spawnVPWorkerThread(vp, medData, 0);
	    break;
	case SWITCH_MEDIATE_BLOCK:
	    spawnVPWorkerThread(vp, medData, 0);
	    break;
	default:
	    // NOTREACHED
	    tassert(0, ;);
	}
    }

    return medData;
}

/*virtual*/ SysStatus
CObjRootMediator::handleMiss(COSTransObject * &co, CORef ref,
			       uval methodNum)
{
    rootPhaseLock();
    switch (switchPhase) {
    case NORMAL:
	rootPhaseUnlock();
	// NOTREACHED in new model
	tassert(0,;);
	err_tprintf("--=> handleMiss() (NORMAL)\n");
	oldRoot->handleMiss(co, ref, methodNum);
	break;
    case SWITCH_MEDIATE_FORWARD:
    case SWITCH_MEDIATE_BLOCK:
	err_tprintf("--=> handleMiss() (SWITCHING)\n");
	//installMediator(co, ref, methodNum);
	CObjRootMultiRep::handleMiss(co, ref, methodNum);
	// release here to protect this thread from reinstalling the mediator
	// after the final LTE flush, when all the inflight calls are finished.
	rootPhaseUnlock();
	break;
    case SWITCH_COMPLETED:
	rootPhaseUnlock();
	err_tprintf("--=> handleMiss() (SWITCH_COMPLETED)\n");
	// new root should be now be installed in the GTE
	newRoot->handleMiss(co, ref, methodNum);
	break;
    default:
	tassert(0, err_printf("Unknown switchPhase\n"));
	rootPhaseUnlock();
	break;
    }

    return 0;
}

/*virtual*/ CObjRep *
CObjRootMediator::createRep(VPNum vp)
{
    CObjRepMediator::Data *medData;

    medData = spawnWorkerIfNeeded();

    // Mediator not yet created for this vp, will create one now.
    // We need to pass the medData in constructing the mediator.
    // If the medData is available already due to spawning of a new
    // worker thread at spawnWorkerIfNeeded(), then we don't need to
    // look it up any more.
    if (!medData) {
	medDataFind(vp, medData);
    }
    tassert(medData, err_printf("mediator data should be available!\n"));
    CObjRep *const rep = oldRoot->getRepOnThisVP();
    CObjRepMediator *const med = new CObjRepMediator(rep, medData);

    mediatingVPSet.addVP(vp);

    err_tprintf("*** CORootSw::createRep(): mediator = %p\n", med);

    return (CObjRep *)med;
}

void
CObjRootMediator::waitForBlockPhaseFiniCompletion()
{
    ThreadID threadTmp;

    err_tprintf("waitForBlockPhaseFiniCompletion()...\n");
    if (blockPhaseFiniCompletion.registerForCondition(threadTmp) == 0) {
	blockPhaseFiniCompletion.waitForCondition(threadTmp);
    }
}

#if 0
void
CObjRootMediator::waitForRootSwitchCompletion()
{
    ThreadID threadTmp;

    rootPhaseLock();
    if (switchPhase == SWITCH_COMPLETED) {
	rootPhaseUnlock();
	// switch already completed... no need to wait
	err_tprintf("ROOTwait: BYPASSING BLOCK\n");
	return;
    }
    // else wait for condition
    err_tprintf("waitForRootSwitchCompletion()...\n");
    rootSwitchCompletion->registerForCondition(threadTmp);
    rootPhaseUnlock();
    rootSwitchCompletion->waitForCondition(threadTmp);
}
#endif /* #if 0 */

void
CObjRootMediator::initWorkerThreads(VPSet vpset)
{
    // Here, we are assuming that we have a dense clustering of the set of VPs
    // for each mediator. More precisely, a cluster of size C will have:
    // cluster0 = {0..C-1}, ...  clusterk = {kC..k(C+1)-1}, ...
    const uval numVP = DREFGOBJ(TheProcessRef)->vpCount();
    const uval medCSize = clustersize;
    CObjRepMediator::Data *data;
    uval currClusterVPCount;

    for (uval vpStart = 0; vpStart < numVP; vpStart += medCSize) {
	currClusterVPCount = 0;

	// count up the number of vps we need to spawn worker threads
	for (uval vp = vpStart; vp < vpStart+medCSize && vp < numVP; vp++) {
	    if (!vpset.isSet(vp)) continue;
	    currClusterVPCount++;
	}

	// We spawn the thread in the second pass so that the mediator data
	// are initialized properly before the worker threads are spawned.

	if (currClusterVPCount) {
	    tassert(!medDataFind(vpStart, data), err_printf("oops\n"));
	    data = new CObjRepMediator::Data(SWITCH_MEDIATE_FORWARD, dispDir);
	    medDataAdd(vpStart, data);
	    fetchAndAddVPsInForwardPhase(currClusterVPCount);
	    fetchAndAddPerMediatorVPsInForwardPhase(data, currClusterVPCount);

	    // Spawn the threads here now that the data is set up
	    for (uval vp = vpStart; vp < vpStart+medCSize && vp < numVP; vp++) {
		if (!vpset.isSet(vp)) continue;
		SysStatus rc = spawnVPWorkerThread(vp, data, 1);
		tassert(rc==0, err_printf("spawnVPWorkerThread() failed\n"));
	    }
	}
    }
}

SysStatus
CObjRootMediator::spawnVPWorkerThread(VPNum vp,
					CObjRepMediator::Data *medData,
					uval fullWorker)
{
    SysStatus rc;
    WorkerThreadMsg::Args *const pargs =
	new WorkerThreadMsg::Args(this, medData, fullWorker);

    if (fullWorker) {
	numWorkerThreads++;
    } else {
	FetchAndAddSignedSynced(&numWorkerThreads, 1);
    }
    if (vp == Scheduler::GetVP()) {
	//err_printf("Using ScheduleFunction.\n");
	rc = Scheduler::ScheduleFunction(
		Scheduler::ThreadFunction(workerThreadStub),
		uval(pargs));
    } else {
	//err_printf("Using msg->send.\n");
	WorkerThreadMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr()) WorkerThreadMsg;
	tassert(msg, err_printf("message alloc failed.\n"));
	msg->args = pargs;
	rc = msg->send(SysTypes::DSPID(0, vp));
    }
    if (!_SUCCESS(rc)) {
	tassert(0, err_printf("thread operation failed.\n"));
	delete pargs;
	FetchAndAddSignedSynced(&numWorkerThreads, -1);
    }

    return rc;
}

/*static*/ SysStatus
CObjRootMediator::workerThreadStub(uval p)
{
    const WorkerThreadMsg::Args args = *(WorkerThreadMsg::Args *)p;
    delete ((WorkerThreadMsg::Args *)p);

    SysStatus rc = args.root->workerThreadEntry(args.fullWorker, args.medData);
    tassert(_SUCCESS(rc), ;);
    return 0;
}

inline SysStatus
CObjRootMediator::flushProcessorLocalTransEntry()
{
    DREFGOBJ(TheCOSMgrRef)->resetLocalTransEntry((CORef)getRef());

    return 0;
}

#define err_tprintf	if (0) err_printf
// Worker thread -- responsible for flushing LTE, polling generation count,
// and most phase changes.
SysStatus
CObjRootMediator::workerThreadEntry(uval full,
				      CObjRepMediator::Data *medData)
{
    CObjRepMediator *med = 0;
    uval genCheckCount = 0;
    err_tprintf("Deactiveting self (removed from active count)\n");
    Scheduler::DeactivateSelf();
    uval dtTicket = 0;

#ifndef NDEBUG
    if (full) {
	err_tprintf("FULLWORKER HERE\n");
    } else {
	err_tprintf("PARTWORKER HERE\n");
    }
#endif /* #ifndef NDEBUG */

    // Some explanations for the fullWorker flag used here:
    //
    // For worker threads that are spawned from the beginning since those
    // corresponding VPs have reps installed, we start them with fullWorker
    // being true.
    //
    // For worker threads that are spawned due to a miss since there were not
    // any rep on those vps prior to the switch, we start them with fullWorker
    // being false. Their responsiblities only include waiting for the final
    // transfer to complete and flushing the LTE in the end. They do not need
    // to go through with the mediator/root FORWARD->BLOCK phase switch.
    // Reason: If the worker's associated mediator has another worker
    // thread started since that vp has a rep originally, it will perform the
    // phase switching tasks. Otherwise, the associated mediator has
    // no rep originally, and so our algorithm sets the mediator's phase to be
    // BLOCK initially, and we're okay also. Here we note that, in the latter
    // case, if we choose not to start the mediator at BLOCK phase, we will
    // need the first worker thread to perform the FORWARD->BLOCK phase switch
    // protocol also.
    if (full) {
	// this worker thread is started from the initial spawn.
	sval count;

	// First flush the local LTE so that the mediator will be used for new
	// incoming CO calls
	flushProcessorLocalTransEntry();
	err_tprintf("Done initial flush\n");

	// request a generation change
	// poll til the prev generation count reaches 0 (discounting cur thrd)
	COSMgr::ThreadMarker marker;
	COSMgr::MarkerState  mstate;
	err_tprintf("Creating thread marker...\n");
	DREFGOBJ(TheCOSMgrRef)->setVPThreadMarker(marker);

	Scheduler::Yield();
	DREFGOBJ(TheCOSMgrRef)->updateAndCheckVPThreadMarker(marker, mstate);
	if (mstate != COSMgr::ELAPSED) {
	    DREFGOBJ(TheCOSMgrRef)->updateAndCheckVPThreadMarker(marker,
								 mstate);
	}
	genCheckCount++;
	while (mstate != COSMgr::ELAPSED)
	{
	    if ((genCheckCount & 0x3) != 0) {
		Scheduler::Yield();
		Scheduler::YieldProcessor();
		//err_printf("[0x%lx genwait]\n", Scheduler::GetCurThread());
	    } else {
#if GENWAIT_DEBUG
		err_printf("[0x%lx GENWAIT]\n", Scheduler::GetCurThread());
		// FIXME: figure out why this debug aid hangs
		//Scheduler::PrintStatus();
#endif /* #if GENWAIT_DEBUG */
		if (KernelInfo::OnSim()) {
		    // FIXME: maybe a small number here is okay also?
		    Scheduler::DelayMicrosecs(5000);
		} else {
		    Scheduler::DelayMicrosecs(10);
		}
	    }

	    DREFGOBJ(TheCOSMgrRef)->updateAndCheckVPThreadMarker(marker,
								 mstate);
	    if (mstate != COSMgr::ELAPSED) {
		DREFGOBJ(TheCOSMgrRef)->
		    updateAndCheckVPThreadMarker(marker, mstate);
	    }
	    genCheckCount++;
	}

	//err_printf("gen ELAPSED... genCheckCount = %ld\n", genCheckCount);

	// We cannot change to the BLOCK phase while there still are in-flight
	// calls that are not accounted for. (Blocking recursive calls will
	// result in deadlock.) So we will let the last thread passing through
	// here perform the phase switch.

	count = fetchAndAddPerMediatorVPsInForwardPhase(medData, -1);
	if (count == 1) {
	    // don't execute this unless the generation counts for the subset
	    // of vp that a mediator is responsible for are all 0's

	    err_tprintf("Generation count is now zero\n");

#if 0
	    // FIXME
	    Scheduler::DelayMicrosecs(20000);
#endif /* #if 0 */

	    // acquired lock, no mediator will be created while I hold the lock
	    lockMediators();
	    // should not grab rootPhaseLock while got lockMediators
	    med = (CObjRepMediator *)locked_findRepOn(Scheduler::GetVP());
	    if (med) {
		err_tprintf("med->genCountNotification()\n");
		med->genCountNotification();
	    } else {
		// no mediator available for this set of worker threads,
		// directly modify the data in root.
		// Can do this with the mediatorList locked, since the phase
		// change need not be protected when the medData is not
		// associated with a mediator. The last condition is ensure
		// by the facts that 1) we are in the else clause 2) we have
		// the mediatorList lock
		err_tprintf("medData->switchPhase = SWITCH_MEDIATE_BLOCK\n");
		medData->switchPhase = SWITCH_MEDIATE_BLOCK;
	    }
	    unlockMediators();

#if 0
	    // FIXME
	    Scheduler::DelayMicrosecs(10000);
#endif /* #if 0 */
	}

	// Dec count and enter only if you are the last thread executing. Why?
	// Because we need a way to know if all the local mediators are at the
	// BLOCK phase before we can do the mediated call counter check, and
	// this way is the simplest.

	count = fetchAndAddVPsInForwardPhase(-1);
	if (count == 1) {
	    // Multi med code will need to change the root phase when all
	    // other threads have gotten out of the gencount polling.

	    rootPhaseLock();
	    if (switchPhase == SWITCH_MEDIATE_FORWARD) {
		// only 1 is doing the change
		err_tprintf("Switching root Phase to BLOCK\n");
		switchPhase = SWITCH_MEDIATE_BLOCK;
	    }
	    rootPhaseUnlock();

	    // The slowest worker thread gets here.
	    // Note that this check should happen after the locked root phase
	    // change, otherwise new FORWARD calls might still enter and make
	    // the count go up again.
	    if (callCounterIsZero()) {
		// root switch, phase change, unblock threads
		dtTicket = blockPhaseFini();
	    }
	}
    }

    waitForBlockPhaseFiniCompletion();

    // parallel data transfer
    doDataTransfer();

    sval dtCount = fetchAndAddVPsInDT(-1);

#define HOTSWAP_OLDOBJECT_CLEANUP 1
#define CLEANUP_USING_STD_DESTROY 1

#ifdef HOTSWAP_OLDOBJECT_CLEANUP
#ifndef CLEANUP_USING_STD_DESTROY
    COSMgrObject::ManualCleanupOperator cleanupOp;
#endif
#endif

    if (dtCount == 1) {
	// only the last guy should exec this
	err_tprintf("Last DT thread done... switching root.\n");
	installNewRootAndCompleteSwitchUnlocked();

	if (callback != NULL) {
	    callback->complete(0);
	}

        // If directed to cleanup the old object we start that process
#ifdef HOTSWAP_OLDOBJECT_CLEANUP
        if (medData->disposition == COSMgr::DESTROY_WHEN_DONE) {
#ifdef CLEANUP_USING_STD_DESTROY
            CORef ref;
            oldRoot->reAssignRef(ref);
            DREFGOBJ(TheCOSMgrRef)->destroyCO(ref, oldRoot);            
#else            
            cleanupOp.start(oldRoot);
#endif
        }
#endif
        if (medData->disposition == COSMgr::REASSIGN_WHEN_DONE) {
            CORef ref;
            oldRoot->reAssignRef(ref);
        }
    }

    // Here, we have the final wait to synchronize the threads so that we
    // don't proceed to flush the LTEs before the final switch has
    // completed.
    dtBarrier.enter();

#ifdef HOTSWAP_OLDOBJECT_CLEANUP
#ifndef CLEANUP_USING_STD_DESTROY
    if (medData->disposition == COSMgr::DESTROY_WHEN_DONE) {
        cleanupOp.perVPInvocation();
    }
#endif
#endif

    if (dtTicket) {
	// data transfer and root switch has completed
	// release root phase lock
	rootPhaseUnlock();
    }

    tassert(switchPhase == SWITCH_COMPLETED, ;);

    // Flush the LTE again (if mediator is installed) so that the new miss
    // handler will be used.
    //
    // Optimizations:
    // 1. shouldn't need to flush if there are no new faults after
    // the initial flush.
    // 2. should avoid duplicate final flush
    // To solve both 1+2: use a bit mask to mark all the VPs with a mediator
    // installed. Set the bit when you install the mediator. Clear the bit if
    // we are sure that the mediator is not installed in the LTE (eg. after you
    // have done a handleMiss to install the new rep.  And flush here only if
    // the bit for this VP is set.
    // FIXME: use ProcessSet object of some kind instead of an uval mask
    const VPNum currVP = Scheduler::GetVP();
    if (mediatingVPSet.isSet(currVP)) {
	err_tprintf("Flushing LTE\n");
	flushProcessorLocalTransEntry();
	mediatingVPSet.removeVP(currVP);

	// 1 worker thread per mediator needs to unblock the client threads.
	// And before doing so, it must first change the mediator's phase
	// to avoid further blocking.
	if (med == 0) {
	    lockMediators();
	    med = (CObjRepMediator *)locked_findRepOn(Scheduler::GetVP());
	    unlockMediators();
	}
	tassert(med, err_printf("Couldn't find mediator!\n"));
	// only 1 thread per mediator really executes the unblock
	med->unblockThreads();
    } else {
	err_tprintf("Skipping LTE flushing as it doesn't cache a mediator\n");
	//tassert((_clustersize != 1 || !findRepOn(Scheduler::GetVP())),
	//	err_printf("Shouldn't have a mediator for this vp!\n"));
    }

    // FIXME: mediator cleanup (do it after all LTE flushes are done and after
    // all the threads have died off).
    // Hook up with Jonathan's CODestroy interface (but without having to
    // reclaim the COID also)

    Scheduler::ActivateSelf();
#if GENWAIT_DEBUG
    if (genCheckCount > 3) {
	err_printf("[0x%lx] switch took %ld (>3) genChecks\n",
		   Scheduler::GetCurThread(), genCheckCount);
    }
#endif /* #if GENWAIT_DEBUG */
    err_tprintf("---- Worker Thread Completes ----\n");
    return 0;
}

// This call doesn't block (except for the lock).
// So, the LTEs are not guaranteed to be flushed upon return.
SysStatus
CObjRootMediator::switchImpl()
{
    tassert(phaseLock.isLocked(), ;);
    //rootPhaseLock();

    // take control over the original root -- now I'm the miss handler.
    // don't do this unless you have the lock in advance.
    DREFGOBJ(TheCOSMgrRef)->swingRoot((CORef)getRef(), this);
    // unlock GTE which was locked by COSMgr::switchCObj
    DREFGOBJ(TheCOSMgrRef)->gteWriteComplete((CORef)getRef());

    tassert(switchPhase == NORMAL, err_printf("huh\n"));

    err_tprintf("*** CObj switch initiated.\n");

    totalVPsInCurrPhase = 0;
    workerThreadsVPSet = oldRoot->getTransSet();
    mediatingVPSet.init();

    if (workerThreadsVPSet.isEmpty()) {
	// No reps has been instantiated yet before the switch was initiated.
	// And since we now hold the phaseLock, we can just complete the
	// switch.

	err_tprintf("CObj never touched... completing switch NOW.\n");

	// Not needed if lock is held since we are getting to COMPLETED
	// keep here for completeness
	switchPhase = SWITCH_MEDIATE_BLOCK;

	// We are holding the phase lock already
	uval ticket = blockPhaseFini(0);
	tassert(ticket, ;);
	doDataTransfer();
	installNewRootAndCompleteSwitchUnlocked();

	tassert(switchPhase == SWITCH_COMPLETED, ;);
	rootPhaseUnlock();
	if (callback != NULL) {
	    callback->complete(0);
	}
	return 0;
    }

    // Note that at this moment calls can still come in through the current co
    // ref and invoke the original rep, until I have flushed the translation
    // entries on _all_ processors

    // This goes before flushing the entries so that when the miss handler is
    // invoked after this, we get the switching miss handling behaviour
    switchPhase = SWITCH_MEDIATE_FORWARD;

    // Set up the counters before entering the switch mode, so that the worker
    // threads will not over-decrement the counters due to race.
    //
    // The worker threads are responsible for flushing the processor specific
    // local translation entries for this co ref
    initWorkerThreads(workerThreadsVPSet);

    tassert(phaseLock.isLocked(), ;);
    rootPhaseUnlock();

    return 0;
}

void
CObjRootMediator::installNewRootAndCompleteSwitchUnlocked()
{
    // Once the new miss handler is installed the old one should
    // not be used any more. We use the phase lock around the
    // code path to guarantee that.

    DREFGOBJ(TheCOSMgrRef)->swingRoot((CORef)getRef(), newRoot);
    DREFGOBJ(TheCOSMgrRef)->gteSwitchComplete((CORef)getRef());

    switchPhase = SWITCH_COMPLETED;
    //rootSwitchCompletion->conditionSet();
}

uval
CObjRootMediator::incNumMediatorsWithInFlightCalls()
{
    uval count;
    uval success = 0;

    while (!success) {
	count = numMediatorsWithInFlightCalls;
	if (count == 0 && getPhase() >= SWITCH_MEDIATE_BLOCK) break;
	const uval tmpCount = count + 1;
	success = CompareAndStoreSynced(&numMediatorsWithInFlightCalls,
					count, tmpCount);
    }
    err_tprintf("incNumMediatorsWithInFlightCalls: #%ld (%s)\n",
	    count + 1, success ? "SUCCEEDED": "FAILED");

    return success;
}

uval
CObjRootMediator::decNumMediatorsWithInFlightCalls()
{
    uval count;
    uval success = 0;

    while (!success) {
	count = numMediatorsWithInFlightCalls;
	tassert(count > 0, ;);
	const uval tmpCount = count - 1;
	success = CompareAndStoreSynced(&numMediatorsWithInFlightCalls,
					count, tmpCount);
    }
    err_tprintf("decNumMediatorsWithInFlightCalls: #%ld\n", count - 1);

    return (count - 1 == 0);
}

uval
CObjRootMediator::callCounterIsZero()
{
    return numMediatorsWithInFlightCalls == 0;
}

uval
CObjRootMediator::blockPhaseFini(uval doLock)
{
    err_tprintf("Root::blockPhaseFini(%s)\n", doLock?"":"Unlocked");
    const uval ticket = FetchAndClearSynced(&blockPhaseExitTicket);
    if (ticket) {
	if (doLock) {
	    rootPhaseLock();
	}

	//rootSwitch();
	tassert(callCounterIsZero(), err_printf("call count not zero!!\n"));
	err_tprintf("blockPhaseFini(): numWorkerThreads = %ld\n",
		    numWorkerThreads);
	totalVPsInCurrPhase = numWorkerThreads;
	dtBarrier.reinit(numWorkerThreads);
	blockPhaseFiniCompletion.conditionSet();

	//if (doLock) {
	//    rootPhaseUnlock();
	//}
    }

    return ticket;
}

CObjRep *
CObjRootMediator::getNewRep(uval methNum)
{
    // Note that at this point the LTE could still contain the
    // mediator (with phase == SWITCH_COMPLETED)
    // assertion: it's associated mediator is at phase COMPLETED
    CObjRep *nrep = 0;

    tassert(switchPhase == SWITCH_COMPLETED, ;);
    newRoot->handleMiss((COSTransObject * &)nrep,
			(CORef)newRoot->getRef(), methNum);

    const VPNum currVP = Scheduler::GetVP();
    if (mediatingVPSet.isSet(currVP)) {
	// clearing the bit as the mediator is not in LTE any more
	mediatingVPSet.removeVP(currVP);
    }

    return nrep;
}

void
CObjRootMediator::doDataTransfer()
{
    // This is executed in parallel, one per VP

    DataTransferObject *dtobj = 0;

    err_tprintf("Doing dataXfer...\n");
    dtobj = oldRoot->dataTransferExport(dataTransferType, workerThreadsVPSet);
    newRoot->dataTransferImport(dtobj, dataTransferType, workerThreadsVPSet);
}

#if 0
void
CObjRootMediator::rootSwitch()
{
    err_tprintf("SwRoot::rootSwitch: I'm the ticket holder\n");
    tassert(callCounterIsZero(), err_printf("call count not zero!!\n"));
    // Ask Root to do the following in this order:
    //   1. get new CO
    //   2. perform data transfer
    //   3. change root's phase variable to SWITCH_COMPLETED (locked)
    //   4. tell all mediators to: [unblockThreads()]
    //    a. change the local switchPhase to SWITCH_COMPLETED (locked)
    //    b. unblock the pending incoming calls
    doDataTransfer(oldRoot, newRoot);
    installNewRootAndCompleteSwitchUnlocked();

    tassert(getPhase() == SWITCH_COMPLETED,
	    err_printf("switch should be completed!\n"));
}
#endif /* #if 0 */
