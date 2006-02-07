/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000-2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMRoot.C,v 1.61 2005/06/27 06:15:53 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Root of all PMs plus some FCMs
 * **************************************************************************/

#include "kernIncs.H"
#include "defines/paging.H"
#include "mem/FCM.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PMRoot.H"
#include "proc/Process.H"
#include "cobj/sys/COSMgrObject.H"
#include <stub/StubKBootParms.H>

class PMRoot::MyRoot : public CObjRootMultiRepPinned {
protected:
    LockType      lock;		// lock on object
    
    FreeFrameList freeFrameLists;

    // FIXME: make an array of these
    uval largePageSize;
    FreeFrameList freeLargePages;	// for now, just one size
    uval largePageTotalCount;		// yes only one size
    
    //FIXME:  All a big Kludge until we are ready to do the "right" thing
    uval exploitMultiRepFlag;
    PMRoot *firstRep;
    uval exploitMultiRep() { return (exploitMultiRepFlag==1); }
    uval numVPs;
    PMRoot *repArray[Scheduler::VPLimit];
    
    PMRoot *locateRep(FCMRef fcm);
    uval  numPM;
    
    MemLevelState currMemLevel; 	// memory available
    
    // if going from LOW to HIGH, dirUp is true, else false
    // i.e., keep pushing pager until hit high water mark
    uval dirUp;
    
    // FIXME: change to a hash for quick remove...
    ListSimpleKey<PMRef, uval, AllocGlobal> PMList;
    void scanFCMs();
    
    void scanPMs();
    
    PMRef  lastPMScanned;		// last pm to be scanned
    virtual CObjRep * createRep(VPNum vp);
    DEFINE_PINNEDGLOBALPADDED_NEW(PMRoot::MyRoot);
    MyRoot(RepRef rep, uval exploitMultiRep=1);
    
    uval scannerActive;		// one may be running, so don't bother
    void realDoScans();
    static void CallRealDoScans(uval ptrThis);
    
    // tell PMRoot to kick pager
    virtual SysStatus kickPaging();
    
    // attach/detach a pm to/from this pm ('this' becomes 'pm's parent)
    virtual SysStatus attachPM(PMRef pm);
    virtual SysStatus detachPM(PMRef pm);
    
    struct BlockedPageableThreads {
	ThreadID    thread;		// blocked thread;
	struct BlockedPageableThreads *next;
	uval notified;
    } *blockedThreadHead;
    
    // set the memory level in all reps, if low, gives back all frames
    virtual void setMemLevel();
    virtual void blockPageable();
    
    void freeFreeFrameList(FreeFrameList *ffl) {
	lock.acquire();
	freeFrameLists.freeList(ffl);
	lock.release();
    }
    
    // if upto is set, then we just want up to the amount
    uval getFreeFrameList(uval count, FreeFrameList *ffl) {
	uval rc;
	lock.acquire();
	rc = freeFrameLists.getList(count, ffl);
	lock.release();
	return rc;
    }
    
    // if upto is set, then we just want up to the amount
    void getList(FreeFrameList *ffl) {
	lock.acquire();
	freeFrameLists.getList(ffl);
	lock.release();
    }
    

    // Push all free frames back to allocator
    SysStatus pushAllFreeFrames();

    // for testing - print summary info
    virtual SysStatus print();
    
    void locked_print();
    
    void printFreeListStats();
    
    friend class PMRoot;

    virtual void getLargePageInfo(uval& largePgSize, uval& largePgReservCount,
			     uval& largePgFreeCount) {
	largePgSize        = largePageSize;
	largePgReservCount = largePageTotalCount;
	largePgFreeCount   = freeLargePages.getCount();
    }
};


uval 
PMRoot::exploitMultiRep() 
{ 
    return (COGLOBAL(exploitMultiRepFlag)==1); 
}

/* virtual */ SysStatus 
PMRoot::attachPM(PMRef pm) 
{
    return COGLOBAL(attachPM(pm));
}

/* virtual */ SysStatus
PMRoot::detachPM(PMRef pm) 
{
    return COGLOBAL(detachPM(pm));
}

/* virtual */ SysStatus
PMRoot::kickPaging() 
{ 
    return COGLOBAL(kickPaging());
}

