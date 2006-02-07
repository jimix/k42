/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefaultMultiRepRoot.C,v 1.29 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/

#include "kernIncs.H"
#include <trace/traceMem.h>
#include "defines/paging.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FR.H"
#include "mem/FCMDefaultMultiRep.H"
#include "mem/PageFaultNotification.H"
#include "mem/PM.H"
#include <cobj/DTType.H>
#include "mem/FCMDataXferObj.H"
#include "mem/PageCopy.H"
#include <sys/KernelInfo.H>

/*virtual*/
SysStatus
FCMDefaultMultiRepRoot::getDataTransferExportSet(DTTypeSet *set)
{
    //dttset.addType(DTT_FCM_CANONICAL);
    //dttset.addType(DTT_FCM_DEFAULTMRROOT);
    return 0;
}

/*virtual*/
SysStatus
FCMDefaultMultiRepRoot::getDataTransferImportSet(DTTypeSet *set)
{

    set->addType(DTT_FCM_CANONICAL);
    set->addType(DTT_FCM_DEFAULT);
    return 0;
}

/*virtual*/
DataTransferObject *
FCMDefaultMultiRepRoot::dataTransferExport(DTType dtt, VPSet dtVPSet)
{
    DataTransferObject *data = 0;

    switch (dtt) {
    default:
	tassert(0, err_printf("oops, unhandled export type\n"));
	break;
    }
    return (DataTransferObject *)data;
}

//gcc bug - N.B. can't even inline the body gcc 2.95.3
DHashTableBase::AllocateStatus
FCMDefaultMultiRepRoot::gccBug(uval fileOffset,
				      MasterPageDescData ** md)
{
    return masterDHashTable.findOrAllocateAndLock(fileOffset, md, 1);
}

SysStatus
FCMDefaultMultiRepRoot::doTransferFromDefault(FCMDefault *fcmdef)
{
    tassert(!fcmdef->lock.isLocked(), err_printf("FCMDefault being used??\n"));
    frRef = fcmdef->frRef;
    numanode = fcmdef->numanode;
    pageable = fcmdef->pageable;
    backedBySwap = fcmdef->backedBySwap;
    priv = fcmdef->priv;
    pmRef = fcmdef->pmRef;
    beingDestroyed = fcmdef->beingDestroyed;
    mappedExecutable = fcmdef->mappedExecutable;
    noSharedSegments = fcmdef->noSharedSegments;

    nextOffset = fcmdef->nextOffset;

    referenceCount = fcmdef->referenceCount;

    // Copy regionList over.
    // Maybe we can steal those existing RegionInfo entries from the old
    // list... this way we don't need to allocate them and copy them again.
#ifndef NDEBUG
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("copying regionList...\n");
    }
#endif
    RegionRef rlk;
    FCMDefault::RegionInfo *ri;
    RegionInfo *myri=0;
    for (void *curr = fcmdef->regionList.next(0, rlk, ri);
	 curr != 0;
	 curr = fcmdef->regionList.next(curr, rlk, ri)) {
	myri = new RegionInfo(ri->pm);
	myri->ppset = ri->ppset;
	regionList.add(rlk, myri);
    }


    //  Copy pageList over.
#ifndef NDEBUG
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("copying pageList[");
    }
#endif
    for (PageDesc *pd = fcmdef->pageList.getFirst(); pd != 0;
	 pd = fcmdef->pageList.getNext(pd)) {
	MasterPageDescData *md = 0;
	DHashTableBase::AllocateStatus astat =
	    masterDHashTable.findOrAllocateAndLock(pd->fileOffset, &md, 1);
	tassert(astat == DHashTableBase::ALLOCATED && md != 0, ;);
	// copyFrom copies the fn pointer... double check this
	md->copyFrom((PageDescData *)pd);
#ifndef NDEBUG
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf(".");
	}
#endif
	md->clearEmpty();	// FIXME: double check this
	md->unlock();
    }
#ifndef NDEBUG
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("]\n");
    }
