/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPartitioned.C,v 1.2 2005/08/24 15:00:43 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/
#include <kernIncs.H>
#include "mem/FCMPartitioned.H"
#include "mem/Region.H"
#include "mem/SegmentHATPrivate.H"
#include "mem/CacheSync.H"

void
FCMPartitioned::FCMPartitionedRoot::init(uval ps, PageListDisposition pld, 
                                         FRRef fr, uval initSize, 
                                         uval skipCount)
{
    this->pm	  = uninitPM();
    this->oldpm   = NULL;
    this->fr      = fr;
    this->reg     = NULL;
    this->numanode= PageAllocator::LOCAL_NUMANODE; // default, allocate locally
    // hardcoded not to use shared segments;
    this->noSharedSegments = 1;
    //FIXME when locked_cacheSyncAll is implememted make this zero
    this->mappedExecutable = 1;
    this->beingDestroyed = 0;
    this->partitionSize = ps;
    this->sizeRep       = initSize;
    this->skipCount     = skipCount;
    this->destroyPageListDir = pld;
    for (uval i=0;i<Scheduler::VPLimit;i++) {
        this->repArray[i].rep=0;
    }
}

/* virtual */
CObjRep *
FCMPartitioned::FCMPartitionedRoot::createRep(VPNum vp)
{
    FCMPartitioned *rep;
    if (sizeRep) rep = new FCMPartitioned(partitionSize,vp,sizeRep,skipCount);
    else rep = new FCMPartitioned(partitionSize,vp);
    return rep;
}

/* static */
SysStatus
FCMPartitioned::Create(FCMRef &ref, uval ps, PageListDisposition pld, FRRef fr)
{
    FCMPartitionedRoot *root = new FCMPartitionedRoot();
    root->init(ps,pld,fr);
    ref=(FCMRef)(root->getRef());
    return 0;
}

/* static */
SysStatus
FCMPartitioned::Create(FCMRef &ref, uval ps, PageListDisposition pld, 
                       uval initSize, uval skipCount, FRRef fr)
{
    FCMPartitionedRoot *root = new FCMPartitionedRoot();
    root->init(ps,pld,fr, initSize, skipCount);
    ref=(FCMRef)(root->getRef());
    return 0;
}

SysStatus
FCMPartitioned::FCMPartitionedRoot::attachRegion(
    RegionRef regRef, PMRef newPM, AccessMode::mode accessMode)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    tassert(reg == 0,
	    err_printf("FCMPartitionedTrivial second attach\n"));
    reg = regRef;
    pm  = newPM;
    DREF(pm)->attachFCM((FCMRef)getRef());
    return 0;
}

void
FCMPartitioned::destroyPageList()
{
    void *curr;
    uval skip;
    FCMPartPageDesc* pd;
    FreeFrameList ffl;
//    uval cnt=0, IONBcnt=0, IOBcnt=0, Ecnt=0;

    skip = 0;
#ifdef DHASH_RCU_REQUESTCOUNT
    RequestCountWithStopRCU::RequestToken token;
    hashTable.addReference(&token);
#else
    hashTable.addReference();
#endif
    curr = hashTable.getNextAndLock(0, &pd);
    while (curr) {
//        cnt++;
//        if (Ecnt > 40000) breakpoint();
//        if (cnt > 400) {
//            err_printf("curr=%p md=%p offset=%p paddr=%p :"
//                       "Ecnt=%ld IONBcnt=%ld IOBcnt=%ld\n",
//                       curr, md, (void *)(md->getFileOffset()),
//                       (void *)(md->getPAddr()), Ecnt, IONBcnt, IOBcnt);
//            cnt=0;
//        }
        passert(!pd->isDoingIO(), 
                err_printf("do not support io what is going on\n"));
//            Ecnt++;
        uval vaddr;
        // Remember in general local map requests can be occuring
        // until the page descriptor has been completely removed.
        // Hence we first record page info so we can give back
        // after its descriptor has been removed. (this is different
        // from the code paths in which doingIO is set)
        tassertMsg(pd->getLen() == PAGE_SIZE, "list just for fixed size\n");
        vaddr = PageAllocatorKernPinned::realToVirt(pd->getPAddr());
        // Record frame for deallocation
        ffl.freeFrame(vaddr);
        // Remove Page descriptor
        LocalPageDescData::EmptyArg emptyArg = { 0, // doNotify
                                                 0, // skipFn=0
                                                 0 };
        hashTable.doEmpty(pd, DHashTableBase::OpArg(&emptyArg));
        curr = hashTable.getNextAndLock(curr, &pd);
    }
#ifdef DHASH_RCU_REQUESTCOUNT
    hashTable.removeReference(token);
#else
    hashTable.removeReference();
#endif

    // FIXME I think it is safe to do this with no locks or
    // reference to the hash table.  But should probably check this.
    if (ffl.isNotEmpty()) {
        SysStatus rc;
        passert(COGLOBAL(oldpm)!=NULL,
                err_printf("oops oldpm not set..what happend\n"));
	rc = DREF(COGLOBAL(oldpm))->deallocListOfPages((FCMRef)getRef(), &ffl);
	tassert(_SUCCESS(rc), err_printf("dealloc list failed\n"));
    }
}