/* virtual */ SysStatus
PMRoot::print() 
{
    return COGLOBAL(print());
}

/* virtual */ SysStatus
PMRoot::printFreeListStats() 
{
    COGLOBAL(printFreeListStats()); return 0;
}

inline PM::MemLevelState 
getNewMemLevel() {
    PM::MemLevelState newLevel;
    DREFGOBJK(ThePinnedPageAllocatorRef)->getMemLevelState(newLevel);
    return newLevel;
}

PMRoot::MyRoot::MyRoot(RepRef rep, uval exploitMultiRep /*=1*/) : 
    CObjRootMultiRepPinned(rep), exploitMultiRepFlag(exploitMultiRep)
{
    lastPMScanned = NULL; numPM = 0;
    scannerActive = 0;
    // in debug we initialize repArray to nulls to catch errors
    tassert(memset(repArray, 0, sizeof(PMRoot *)*Scheduler::VPLimit), ;);
    // Since we know the kernel is instantiated on every processor
    // We know that we will have a rep per processor so for convinence
    // we record the number of reps we create as the vp count;
    numVPs = 0;
    freeFrameLists.init();
    freeLargePages.init();
    largePageSize = 0;
    largePageTotalCount = 0;
    currMemLevel = PM::HIGH;
    dirUp = 0;
    lock.init(); 
    blockedThreadHead = NULL;
    
}

PMRoot::PMRoot()
{
    repLock.init();
    freeFrameList.init();
    lastFCMScanned = NULL;
    numFCM = 0;
    currMemLevel = PM::HIGH;
}

/****************************************************************************
* Given that this is a kernel system wide object we instantiate all
* reps when the system is started this allows us to not worry about
* the rep set changing during the life time of the object (= system)
* Further we do not need to worry about destruction issues as it should
* never go away.  Note there are things that could break these assumptions:
*    Hot swapping of Hardware
*    Rep migration / restructuring
*****************************************************************************/
/* static */ void
PMRoot::ClassInit(VPNum vp, uval exploitMultiRep /* = 1*/)
{
    MyRoot *myRoot;

    err_printf("class init on PMRoot\n");
    if (vp == 0) {
        myRoot = new MyRoot((RepRef)GOBJK(ThePMRootRef), exploitMultiRep);
        passert(myRoot!=NULL, err_printf("No mem for PMRoot\n"));
    } 
    // To make life easier we instantiate all reps on all vp at startup
    // by making an external call to the single instance
    DREF(((PMRoot **)GOBJK(ThePMRootRef)))->establishRep();
    err_printf("class init on PMRoot done\n");
}

// second phase for parge pages needed to be later because we need to get
// environment variable
SysStatus 
PMRoot::ClassInit2(VPNum vp)
{
    if (vp != 0) return 0;

    COGLOBAL(largePageSize) = LARGE_PAGES_SIZE;
    
    char *const varname = "K42_NUMB_LARGE_PAGES_RESERVED";
    char varvalue[128];

#ifdef LARGE_PAGES_FIXED_POOLS
    // get from environment variable at initialization time
    uval numbLargePagesReserved;

    uval i, tmpptr;
    SysStatus rc;

    rc = StubKBootParms::_GetParameterValue(varname, varvalue, 128);
    if (_FAILURE(rc) || (varvalue[0] == '\0')) {
	err_printf("%s undefined.  Using 0\n", varname);
	numbLargePagesReserved = 0;
    } else {
	numbLargePagesReserved = baseAtoi(varvalue);
    }


    rc = 0;
    err_printf("allocating fixed pool of %ld large pages with size 0x%lx\n", 
	       numbLargePagesReserved, COGLOBAL(largePageSize));
    for (i=0; i<numbLargePagesReserved; i++) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPagesAligned(tmpptr, COGLOBAL(largePageSize), 
			      COGLOBAL(largePageSize), 0, 0, 
			      PageAllocator::LOCAL_NUMANODE);
	tassertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	COGLOBAL(freeLargePages.freeFrame(tmpptr));
    }

    COGLOBAL(largePageTotalCount) = numbLargePagesReserved;
#else
    err_printf("starting without fixed pools for large pages\n");
#endif
    
    return rc;
}