#endif

    return 0;
}

/*virtual*/
SysStatus
FCMDefaultMultiRepRoot::dataTransferImport(DataTransferObject *dtobj,
					   DTType dtt, VPSet dtVPSet)
{
    tassert(dtobj, err_printf("dtobj is NULL.\n"));

    switch (dtt) {
    case DTT_FCM_CANONICAL:
	passertMsg(0, "DTT_FCM_CANONICAL: NYI\n");
	break;
    case DTT_FCM_DEFAULT:
	{
	    DataTransferObjectFCMDefault *def =
		(DataTransferObjectFCMDefault *)dtobj;
	    if (dtVPSet.firstVP() == Scheduler::GetVP()) {
		FCMDefault *fcmdef = def->fcm();
		tassert(fcmdef, ;);
		doTransferFromDefault(fcmdef);
	    }
	}
	break;
    default:
	tassert(0, err_printf("FCMCommonMultiRepRoot::dataTransferImport: "
			      "unsupported type!\n"));
	break;
    }
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("DT completed using typeid %ld.\n", (uval)dtt);
    }
    delete dtobj;
    return 0;
}

/* virtual */ CObjRep *
FCMDefaultMultiRepRoot::createRep(VPNum vp)
{
    FCMDefaultMultiRep *rep=new FCMDefaultMultiRep();
    FCMDefaultMultiRep::LHashTable *lt = rep->getLocalDHashTable();
    masterDHashTable.addLTable(vp,clustersize,lt);
    lt->setMasterDHashTable(&masterDHashTable);
    return rep;
}

/* static */ SysStatus
FCMDefaultMultiRepRoot::Create(FCMRef &ref, FRRef cr, uval pageable)
{
    FCMDefaultMultiRepRoot *fcmroot;

    fcmroot = new FCMDefaultMultiRepRoot();
    if (fcmroot == NULL) return -1;

    return fcmroot->init(ref, cr, pageable, 0);
}

/* static */ CObjRoot *
FCMDefaultMultiRepRoot::CreateRootForSwitch(uval numPages)
{
    FCMDefaultMultiRepRoot *fcmroot = new FCMDefaultMultiRepRoot(numPages);

    return fcmroot;
}

SysStatus
FCMDefaultMultiRepRoot::doFillAllAndLockLocal(uval fileOffset,
					     PageFaultNotification *skipFn,
					     LocalPageDescData *ld)
{
    tassert(ld!=0, ;);
    if (ld->isFreeAfterIO()) {  // Free after IO must be globally consistent.
	                        // master and local values all match
	uval vaddr;
	// we are meant to get rid of this page
	//tassert(!pg->mapped, err_printf("Freeing but mapped\n"));
	// indicate that we are given up ownership of pages
	// PageAllocatorKernPinned::clearFrameDesc(ld->getPAddr());

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(ld->getPAddr());
	DREF(pmRef)->deallocPages((FCMRef)getRef(), vaddr, ld->getLen());
	tassert(ld->isDoingIO(), ;);
	ld->unlock(); // doingIO ensures that this is safe as it is has not
                      // yet been cleared.  FIXME: verify
	LocalPageDescData::EmptyArg emptyArg = { 1,     //doNotify
			                         skipFn,
						 0 };   //rc
	masterDHashTable.doEmpty(ld->getKey(),
				 (DHashTableBase::OpArg)
				 &emptyArg);
	return -1;

    } else {
	LocalPageDescData::IOCompleteArg ioCompleteArg =
	{ 1,      //dirty=1
	  skipFn, //fn=skipFn
	  0  };   //rc=0
	masterDHashTable.doOp(fileOffset, &MasterPageDescData::doIOComplete,
			      &LocalPageDescData::doIOComplete,
			      (DHashTableBase::OpArg)&ioCompleteArg, ld);
    }
    return 0;
}