/* static */ SysStatus 
FCMPartitioned::FCMPartitionedRoot::DestroyPageListMsgHandler(uval msgUval)
{
    DestroyPageListMsg *const msg = (DestroyPageListMsg *)msgUval;
    FCMPartitionedRoot *root=msg->root;
    FCMPartitioned    *rep=msg->rep;
    msg->ack();
    rep->destroyPageList();
    root->recordRemoteDeallocPageList();
    return 0;
}

void
FCMPartitioned::FCMPartitionedRoot::remoteDestroyPageList(FCMPartitioned *rep)
{
    VPNum vp;
    SysStatus rc;
    DestroyPageListMsg msg;

    vp=rep->myvp;
    msg.init(this,rep);

    rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
                                 SysTypes::DSPID(0,vp),
                                 DestroyPageListMsgHandler,
                                 uval(&msg));
    msg.waitForAck();
    tassert(_SUCCESS(rc),err_printf("remote destroyPageList message failed\n"));
}

void
FCMPartitioned::FCMPartitionedRoot::waitForAllDeallocs(uval remoteCount)
{
    // FIXME: well everything else is a kludge so keeping int the spirt
    //        we simply spin waiting.
    while (remoteDestoryPageListCount != remoteCount);
}

FCMPartitioned::~FCMPartitioned()
{
    if (COGLOBAL(destroyPageListDir)==GC) {
        destroyPageList();
    }
    hashTable.cleanup();
//    err_printf("~FCMPartitioned called for rep %p for vp=%ld on vp=%ld",
//               this, myvp, Scheduler::GetVP());
    tassert(myvp==Scheduler::GetVP(),
            err_printf("oops rep destroyed on a vp other than the one"
                       " it was created on\n"));
}

void
FCMPartitioned::FCMPartitionedRoot::locked_destroyPageList()
{
    FCMPartitioned *rep;

    if (destroyPageListDir==CENTRALIZED) {
        lockReps();
        for (void *curr=nextRep(0,(CObjRep *&)rep);
             curr;
             curr=nextRep(curr,(CObjRep *&)rep)) {
            rep->destroyPageList();
        }
        unlockReps();
    } else if (destroyPageListDir==DISTRIBUTED) {
        FCMPartitioned *myrep=NULL;
        VPNum thisvp = Scheduler::GetVP();
        uval count=0;
        remoteDestoryPageListCount=0;
        lockReps();
        for (void *curr=nextRep(0,(CObjRep *&)rep);
             curr;
             curr=nextRep(curr,(CObjRep *&)rep)) {
            if (rep->myvp == thisvp) myrep=rep;
            else {
                remoteDestroyPageList(rep);
                count++;
            }
        }
        unlockReps();
        if (myrep) myrep->destroyPageList();
        waitForAllDeallocs(count);
    } else if (destroyPageListDir==GC) {
        return;
    } else {
        passertMsg(0, "unknowm PageListDisposition\n");
    }
    return;
}