/* virtual */ SysStatus
PMRoot::establishRep()
{
    return 0;
}

CObjRep *
PMRoot::MyRoot::createRep(VPNum vp)
{
    PMRoot *rep = new PMRoot;
    passert(rep!=NULL, err_printf("No mem for PMRoot\n"));
    if (vp==0) firstRep=rep;
    numVPs++;
    repArray[vp]=rep;
    return rep;
}

PMRoot *
PMRoot::MyRoot::locateRep(FCMRef fcm)
{
    // FIXME:  All a big kludge because I am a chicken.  Fix this when ready
    //         to do a proper distributed implementation with hotswapping.
    if (!exploitMultiRep()) return firstRep;

    VPNum vp = COSMgrObject::refToVP((CORef)fcm);
    tassert(repArray[vp]!=NULL, err_printf("What empty no rep for %ld", vp));
    return repArray[vp];
}

void
PMRoot::repAttachFCM(FCMRef fcm) 
{
    uval tmp=0;
    AutoLock<LockType> al(&repLock); // locks now, unlocks on return
    tassert(!FCMHash.find(fcm, tmp), err_printf("re-attach\n"));
    numFCM++;
    FCMHash.add(fcm, tmp);
}

SysStatus
PMRoot::attachFCM(FCMRef fcm)
{
    PMRoot *rep=NULL;

    rep=COGLOBAL(locateRep(fcm));
    tassert(rep, err_printf("unable to locate a rep for fcm=%p\n", fcm));
    rep->repAttachFCM(fcm);
    return 0;
}

void
PMRoot::repDetachFCM(FCMRef fcm)
{
    uval tmp;
    AutoLock<LockType> al(&repLock); // locks now, unlocks on return
    //err_printf("PM %lx detaching fcm %lx\n", getRef(), fcm);

    if (FCMHash.remove(fcm, tmp)) {
        numFCM--;
        return;
    }
    tassert(0, err_printf("detach null\n"));
}

SysStatus
PMRoot::detachFCM(FCMRef fcm)
{
    PMRoot *rep=NULL;

    rep=COGLOBAL(locateRep(fcm));
    tassert(rep, err_printf("unable to locate a rep for fcm=%p\n", fcm));
    rep->repDetachFCM(fcm);
    return 0;
}


SysStatus
PMRoot::MyRoot::attachPM(PMRef pm)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    numPM++;
    //tassert(!PMList.find(pm, 0), err_printf("re-attach\n"));

    PMList.add(pm, 0);
    return 0;
}

SysStatus
PMRoot::MyRoot::detachPM(PMRef pm)
{
    uval tmp;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    //err_printf("PM %lx detaching pm %lx\n", getRef(), pm);

    tassert(PMList.find(pm, tmp), err_printf("detach null\n"));
    PMList.remove(pm, tmp);
    numPM--;

    //locked_print();
    return 0;
}


SysStatus
PMRoot::allocPagesInternal(uval &ptr, uval size, uval pageable, uval flags, 
			   VPNum node)
{
    SysStatus rc;
    
    if ((size == PAGE_SIZE) && (node==PageAllocator::LOCAL_NUMANODE) &&
       getFrame(ptr)) {
	return 0;
    }

    if (size == PAGE_SIZE) {
	COGLOBAL(setMemLevel());
	if (currMemLevel != PM::HIGH) { 
	    // if its low/crit, or its medium and direction is up
	    if (PM::IsLow(currMemLevel) || (COGLOBAL(dirUp))) {
		kickPaging();
	    }
	    if (pageable) { // block pageable requests until non crit
		COGLOBAL(blockPageable());
	    }
	}
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(ptr,size,flags, node);
	tassertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	return rc;
    }

    // for paging, we always allocate on a boundary aligned to the size
    // if machines that don't need this appear, this policy must become
    // machine dependent.  I don't expect that because without this rule
    // the hardware must do an add in the virt to real conversion.

#ifndef LARGE_PAGES_FIXED_POOLS
    //err_printf("allocating large page not from pool\n");
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	allocPagesAligned(ptr, size, size, 0, flags, node);
    tassertMsg((_SUCCESS(rc)), "woops, out of memory\n");
#else
    //err_printf("allocating large page\n");
    passertMsg( (size == COGLOBAL(largePageSize)), 
		"woops, page size %ld not supported\n",
		size);
    ptr = COGLOBAL(freeLargePages.getFrame());
    if (ptr == 0) {
	tassertWrn((ptr != 0), "woops, ran out of large pages\n");
	rc = -1;			// FIXME: ERROR
    } else {
	rc=0;
    }

#endif
    return rc;
}