SysStatus
FCMDefaultMultiRepRoot::doIOCompleteAll(uval fileOffset,
				  SysStatus rc)
{
    MasterPageDescData *md=masterDHashTable.findAndLock(fileOffset);
    tassert(md, err_printf("bad request from FR %lx\n", fileOffset));

    tassert(!(md->isForkIO()),
	    err_printf("why are we here with forkIO set?\n"));

    if (md->isFreeAfterIO()) {
	tassert(rc!=-1, ;);
	uval vaddr;
	// we are meant to get rid of this page
	// FIXME: Must assert this in a distributed fashion
//	tassert(!pg->mapped, err_printf("Freeing but mapped\n"));
	// indicate that we are given up ownership of pages

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(md->getPAddr());
	DREF(pmRef)->deallocPages((FCMRef)getRef(), vaddr, md->getLen());
	tassert(md->isDoingIO(), ;);
	LocalPageDescData::EmptyArg emptyArg = { 1, // doNotify
						 0, // skipFn=0
						 rc };
	masterDHashTable.doEmpty(md,
				 (DHashTableBase::OpArg)
				 &emptyArg);
	// no need to unlock anything after empty even though we locked
	// md to begin with.  All these protocols with DHashTable need to
	// be clarified and improved
        checkEmpty();
	return 2;
    } else {
        LocalPageDescData::IOCompleteArg ioCompleteArg =
            { 0,    // clear dirty bit
              0,    // fn
              rc }; // rc

	masterDHashTable.lockAllReplicas(md);
	if (md->doIOComplete((DHashTableBase::OpArg)&ioCompleteArg) !=
	    DHashTableBase::FAILURE) {
	    masterDHashTable.doOp(md, &LocalPageDescData::doIOComplete,
				  (DHashTableBase::OpArg)&ioCompleteArg,
				  DHashTableBase::LOCKNONE);
	} else {
	    passert(0, ;);
	}
	masterDHashTable.unlockAllReplicas(md);
	md->unlock();
    }
    return 0;
}

#if 0
/*
 * replaced by common doIOCompleteAll derived from doFillAll
 * note that locking strategy here was different
 * was it better?  was it correct?
 */
SysStatus
FCMDefaultMultiRepRoot::doPutAll(uval fileOffset, SysStatus rc)
{
    MasterPageDescData *md=masterDHashTable.findAndLock(fileOffset);
    tassert(md!=0, ;);
    if (md->isFreeAfterIO()) {
	uval vaddr;
	// we are meant to get rid of this page
	//tassert(!pg->mapped, err_printf("Freeing but mapped\n"));
	// indicate that we are given up ownership of pages
	// PageAllocatorKernPinned::clearFrameDesc(md->getPAddr());

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(md->getPAddr());
	DREF(pmRef)->deallocPages((FCMRef)getRef(), vaddr, md->getLen());

	LocalPageDescData::EmptyArg emptyArg = { 1,      //doNotify=1
						 0,      //fn=skipFn
						 rc  };  //rc=0
	masterDHashTable.doEmpty(md,
				 (DHashTableBase::OpArg)
				 &emptyArg);
	// no need to unlock anything after empty even though we locked
	// md to begin with.  All these protocols with DHashTable need to
	// be clarified and improved
	return -1;
    } else {
	LocalPageDescData::PutArg putArg = {  1,       // clearDirty=1
					      0,       // fn=0
					      rc  };   // rc=rc
	md->doPut((DHashTableBase::OpArg)&putArg);
	masterDHashTable.doOp(md,
			      &LocalPageDescData::doPut,
			      (DHashTableBase::OpArg)&putArg);
	md->unlock();
    }
    return 0;
}
#endif

// FIXME: ******** NOT SURE LOCKING IS SAFE
//        Even if it is not sure things should not be redefined to get ride
//        of global lock.
SysStatus
FCMDefaultMultiRepRoot::doReleasePage(uval fileOffset)
{
    lock.acquire();
    MasterPageDescData *md=masterDHashTable.findAndLock(fileOffset);
    passertMsg(md, "attempt to release (unpin) a frame that is not pinned\n");

    md->setPinCount((md->getPinCount()) - 1);
    if (md->getPinCount() == 0) pinnedCount--;
    md->unlock();
    return locked_removeReference();
}

