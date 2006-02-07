#ifndef __COBJ_ROOT_MEDIATOR_H_
#define __COBJ_ROOT_MEDIATOR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootMediator.H,v 1.10 2005/08/09 12:03:06 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definition of the mediator root for dyn-switching
 * **************************************************************************/

#include <cobj/CObjRootMultiRep.H>
#include <cobj/CObjRep.H>
#include <sync/Barrier.H>
#include <cobj/DataTransfer.H>
#include "cobj/CObjRepMediator.H"
#include "cobj/MediatedThreadTable.H"

class CObjRootMediator : public CObjRootMultiRep {
    void init();
protected:
    // FIXME: may be cache-unfriendly... but this is not frequently accessed
    ListSimpleKey<uval, CObjRepMediator::Data *, AllocGlobal> mediatorDataList;
    BLock phaseLock;
    COSwitchPhase switchPhase;
    uval blockPhaseExitTicket; // FIXME just need a bit really
    CObjRoot *oldRoot;
    CObjRoot *newRoot;
    //ConditionObject *rootSwitchCompletion;
    ConditionObject blockPhaseFiniCompletion;
    uval numMediatorsWithInFlightCalls;
    sval totalVPsInCurrPhase;
    VPSet workerThreadsVPSet;
    VPSet mediatingVPSet; // LTE[vp] has a mediator installed => bit set
    sval numWorkerThreads;
    //COSMgr::DataTransferFunction dataTransfer;
    DTType dataTransferType;
    BlockBarrier dtBarrier;
    COSMgr::SwitchDispDir dispDir;
    Callback *callback;

    // since THIS::handleMiss grabs the phaseLock before calling
    // RootMultirep::handleMiss, which grabs medList while handling miss,
    // we should not grab rootPhaseLock while got lockMediators or we may
    // deadlock
    void rootPhaseLock() { phaseLock.acquire(); }
    void rootPhaseUnlock() { phaseLock.release(); }
    void initWorkerThreads(VPSet vpset);
    SysStatus flushProcessorLocalTransEntry();
    SysStatus workerThreadEntry(uval full, CObjRepMediator::Data *medData);
    CObjRepMediator::Data *spawnWorkerIfNeeded();
    SysStatus spawnVPWorkerThread(VPNum vp, CObjRepMediator::Data *medData,
				  uval fullWorker);
    void waitForBlockPhaseFiniCompletion();
    void lockMediators() {
	err_tprintf("lockMediators()...\n");
	lockReps();
	err_tprintf("lockMediators()...DONE\n");
    }
    void unlockMediators() {
	err_tprintf("unlockMediators()...DONE\n");
	unlockReps();
    }
    void medDataAdd(VPNum vp, CObjRepMediator::Data *data) {
	const uval mask = CObjRepMediator::fullMask(vp, clustersize);
	mediatorDataList.add(mask, data);
    }
    uval medDataFind(VPNum vp, CObjRepMediator::Data * &data) {
	const uval mask = CObjRepMediator::fullMask(vp, clustersize);
	return mediatorDataList.find(mask, data);
    }
    sval fetchAndAddPerMediatorVPsInForwardPhase(CObjRepMediator::Data *data,
						 sval delta) {
	return FetchAndAddSignedSynced(&data->workerThreadCount, delta);
    }
    sval fetchAndAddVPsInForwardPhase(sval delta) {
	return FetchAndAddSignedSynced(&totalVPsInCurrPhase, delta);
    }
    sval fetchAndAddVPsInDT(sval delta) {
	return FetchAndAddSignedSynced(&totalVPsInCurrPhase, delta);
    }
    //void rootSwitch();
    void doDataTransfer();
    void installNewRootAndCompleteSwitchUnlocked();

public:
    DEFINE_GLOBAL_NEW(CObjRootMediator);

    CObjRootMediator(CObjRoot *const ort, CObjRoot *const nrt, DTType dtt,
                     COSMgr::SwitchDispDir dir, Callback *cb, sval c=1);
    ~CObjRootMediator() { tassert(0, ;); } // NYI: cleanup mediatorDataList

    SysStatus switchImpl();
    virtual SysStatus handleMiss(COSTransObject * &co,
				 CORef ref, uval methodNum);
    virtual CObjRep *createRep(VPNum vp);

    COSwitchPhase getPhase() { return switchPhase; }

    uval incNumMediatorsWithInFlightCalls();
    uval decNumMediatorsWithInFlightCalls();
    uval callCounterIsZero();
    uval blockPhaseFini(uval doLock = 1);
    CObjRep *getNewRep(uval methNum);


    static SysStatus workerThreadStub(uval p);
};

#endif /* #ifndef __COBJ_ROOT_MEDIATOR_H_ */