SysStatus
PMRoot::allocPages(FCMRef fcm, uval &ptr, uval size, uval pageable, uval flags, 
		   VPNum node)
{
    return allocPagesInternal(ptr, size, pageable, flags, node);
}

SysStatus
PMRoot::allocListOfPages(FCMRef fcm, uval count, FreeFrameList *ffl)
{
    return getFreeFrameList(count, ffl);
}

SysStatus
PMRoot::allocListOfPages(PMRef pm, uval count, FreeFrameList *ffl)
{
    return getFreeFrameList(count, ffl);
}

SysStatus
PMRoot::deallocPages(FCMRef fcm, uval paddr, uval size)
{
    SysStatus rc;
    if (size == PAGE_SIZE) {
	freeFrame(paddr);
	rc = 0;
    } else {
#ifndef LARGE_PAGES_FIXED_POOLS
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(paddr, size);
	tassertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	COGLOBAL(setMemLevel());
#else
	tassertMsg(size == COGLOBAL(largePageSize), "woops\n");
	COGLOBAL(freeLargePages.freeFrame(paddr));
	rc = 0;
#endif
    }
    return rc;
}

SysStatus
PMRoot::deallocListOfPages(FCMRef fcm, FreeFrameList *ffl)
{
    freeList(ffl);
    return 0;
}

SysStatus
PMRoot::allocPages(PMRef pm, uval &ptr, uval size, uval pageable, uval flags, 
		   VPNum node)
{
    return allocPagesInternal(ptr, size, pageable, flags, node);
}

SysStatus
PMRoot::deallocPages(PMRef pm, uval paddr, uval size)
{
    SysStatus rc;
    if (size == PAGE_SIZE) {
	freeFrame(paddr);
	rc = 0;
    } else {
#ifndef LARGE_PAGES_FIXED_POOLS
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(paddr, size);
	tassertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	COGLOBAL(setMemLevel());
#else
	tassertMsg(size == COGLOBAL(largePageSize), "woops\n");
	COGLOBAL(freeLargePages.freeFrame(paddr));
	rc = 0;
#endif
    }
    return rc;
}

SysStatus
PMRoot::deallocListOfPages(PMRef pm, FreeFrameList *ffl)
{
    freeList(ffl);
    return 0;
}

void
PMRoot::freeFrame(uval addr)
{
    if (!exploitMultiRep() && (COGLOBAL(firstRep) != this)) {
        return COGLOBAL(firstRep)->freeFrame(addr);
    }
    repLock.acquire();
    if (PM::IsLow(currMemLevel)) {
	if (freeFrameList.getCount() != 0) {
	    // this shouldn't happen, but for simplicity am tolerating now
	    // relaly, if memory is low these lists shold be empty
	    uval num = freeFrameList.getCount();
	    DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(
		&freeFrameList);
	    tassertMsg((freeFrameList.getCount() == 0), 
		       "woops, should be empty after give back\n");
	    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
		if (num > 256) {
		    err_printf("[Ret %lu]", num);
		}
	    }
	}
	    
	SysStatus rc;
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(addr, 
								PAGE_SIZE);
	tassertMsg((_SUCCESS(rc)), "woops, out of memory\n");
	
	repLock.release();
	COGLOBAL(setMemLevel()); 
	return;
    }
	
    // regular path when memory not low, lock still held
    freeFrameList.freeFrame(addr);
    if (freeFrameList.getCount() < ELEM_IN_FREE_FRAME_LIST) {
	repLock.release();
	return ;
    }	

    // release the tail of list to global list
    FreeFrameList fflToFree;
    freeFrameList.getListTail(&fflToFree);
    
    tassertMsg((fflToFree.isNotEmpty()), "woops, was full???\n");
    repLock.release();
    COGLOBAL(freeFreeFrameList(&fflToFree));
}