SysStatus
FCMPartitioned::FCMPartitionedRoot::doDestroy()
{

    SysStatus rc;

    _ASSERT_HELD(lock);

    // FXIME: do not support external references
    {   // remove all ObjRefs to this object
	SysStatus rc=exported.close();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    if (reg) {
        lock.release();
        DREF(reg)->destroy();
        lock.acquire();
    }

    tassert(reg==0, err_printf("huh region is still attached\n"));

    if (!uninitPM(pm)) {
	oldpm = pm;
	pm = uninitPM();		// mark in case other activity ongoing
	DREF(oldpm)->detachFCM((FCMRef)getRef());
    }

    // no regions are attached, so all frames are unmapped.  free them
    // note that this call may drop locks if pages are currently in use
    locked_destroyPageList();

    // now destroy lists and seghats
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;
    uval offkey, acckey;
    while (segmentHATList.removeHead(offkey, byAccessList)) {
	while (byAccessList->removeHead(acckey, seghat)) {
	    //err_printf("Destroying %p for acc %lx off %lx\n",
	    //       seghat, acckey, offkey);
	    rc = DREF((SegmentHATShared **)seghat)->sharedSegDestroy();
	    tassert(_SUCCESS(rc), err_printf("oops\n"));
	}
	delete byAccessList;
    }

    // now schedule FCM for freeing
    DREFGOBJ(TheCOSMgrRef)->destroyCO((CORef)getRef(), (COSMissHandler *)this);

    return 0;
}

SysStatus
FCMPartitioned::FCMPartitionedRoot::destroy()
{
    SysStatus rc;

    //err_printf("FCMCommon::destroy() : %p\n", getRef());

    lock.acquire();

    if (beingDestroyed) {
	lock.release();
	return 0;
    }

    beingDestroyed = 1;

    rc = doDestroy();

    lock.release();

    return rc;
}

// Like everything else in this file big kludges here
SysStatus
FCMPartitioned::FCMPartitionedRoot::detachRegion(RegionRef regRef)
{
    //FIXME: adde code to cleanup
    lock.acquire();
    passert(reg == regRef,
	    err_printf("FCMPartitionedTrivial second attach\n"));
    reg = 0;
    lock.release();
    notInUse();
    return 0;
}

SysStatus
FCMPartitioned::FCMPartitionedRoot::noSegMapPageInHAT(
    uval paddr, uval vaddr, AccessMode::pageFaultInfo pfinfo,
    AccessMode::mode access, VPNum vp, HATRef hat, RegionRef regRef,
    uval offset, uval acckey)
{
    SysStatus rc;
    
    uval found;
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;

    uval offkey = offset - vaddr + vaddr&~SEGMENT_MASK;

    lock.acquire();

    // find the correct list-by-access-mode based on offset
    found = segmentHATList.find(offkey, byAccessList);
    if (!found) {
	byAccessList = new SegmentHATByAccessModeList;
	tassert(byAccessList != NULL, err_printf("oops\n"));
	// add byaccess list to list by offset
	segmentHATList.add(offkey, byAccessList);
    }

    // now look for seghat for correct access mode
    found = byAccessList->find(acckey, seghat);
    if (!found) {
	// create segmentHAT for offset and access mode
	rc = SegmentHATShared::Create(seghat);
	tassert(_SUCCESS(rc), err_printf("oops\n"));

	// add seghat to byaccess list
	byAccessList->add(acckey, seghat);
    }

    // now attach segment to HAT
    rc = DREF(hat)->attachRegion(vaddr&~SEGMENT_MASK,SEGMENT_SIZE,seghat);
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    lock.release();

    // have segment attached, retry map request
    // FIXME wasMapped logic needed
    rc = DREF(hat)->mapPage(paddr, vaddr, PAGE_SIZE, pfinfo, access, vp, 1, 0);
    tassert(rc != HAT::NOSEG, err_printf("oops\n"));

    return rc;
}

void
FCMPartitioned::detectModified(FCMPartPageDesc * pg,
                               AccessMode::mode &access,
                               AccessMode::pageFaultInfo pfinfo)
{
    tassert(!pg->isFree() && !(pg->isDoingIO()),
	    err_printf("mapping unavailable page\n"));
    // set locally requires a gather if queried
    pg->setPP(Scheduler::GetVP());


    if (COGLOBAL(mappedExecutable) && !pg->isCacheSynced()) {
	// Machine dependent operation.
	CacheSync((PageDesc *)pg);
    }

    if (!pg->isMapped()) {
        pg->setMapped();
        if (pg->isFree()) {
            // FIXME:  add a local free list here
            pg->clearFree();
        }
    }

    if ((!AccessMode::isWrite(access)) || pg->isDirty()) return;

    if (AccessMode::isWriteFault(pfinfo)) {
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	pg->setDirty();
	return;
    }

    if (AccessMode::makeReadOnly(access)) return;

    // Can't reduce this access mode to read only - so assume
    // frame will be modified and map it
    DILMA_TRACE_PAGE_DIRTY(this,pg,1);
    pg->setDirty();
}

SysStatus
FCMPartitioned::mapPageInHAT(uval paddr, uval vaddr,
                             AccessMode::pageFaultInfo pfinfo,
                             AccessMode::mode access, VPNum vp,
                             HATRef hat, RegionRef regRef,
                             FCMPartPageDesc *pg, uval offset)

{
    SysStatus rc;
    uval wasMapped;
    uval acckey = uval(access);	// need uval for list and original access value

    if (!COGLOBAL(mappedExecutable) && AccessMode::isExecute(access)) {
	// transition to executable  - normally there will be
	// almost nothing mapped unless the file has just been written
	passertMsg(0, "implement me\n");
        //locked_cacheSyncAll();
	COGLOBAL(mappedExecutable) = 1;
    }

    /*
     * mapped is local information for all the processors servered
     * by this rep.
     */
    wasMapped = pg->isMapped();

    // note, access may be modified by call
    detectModified(pg, access, pfinfo);

    // we optimize the common case by not even testing for shared segment
    // before trying to map - is this a good trade?
    // notice that if noSharedSegments is true, we always map here,
    // creating a SegmentHATPrivate if necessary
    rc = DREF(hat)->mapPage(paddr, vaddr, PAGE_SIZE, pfinfo, access, vp,
			    wasMapped, COGLOBAL(noSharedSegments));

    if (rc != HAT::NOSEG) {
	return rc;
    }

    if (!DREF(regRef)->isSharedVaddr(vaddr)) {
    	return DREF(hat)->mapPage(paddr, vaddr, PAGE_SIZE, pfinfo, access, vp,
			    wasMapped, 1 /*create private segment*/);
    }

    rc = COGLOBAL(noSegMapPageInHAT(paddr, vaddr, pfinfo, access, vp, hat,
				      regRef, offset, acckey));
    return (rc);
}

SysStatus
FCMPartitioned::getFrame(FCMPartPageDesc *pd)
{
    SysStatus rc;
    uval paddr;

    tassert(pd->isLocked(), ;);

    uval virtAddr;
    // release lock while allocating
    pd->unlock();  // FIXME:  I don't think it is actuall necessary to drop
                   //         this lock.  As a dead lock can only be caused
                   //         by a recusive fault which is problematic
                   //         regardless. But for the moment we do anyway

    rc =  DREF(COGLOBAL(pm))->allocPages(getRef(), virtAddr, PAGE_SIZE, 0,
	    COGLOBAL(numanode));

    if (_FAILURE(rc)) {
	pd->lock();  // This is safe as doingIO is an existence lock on the
	return rc;
    }

    pd->lock();

    paddr = PageAllocatorKernPinned::virtToReal(virtAddr);

    pd->setPAddr(paddr);

    return 0;
}


FCMPartPageDesc *
FCMPartitioned::findOrAllocatePageAndLock(uval fileOffset, SysStatus &rc,
                                          uval *needsIO) 
{
    FCMPartPageDesc *pd;

    *needsIO = 0;
    rc = 0;
    // find or allocate in the hash table.
    // Note entry is locked on return.  If newly allocated
    // the entry will be set to empty.
    DHashTableBase::AllocateStatus astat =
	hashTable.findOrAllocateAndLock(fileOffset, &pd);

    // **** NOTE a newly allocated PD has doingIO set by default *****
    if (astat == DHashTableBase::FOUND) { return pd; }

    // END OF HOT-PATH
    // FIXME: consider working with only the master copy outside of the hot
    // path

    // Only the allocating thread of a pd ever executes this code
    // Note allocation is serialized so there is only one allocator.
    // The doingIO bit still serves as a existence lock.  The code
    // below relies on this.  May want to revist now that we have
    // a per descriptor lock.
    tassert(pd->isDoingIO(), err_printf("Not doingIO????\n"));
    *needsIO = 1;

    // Miss
    rc=getFrame(pd); // drops and reaquires locks but safe due to
                     // doingIO

    tassert(pd->isLocked(), err_printf("huh he not locked!\n"));

    if (_FAILURE(rc)) {
        passert(0, err_printf("best layed plans of mice and men\n"));
	// must clean up pagedesc we added and wakeup anyone waiting
	// notify but retain lock
	// TODO
	// FIXME: Please verify but it seems wrong not to drop the local lock
	pd->unlock();                        // Fixed with Marc
#if 0
	COGLOBAL(doNotifyAllAndRemove(pd));  // all locks are released on exit
#endif 
	return NULL;	                     // no paging space
    }

    return pd;
}

/*
 * in special case of a zero fill page which gets freed because
 * freeAfterIO is set, return 2 and always unlock
 */

/* virtual */ SysStatus
FCMPartitioned::ioCompleteInternal(FCMPartPageDesc* pg, SysStatus rc,
			       PageFaultNotification* skipFn)
{
    tassert(pg->isLocked(), err_printf("oops\n"));
    if (pg->isFreeAfterIO()) {
	uval vaddr;
	// we are meant to get rid of this page
	//tassert(!pg->mapped, err_printf("Freeing but mapped\n"));
	// indicate that we are given up ownership of pages
	// PageAllocatorKernPinned::clearFrameDesc(pg->paddr);

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(pg->getPAddr());
	DREF(COGLOBAL(pm))->deallocPages(getRef(), vaddr, pg->getLen());

        passert(0, err_printf("ooops fix me\n"));
#if 0
	// notify waiters, but retain lock
	notify(pg, rc, skipFn, 1);
	tassertWrn(pg->freeAfterIO,"Should not have freed page: %p %p\n",
		   pg,pg->fn);
	// remove from page list
	pageList.remove(pg->fileOffset);
	lock.release();
	checkEmpty();
#endif
	return 2;			// page has disappeared
    } else {
	// when either read or write completes
	// page is "clean" since it matches the disk
	DILMA_TRACE_PAGE_DIRTY(this,pg,-1);
	pg->clearDirty();
	pg->clearDoingIO();
	pg->notify(rc, skipFn);
    }
    return 0;
}

SysStatusUval
FCMPartitioned::startFillPage(uval offset, uval paddr,
                               PageFaultNotification* fn,
                               FCMPartPageDesc *pd)
{
    SysStatusUval rc;
    // now initiate I/O operation
    tassert(pd->isDoingIO(),
	    err_printf("startFillPage on unlocked page\n"));

    rc = DREF(COGLOBAL(fr))->startFillPage(pd->getPAddr(), offset);

    tassert(_SUCCESS(rc), err_printf("error on startfillpage\n"));


    if (_SUCCESS(rc) && (rc == FR::PAGE_NOT_FOUND)) {
	uval vpaddr;
	vpaddr = PageAllocatorKernPinned::realToVirt(pd->getPAddr());
        PageCopy::Memset0((void *)vpaddr, PAGE_SIZE);

        pd->lock();
        pd->clearDoingIO();
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
        pd->setDirty();
        pd->setCacheSynced();
        rc = ioCompleteInternal(pd,0,fn);
	passert(rc == 0, err_printf("oops the page has been freed\n"));
	// above acquires local lock
	tassert(pd->isLocked(), err_printf("oops he not locked\n"));
	return 0;
    } else {
        passert(0, err_printf("only support zero filling"));
    }
    // return indicates a wait on fn is needed
    return 1;
}

SysStatusUval
FCMPartitioned::getPageInternal(
    uval fileOffset, PageFaultNotification *fn,
    FCMPartPageDesc **pd)
{
    SysStatus rc;
    uval needsIO;

    while (1) {
	*pd = findOrAllocatePageAndLock(fileOffset, rc, &needsIO);

	if (!*pd) {
	    tassert( _FAILURE(rc), err_printf("huh\n"));
	    return rc;
	}

	if ((*pd)->isFree()) (*pd)->clearFree();  

	if (!((*pd)->isDoingIO())) {
	    tassert(!needsIO, err_printf("oops\n"));
	    return 0;
	}

	// END OF HOT-PATH
	uval paddr = (*pd)->getPAddr();  // set for us by getFrame
	if (fn) {
	    fn->next = (*pd)->getFN();
	    (*pd)->setFN(fn);
	    (*pd)->unlock();
	    if (needsIO) {
		// Look at this
		rc = startFillPage(fileOffset, paddr, fn, *pd);
		return rc;
	    }
	    return 1;
	}

	// block here synchronously
	PageFaultNotification notification;
	notification.initSynchronous();
	notification.next = (*pd)->getFN();
	(*pd)->setFN(&notification);

	(*pd)->unlock();

	if (needsIO) {
	    // now initiate I/O operation
	    rc = startFillPage(fileOffset, paddr, &notification, *pd);
	    tassert(_SUCCESS(rc), err_printf("error on startfillpage\n"));
	    if (rc==0) {
		return 0;
	    }
	}

	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}
    }
}