// FIXME: ******** NOT SURE LOCKING IS SAFE
//        Even if it is not sure things should not be redefined to get ride
//        of global lock.
SysStatus
FCMDefaultMultiRepRoot::doGetPage(uval fileOffset, uval *paddr)
{
    lock.acquire();
    MasterPageDescData *md=masterDHashTable.findAndLock(fileOffset);
    if (md == 0) {
        lock.release();
	return -1;				// retry
    }
    *paddr = md->getPAddr();
    // lock page
    if (md->getPinCount() == 0) pinnedCount++;
    md->setPinCount((md->getPinCount()) + 1);
    referenceCount++;
    md->unlock();
    lock.release();
    return 0;
}

void
FCMDefaultMultiRepRoot::doNotifyAllAndRemove(LocalPageDescData* pd)
{
    LocalPageDescData::EmptyArg emptyArg={ 1,   // doNotify
					   0,   // skipFn
					   0 }; // rc
    masterDHashTable.doEmpty(pd->getKey(),
			     (DHashTableBase::OpArg)
			     &emptyArg);
}

void
FCMDefaultMultiRepRoot::doSetPAddr(uval fileOffset,
				   uval paddr,
				   LocalPageDescData *ld) {

    // DoingIO servers as an existence lock for ld and hence we meet
    // the criteria for keeping the local lock
    masterDHashTable.doOp(fileOffset, &MasterPageDescData::doSetPAddr,
			  &LocalPageDescData::doSetPAddr,
			  (DHashTableBase::OpArg)paddr, ld);
}

// Don't destroy FCM when last use disappears.  Only destroy
// if FR (file representative) detaches (file is really gone)
// or page replacement removes last page (lazy close).
// FIXME: no locking going on here???
SysStatus
FCMDefaultMultiRepRoot::detachRegion(RegionRef regRef)
{
    //err_printf("FCMDefaultMultiRepRoot::detachRegion(%lx) : %lx\n", regRef, getRef());
    RegionInfo *rinfo;
    PMRef       pm;
    uval        found;

    // check before locking to avoid callback on destruction deadlock
    if (beingDestroyed) return 0;

    lock.acquire();

    found = regionList.remove(regRef, rinfo);
    if (!found) {
	// assume race on destruction call
	tassert(beingDestroyed,err_printf("no region; no destruction race\n"));
	lock.release();
	return 0;
    }
    pm = rinfo->pm;
    delete rinfo;
    if (!referenceCount && regionList.isEmpty()) {
	//err_printf("FCMDefaultMultiRepRoot::detachRegion(%lx) regionlist empty: %lx\n",
	//   regRef, getRef());
	lock.release();
	notInUse();
    } else {
	lock.release();
    }
    //err_printf("FCMDefaultMultiRepRoot::detachRegion maybe doing updatePM\n");
    // above code may have destroyed us; updatePM info if we still have a pm
    if (!beingDestroyed) updatePM(pm);

    //err_printf("FCMDefaultMultiRepRoot::detachRegion all done\n");

    return 0;
}



SysStatus
FCMDefaultMultiRepRoot::init(FCMRef &ref, FRRef cr, uval ISpageable, 
			     uval ISbackedBySwap)
{
    frRef = cr;
    numanode = PageAllocator::LOCAL_NUMANODE; // default, allocate locally
    pageable = ISpageable ? 1 : 0;	// convert to 1/0 for bit field
    backedBySwap = ISbackedBySwap ? 1 : 0; // convert to 1/0 for bit field
    // shared if pageable but not backed by swap (hence file), private opp.
    priv = !(ISpageable && !ISbackedBySwap);

    //err_printf("FCMDefaultMultiRepRoot: %p, priv %lu, pageble %lu, backedbySwap %lu\n",
    //ref, priv, pageable, backedBySwap);

    ref = (FCMRef) getRef();

    tassert(ref != NULL, err_printf("oops got a null ref\n"));

    if (pageable) {
	pmRef = GOBJK(ThePMRootRef);// eventually file cache pm
	DREF(pmRef)->attachFCM(ref);
    }

    return 0;
}