uval
PMRoot::getFrame(uval& addr)
{
    if (!exploitMultiRep() && (COGLOBAL(firstRep) != this)) {
	return COGLOBAL(firstRep)->getFrame(addr);
    }
    repLock.acquire();
    addr = freeFrameList.getFrame();
    if (addr != 0) {
	repLock.release();
	return 1;
    }
    
    // need to fetch another chunk of frames from MyRoot
    repLock.release();
    
    FreeFrameList ffl;
    COGLOBAL(getList(&ffl));
    if (ffl.getCount() == 0) return 0;

    // there is a race which might make the list longer... thats okay
    repLock.acquire();
    freeFrameList.freeList(&ffl);
    addr = freeFrameList.getFrame();
    tassertMsg((addr != 0), "woops???\n");
    repLock.release();

    return 1;
}

void
PMRoot::freeList(FreeFrameList *ffl)
{
    if (PM::IsLow(currMemLevel)) {
	DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(ffl);
	tassertMsg( (ffl->getCount() == 0), 
		   "woops, should be empty after give back\n");
	COGLOBAL(setMemLevel());
    } else {	
	uval count;

	if (!exploitMultiRep() && (COGLOBAL(firstRep) != this)) {
	    return COGLOBAL(firstRep)->freeList(ffl);
	}
	repLock.acquire();
	freeFrameList.freeList(ffl);
	count = freeFrameList.getCount();
	
	while (count > ELEM_IN_FREE_FRAME_LIST) {
	    // release the tail of list to global list
	    FreeFrameList fflToFree;
	    freeFrameList.getListTail(&fflToFree);
	    
	    tassertMsg((fflToFree.isNotEmpty()), "woops, was full???\n");
	    repLock.release();
	    COGLOBAL(freeFreeFrameList(&fflToFree));
	    repLock.acquire();
	    count = freeFrameList.getCount();
	}
	repLock.release();
    }
}

SysStatus
PMRoot::getFreeFrameList(uval count, FreeFrameList *ffl)
{
    SysStatus rc = 0;
    if (!exploitMultiRep() && (COGLOBAL(firstRep) != this)) {
        return COGLOBAL(firstRep)->getFreeFrameList(count, ffl);
    }
    repLock.acquire();
    // first try to get from free frame lists
    count = freeFrameList.getList(count, ffl);
    repLock.release();

    while(count--) {
	uval ptr;

	// try to get from root frame lists via get frame, if failes
	// go to pinned allocator
	if (getFrame(ptr) == 0 ) {
	    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
		allocPages(ptr,PAGE_SIZE);
	    if(_FAILURE(rc)) {
		// if allocate failed, return list since caller will 
		// assume that error means no pages allocated
		freeList(ffl);
		return rc;
	    }
	}
	ffl->freeFrame(ptr);
    }
    return rc;
}


void
PMRoot::MyRoot::blockPageable()
{
    lock.acquire();
    while (currMemLevel == PM::CRITICAL) {
	struct BlockedPageableThreads me;
	me.next = blockedThreadHead;
	me.thread = Scheduler::GetCurThread();
	me.notified = 0;
	blockedThreadHead = &me;
	do {
	    lock.release();
	    Scheduler::Block();
	    lock.acquire();
	} while (me.notified == 0);
    }
    lock.release();
}

/*
 * this is all done holding the lock.  Note that the PageAllocator
 * must call this without its lock.  The hierarchy is PMRoot::MyRoot
 * repLock, pageallocator lock.
 * 
 * This has to be the hierarchy, since PM's do dynamic memory allocation,
 * and hence implicitly call the page allocator.
 */