void
FCMPartitioned::FCMPartitionedRoot::updateRegionPPSet(RegionRef regRef)
{
    passert(reg==regRef, err_printf("huh only supports a single region"));
    lock.acquire();
    regPPSet.addVP(Scheduler::GetVP());
    lock.release();
}

/* virtual */
SysStatusUval
FCMPartitioned::mapPage(uval fileOffset,
        uval regionVaddr,
        uval regionSize,
        AccessMode::pageFaultInfo pfinfo,
        uval vaddr,
        AccessMode::mode access,
        HATRef hat, VPNum vp,
        RegionRef regRef, uval firstAccessOnPP,
        PageFaultNotification *fn) 
{
    VPNum repIdx;
    FCMPartitioned *rep;

    fileOffset += vaddr - regionVaddr;
    repIdx=(fileOffset/partitionSize);
    rep=COGLOBAL(repArray[repIdx].rep);

    if (!rep) {
        // try to take ownership of this partition
        CompareAndStoreSynced((uval *)(&(COGLOBAL(repArray[repIdx].rep))),
                              (uval)0, (uval)this);
        // Even if we fail that's ok someone else beat us to it so
        // we simply defer by ensuring we go with the winner
        rep=COGLOBAL(repArray[repIdx].rep);
    }

    tassert(rep!=0, err_printf("oops no rep for %ld\n",repIdx));
#if 0
    if (rep!=this) err_printf("non local access\n");
#endif 
    return rep->mapPageInternal(fileOffset, regionVaddr, regionSize, pfinfo,
                                vaddr, access, hat, vp, regRef, firstAccessOnPP,
                                fn);
    
}