/*
 * Go through all modified pages and unmap and write each one
 * Initial implementation syncronous but that's not good enough
 */
SysStatus
FCMDefaultMultiRepRoot::fsync(uval force)
{
    MasterPageDescData* md;
    SysStatus rc;

//    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
//	err_printf("f");
//    }

    if (backedBySwap) return 0;		// nothing to do

//    if (!COGLOBAL(isDirty()) return 0;  // Some sort of global hint should
                                          // be used

//    if (!KernelInfo::ControlFlagIsSet(KernelInfo::USE_MULTI_REP_FCMS)) {
//        breakpoint();
//    }
    masterDHashTable.addReference();

    for (void *curr = masterDHashTable.getNextAndLock(0, &md); curr != 0;
	curr = masterDHashTable.getNextAndLock(curr, &md)) {
	if (md->isDirty() && !md->isDoingIO()) {
            md->doSetDoingIO(0);
            masterDHashTable.doOp(md, &LocalPageDescData::doSetDoingIO,
                                  0, DHashTableBase::LOCKALL);
            // Must be called after setting doingIO globally to ensure
            // That no one can map the page untill we are done unmapping it.
            unmapPage(md);
            // We have to drop lock so that we can call the fr
            // FIXME: I can't remember why it is save to use curr after the
            //        lock has been dropped need to check the DHashTable
            //        code to validate why (if it really is)
            md->unlock();
//          if (!KernelInfo::ControlFlagIsSet(KernelInfo::USE_MULTI_REP_FCMS)) {
//                err_printf("d");
//            }
            rc = DREF(frRef)->startPutPage(md->getPAddr(),
                                           md->getFileOffset());

            // what should we do on errors; we need to at least remove
            // doingIO flag and wake up anyone waiting
            tassert( _SUCCESS(rc), err_printf("putpage failed\n"));

	} else {
            md->unlock();
        }
    }

    masterDHashTable.removeReference();

#if 0 // local dirty bit made fsync perf unacceptable (SUCKED BIG TIME)
    for (void *curr = masterDHashTable.getNextAndLockAll(0, &md); curr != 0;
	curr = masterDHashTable.getNextAndLockAll(curr, &md)) {
	//FIXME:  All of this is questionable.  Please verify and add
	//        necessary assertions.
	if (!md->isDoingIO()) { // master's doingIO flag reflects global
	                        // state of the page
	    dirty = md->isDirty();  // master's dirty flag is only a hint
	    if (!dirty) {    // can't tell by master must check all replicas
		masterDHashTable.doOp(md,
				      &LocalPageDescData::doCheckDirty,
				      (DHashTableBase::OpArg)&dirty,
				      DHashTableBase::LOCKNONE);
	    }
	    if (dirty) {
		unmapPage(md);
		md->doSetDoingIO(0);
		masterDHashTable.doOp(md, &LocalPageDescData::doSetDoingIO,
				      0, DHashTableBase::LOCKNONE);
		rc = DREF(frRef)->startPutPage(md->getPAddr(),
					       md->getFileOffset());
		// what should we do on errors; we need to at least remove
		// doingIO flag and wake up anyone waiting
		tassert( _SUCCESS(rc), err_printf("putpage failed\n"));
	    }
	}
	masterDHashTable.unlockAllReplicas(md);
	md->unlock();
    }
#endif
    return 0;
}





//FIXME get rid of this once testing is complete
// see FCMComputation::forkCopy - this is for testing
// its used to force all pages to disk so we can test
// disk related fork copy paths
extern uval marcScan;

void
FCMDefaultMultiRepRoot::locked_pageScan()
{
    // get implementation from FCMDefault.C
    tassert(0, err_printf("pageScan not implemented yet\n"));
}