void
PMRoot::MyRoot::setMemLevel()
{
    uval num;
    FreeFrameList ffl;
    MemLevelState newLevel;

    lock.acquire();

    // get the memlevel under protection of lock, 
    // to force consistent setting on reps, don't drop through this routine
    newLevel = getNewMemLevel();
    if (newLevel == currMemLevel) {
	lock.release();
	return;
    }

    if(newLevel != PM::CRITICAL) {
	while (blockedThreadHead != NULL) {
	    blockedThreadHead->notified = 1;
	    Scheduler::Unblock(blockedThreadHead->thread);
	    blockedThreadHead = blockedThreadHead->next;
	}
    }

    // if direction up and hit high, go down
    if ( dirUp && (newLevel == PM::HIGH) ) {
	dirUp = 0; err_printf("D");
    } else if ( !dirUp && PM::IsLow(newLevel) ) {
	dirUp = 1; err_printf("U"); // start going up
    }
	
#if 0
    switch(newLevel) {
    case PM::CRITICAL:
	err_printf("C");
	break;
    case PM::LOW:
	err_printf("L");
	break;
    case PM::MID:
	err_printf("M");
	break;
    case PM::HIGH:
	err_printf("H");
	break;
    default:
	tassertMsg(0, "woops\n");
    }
#endif
    currMemLevel = newLevel;
    if (PM::IsLow(newLevel)) {
	if (freeFrameLists.getCount() > 0) {
	    ffl.freeList(&freeFrameLists);
	}
    }
    if (PM::IsLow(newLevel)) {
	num = ffl.getCount();
	
	DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(&ffl);
	tassertMsg((ffl.getCount() == 0), 
		   "woops, should be empty after give back\n");
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    if (num > 256) {
		err_printf("[Ret %lu]", num);
	    }
	}
    }

    for(uval i=0; i<numVPs;i++) {
	tassert(repArray[i]!=NULL, err_printf("What no rep fro vp=%ld",i));
	repArray[i]->setMemLevel(newLevel);
    }
    lock.release();
}

SysStatus 
PMRoot::MyRoot::pushAllFreeFrames()
{
    uval addr;
    SysStatus rc;
    void *iter = NULL;
    PMRef pm;
    uval tmp;

    lock.acquire();

    // Get PMLeafs to free any frames they may have cached
    // There will be a problem if the PMLeafs free back to the PMRoot
    while ((iter = PMList.next(iter, pm, tmp)) != NULL) {
	DREF(pm)->freeCachedFrames();
    }

    // First push all free frames I have
    while ( (addr = freeFrameLists.getFrame()) ) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(addr, 
								PAGE_SIZE);
	tassertMsg((_SUCCESS(rc)), "Couldn't dealloc page?");
    }

    // Get all the reps to push their free frames back
    for(uval i=0; i<numVPs;i++) {
	tassert(repArray[i]!=NULL, err_printf("What no rep for vp=%ld",i));
	repArray[i]->pushAllFreeFrames();
    }
    

    lock.release();

    return 0;
}

void
PMRoot::MyRoot::printFreeListStats()
{
    uval count;
    uval sum=0;
    PMRoot *rep;
    
    for (uval i=0; i<numVPs; i++) {
        rep = repArray[i];
        rep->repLock.acquire();
        count=rep->freeFrameList.getCount();
        rep->repLock.release();
        sum+=count;
        err_printf("rep %ld : %p freeFrame Count = %ld\n", i, 
                   rep, count);
    }
    lock.acquire();
    count=freeFrameLists.getCount();
    lock.release();
    sum+=count;
    err_printf("root : %p freeFrame Count = %ld\n", this,
               count);
    err_printf("total free pages in PMRoot = %ld\n", sum);
}

/* virtual */ void

PMRoot::setMemLevel(MemLevelState memLevelState)
{
    repLock.acquire();
    currMemLevel = memLevelState;
    if (PM::IsLow(currMemLevel)) {
	uval num = freeFrameList.getCount();
	DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(&freeFrameList);
	tassertMsg((freeFrameList.getCount() == 0), 
		   "woops, should be empty after give back\n");
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    if (num > 256) {
		err_printf("[Ret %lu]", num);
	    }
	}
    }
    repLock.release();
}

void
PMRoot::MyRoot::scanFCMs()
{
    if (!exploitMultiRep()) {
        firstRep->scanFCMs();
    } else {
        for (uval i=0; i<numVPs;i++) {
            tassert(repArray[i]!=NULL, err_printf("What no rep fro vp=%ld",i));
            repArray[i]->scanFCMs();
        }
    }
}

SysTime lastran=0;