SysStatusUval
FCMPartitioned::mapPageInternal(uval fileOffset,
        uval regionVaddr,
        uval regionSize,
        AccessMode::pageFaultInfo pfinfo,
        uval vaddr,
        AccessMode::mode access,
        HATRef hat, VPNum vp,
        RegionRef regRef, uval firstAccessOnPP,
        PageFaultNotification *fn) {
    SysStatusUval rc;
    FCMPartPageDesc *pd;

    if (firstAccessOnPP) COGLOBAL(updateRegionPPSet(regRef));

 retry:
    rc = getPageInternal(fileOffset, fn, &pd);
    /*
     * rc == 0 if page was gotten and locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */
    if (_SUCCESS(rc) && (rc == 0)) {
	//err_printf("mapping %lx\n", offset);
    	rc = mapPageInHAT(pd->getPAddr(), vaddr, pfinfo, access, vp, hat,
                          regRef, pd, fileOffset);
	//err_printf("done mapping %lx\n", offset);
	pd->unlock();
	if (_SGENCD(rc) == ENOMEM) {
	    err_printf("No mem on mapPage: sleeping and retrying\n");
	    Scheduler::DelayMicrosecs(100000);
	    goto retry;
	}
	return rc;
    }

    // I've been sloppy about returning the FaultID all the way up
    // so just get it right.  Debugging a mistake here is hopeless
    if (_SUCCESS(rc)) {
	/* if rc is 0, there may not be an fn, so we need to check
	 * when fn is null, getPageInternal blocks until it can
	 * return success (rc == 0) or a real error
	 */
	return fn->getPageFaultId();
    }
    return rc;
}

template DHashTable<FCMPartPageDesc,FCM_PART_ALLOC>;
template MasterDHashTable<FCMPartPageDesc,FCMPartPageDesc,
                          FCM_PART_ALLOC,FCM_PART_ALLOC>;