void
PMRoot::MyRoot::realDoScans()
{
    SysTime diff, curr;
    setMemLevel();
    // if paging is off, only scan if critical
    if (KernelInfo::ControlFlagIsSet(KernelInfo::PAGING_OFF)) {
	if (currMemLevel != PM::CRITICAL) return;
	err_printf("OUT OF MEMORY: running pager\n");
    }

    do {
	curr = Scheduler::SysTimeNow();
	if (curr < lastran) {
	    lastran = curr;
	}
	diff = curr-lastran;
	lastran = curr;
	// run once per 500msec at most for now
	if (diff < (Scheduler::TicksPerSecond())) {
	    Scheduler::DelayMicrosecs(1000000);
	}
	
	
	err_printf("[");
	// go through all FCM's directly connected to me
	scanFCMs();			
	
	// go through all PMs 
	scanPMs();
	
	setMemLevel();
	
	// race condition, but next allocate will catch this
#ifdef PAGING_VERBOSE
	lock.acquire();
	locked_print();
	lock.release();
#endif
	err_printf("]");
    } while (PM::IsLow(currMemLevel));
    scannerActive = 0;
}

/* static */ void
PMRoot::MyRoot::CallRealDoScans(uval ptrThis)
{
    ((PMRoot::MyRoot *)ptrThis)->realDoScans();
}


/* virtual */ SysStatus
PMRoot::MyRoot::kickPaging()
{
    // pinned allocator called with our lock held sometimes, so can't acquire
    // lock
    if (!scannerActive) {
	scannerActive = 1;
	SysStatus rc;
	rc = Scheduler::ScheduleFunction(CallRealDoScans, uval(this));
	passertMsg(_SUCCESS(rc), "scheduling of thread to page failed\n");
    }
    return 0;
}

void
PMRoot::scanFCMs()
{
    const uval maxListSize = 20;
    uval numScan, i;
    FCMRef fcmList[maxListSize];
    FCMRef   fcm;
    SysStatus rc=0;
    uval tmp;

    repLock.acquire();

    if (numFCM == 0) {
	repLock.release();
	return;
    }

    // collect a list of FCMs to scan; we make copy since we can't make
    // fcm call with lock held

    numScan = numFCM / 4;

    if (numFCM > 0 && numFCM <= 4) numScan = 1;
    // temp hack - logic to be completely reworked
    if (numScan > maxListSize) numScan = maxListSize;

    // ugly hack; we continue scanning at the lastFCMScanned last time
    // Given a hash table we might actually
    // miss some elements since the table may resize between scans but
    // eventually we will scan the all.
    if (lastFCMScanned==NULL) {
        // if we lastFCMScanned is null start with the first fcm 
        rc = FCMHash.getFirst(fcm, tmp);
    } else {
        fcm = lastFCMScanned;
        rc = FCMHash.getNextWithFF(fcm, tmp);
        if (rc!=1) {
            rc=FCMHash.getFirst(fcm, tmp);
        }
    }
    tassert(rc==1, err_printf("if we got here there must be at least 1 fcm in"
                              "hash\n"));
    for (i = 0; i < numScan; i++ ) {
        fcmList[i] = fcm;
	lastFCMScanned = fcm;
        rc = FCMHash.getNext(fcm, tmp);
        tassert(rc==1 || rc==0, err_printf("oops\n"));
        if (rc==0) {
            // at end, just wrap around to beginning
            rc=FCMHash.getFirst(fcm, tmp);
            tassert(rc==1, err_printf("FCM Hash cannot be empty here!\n"));
        }
    }

    repLock.release();

    for (i = 0; i < numScan; i++ ) {
	rc = DREF(fcmList[i])->giveBack(currMemLevel);
	if (currMemLevel == PM::HIGH) break;
    }
}

void
PMRoot::MyRoot::scanPMs()
{
    const uval maxListSize = 20;
    uval numScan, i;
    PMRef pmList[maxListSize];
    PMRef   pm;
    void    *iter;
    SysStatus rc;
    uval tmp;

    lock.acquire();

    if (numPM == 0) {
	lock.release();
	return;
    }

    // collect a list of PMs to scan; we make copy since we can't make
    // pm call with lock held

    // ugly hack; we iterate until we find the last one we scanned, and
    // then continue from there
    iter = NULL;
    if (lastPMScanned != NULL) {
	while ((iter = PMList.next(iter, pm, tmp)) != NULL) {
	    if (pm == lastPMScanned) break;
	}
    }

    numScan = numPM / 4;
    if (numPM > 0 && numPM <= 4) numScan = 1;
    // temp hack - logic to be completely reworked
    if (numScan > maxListSize) numScan = maxListSize;
    for (i = 0; i < numScan; i++ ) {
	iter = PMList.next(iter, pm, tmp);
	if (iter == NULL) {
	    // at end, just wrap around to beginning
	    iter = PMList.next(iter, pm, tmp);
	    tassert(iter != NULL, err_printf("oops\n"));
	}
	pmList[i] = pm;
    }
    lastPMScanned = pm;

    lock.release();

    for (i = 0; i < numScan; i++ ) {
	rc = DREF(pmList[i])->giveBack(currMemLevel);
	if (currMemLevel == PM::HIGH) break;
    }
}

void
PMRoot::printFCMList()
{
    FCMRef   fcm;
    void    *iter;
    uval    count;
    Summary fcmSummary;
    uval fcmInUse = 0, tmp;

    repLock.acquire();

    iter = NULL;
    count = 0;
    if (FCMHash.getFirst(fcm, tmp)) {
        while (1) {        
            count++;
            err_printf("\tfcm %p: ", fcm);
	    // special case, this funciton aquires no locks, so no deadlock
            DREF(fcm)->getSummary(fcmSummary);
            err_printf(" fcm %ld\n", fcmSummary.total);
            fcmInUse += fcmSummary.total;
            if (FCMHash.getNext(fcm, tmp)!=1) break;
        }
    }
    if (count != numFCM) {
	err_printf("FCM Count error - numFCM=%ld found %ld\n",
		   numFCM, count);
    }
    err_printf("fcmInUse %ld\n", fcmInUse);
    repLock.release();
}

void
PMRoot::MyRoot::locked_print()
{
    PMRef pm;
    void *iter;
    uval count, tmp;

    _ASSERT_HELD(lock);

    err_printf("PMRoot %p; \n", getRef());

    if (!exploitMultiRep()) {
        firstRep->printFCMList();
    } else {
        for (uval i=0; i<numVPs; i++) {
            tassert(repArray[i]!=NULL, err_printf("What no rep fro vp=%ld",i));
            repArray[i]->printFCMList();
        }
    }

    iter = NULL;
    count = 0;
    while ((iter = PMList.next(iter, pm, tmp)) != NULL) {
	count++;
	err_printf("\tpm %p: \n", pm);
	DREF(pm)->print();
    }
    if (count != numPM) {
	err_printf("PM Count error - numPM=%ld found %ld\n", numPM, count);
    }

    // get the kernel pm statistics
    ((Process*)(DREFGOBJ(TheProcessRef)))->getPM(pm);
    DREF(pm)->print();
}


SysStatus
PMRoot::giveBack(PM::MemLevelState memLevelState)
{
    tassertMsg(0, "PMRoot::giveBack called, whats this suppsoed to do\n");
    return 0;
}

SysStatus
PMRoot::pushAllFreeFramesAllReps() 
{
    return COGLOBAL(pushAllFreeFrames());
}

SysStatus 
PMRoot::pushAllFreeFrames()
{
    uval addr;
    SysStatus rc = 0;
    repLock.acquire();

    while ( (addr = freeFrameList.getFrame()) ) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(addr, 
								PAGE_SIZE);
	tassertMsg((_SUCCESS(rc)), "Couldn't dealloc page?");
    }
    
    repLock.release();

    return 0;
}

SysStatus
PMRoot::attachRef()
{
    //err_printf("PMRoot::attachRef()\n");
    // don't care
    return 0;
}

SysStatus
PMRoot::detachRef()
{
    //err_printf("PMRoot::detachRef()\n");
    // don't care
    return 0;
}

SysStatus
PMRoot::destroy()
{
    passert(0,err_printf("Should never destroy PMRoot\n"));
    return 0;
}

SysStatus
PMRoot::MyRoot::print()
{
    lock.acquire();
    locked_print();
    lock.release();
    return 0;
}

SysStatus
PMRoot::getLargePageInfo(uval& largePgSize, uval& largePgReservCount,
		 uval& largePgFreeCount) {
    COGLOBAL(getLargePageInfo(largePgSize, largePgReservCount,
			      largePgFreeCount));
    return 0;
}
