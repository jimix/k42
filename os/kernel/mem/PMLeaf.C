/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMLeaf.C,v 1.34 2005/03/02 05:27:57 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Leaf PMs (generally for a given Process); has no
 * children PMs, only FCMs.
 * **************************************************************************/

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include "defines/paging.H"
#include "mem/PMLeaf.H"
#include "mem/FCM.H"
#include "mem/PageAllocatorKernPinned.H"
#include <cobj/DTType.H>

DECLARE_FACTORY_STATICS(PMLeaf::Factory);

/* virtual */ SysStatus
PMLeaf::Factory::create(PMRef &pmref, PMRef parentPM)
{
    //err_printf("Creating PMLeaf\n");
    PMLeaf *pm;
    pm = new PMLeaf();
    tassert(pm!=NULL, err_printf("No mem for PMLeaf\n"));
    PMLeafRoot *root = new PMLeafRoot(pm);
    pmref = (PMRef) root->getRef();
    //err_printf("PMLeaf %lx created with parent %lx\n", pmref, parentPM);
    pm->parentPM = parentPM;
    DREF(parentPM)->attachPM(pmref);
    registerInstance((CORef)pmref);
    return 0;
}

/* virtual */ SysStatus
PMLeaf::Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    err_printf("PMLeaf::replace: BEGIN\n");
    PMLeaf *rep = new PMLeaf;
    root = new PMLeafRoot(rep, (RepRef)ref, CObjRoot::skipInstall);
    tassertMsg(ref == (CORef)root->getRef(), "Opps ref=%p != root->getRef=%p\n",
               ref, root->getRef());
    registerInstance((CORef)root->getRef());
    return 0;
}

SysStatus
PMLeaf::attachFCM(FCMRef fcm)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    refCount++;
    numFCM++;
    tassert(!FCMList.find(fcm), err_printf("re-attach\n"));
    FCMList.add(fcm);
    return 0;
}

SysStatus
PMLeaf::detachFCM(FCMRef fcm)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    //err_printf("PM %lx detaching fcm %lx\n", getRef(), fcm);

    tassert(FCMList.find(fcm), err_printf("detach null\n"));
    FCMList.remove(fcm);

    refCount--;
    numFCM--;
    if (refCount == 0) {
	return destroy();
    }
    //locked_print();
    return 0;
}


/*
 * No support for PMs under PMleafs at this point
 */

SysStatus
PMLeaf::attachPM(PMRef pm)
{
    passert(0,err_printf("unsupported\n"));
    return -1;
}

SysStatus
PMLeaf::detachPM(PMRef pm)
{
    passert(0,err_printf("unsupported\n"));
    return -1;
}

SysStatus
PMLeaf::allocPages(PMRef pm, uval &ptr, uval size, uval pageable, uval flags, 
		   VPNum node)
{
    passert(0,err_printf("unsupported\n"));
    return -1;
}

SysStatus
PMLeaf::allocListOfPages(PMRef pm, uval count, FreeFrameList *ffl)
{
    passert(0,err_printf("unsupported\n"));
    return -1;
}    

SysStatus
PMLeaf::deallocPages(PMRef pm, uval paddr, uval size)
{
    passert(0,err_printf("unsupported\n"));
    return -1;
}

SysStatus
PMLeaf::deallocListOfPages(PMRef pm, FreeFrameList *ffl)
{
    passert(0,err_printf("unsupported\n"));
    return -1;
}

SysStatus
PMLeaf::allocPages(FCMRef fcm, uval &ptr, uval size, uval pageable, uval flags, 
		   VPNum node)
{
    SysStatus rc;
    rc = DREF(parentPM)->allocPages(getRef(), ptr, size, pageable, flags, node);
    return rc;
}

SysStatus
PMLeaf::allocListOfPages(FCMRef fcm, uval count, FreeFrameList *ffl)
{
    SysStatus rc;
    rc = DREF(parentPM)->allocListOfPages(getRef(), count, ffl);
    return rc;
    
}

SysStatus
PMLeaf::deallocPages(FCMRef fcm, uval paddr, uval size)
{
    SysStatus rc;
    rc = DREF(parentPM)->deallocPages(getRef(), paddr, size);
    tassert(_SUCCESS(rc),
	    err_printf("deallocPages failed %lx %lx\n", paddr, rc));
    return rc;
}

SysStatus
PMLeaf::deallocListOfPages(FCMRef fcm, FreeFrameList *ffl)
{
    SysStatus rc;
    rc=DREF(parentPM)->deallocListOfPages(getRef(), ffl);
    return rc;
}

SysStatus
PMLeaf::attachRef()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    //err_printf("PMLeaf::attachRef()\n"); locked_print();
    // increment reference count
    refCount++;
    return 0;
}

SysStatus
PMLeaf::detachRef()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    //err_printf("PMLeaf::detachRef()\n"); locked_print();
    // decrement reference count and destroy if zero
    refCount--;
    if (refCount == 0) {
	return destroy();
    }
    return 0;
}

void
PMLeaf::scanFCMs(PM::MemLevelState memLevelState)
{
    const uval maxListSize = 20;
    uval numScan, i;
    FCMRef fcmList[maxListSize];
    FCMRef   fcm;
    void    *iter;
    SysStatus rc;

    lock.acquire();

    if (numFCM == 0) {
	lock.release();
	return;
    }

    // collect a list of FCMs to scan; we make copy since we can't make
    // fcm call with lock held

    // ugly hack; we iterate until we find the last one we scanned, and
    // then continue from there
    iter = NULL;
    if (lastScanned != NULL) {
	while ((iter = FCMList.next(iter, fcm)) != NULL) {
	    if (fcm == lastScanned) break;
	}
    }

    numScan = numFCM / 4;
    if (numFCM > 0 && numFCM <= 4) numScan = 1;
    // temp hack - logic to be completely reworked
    if (numScan > maxListSize) numScan = maxListSize;
    for (i = 0; i < numScan; i++ ) {
	iter = FCMList.next(iter, fcm);
	if (iter == NULL) {
	    // at end, just wrap around to beginning
	    iter = FCMList.next(iter, fcm);
	    tassert(iter != NULL, err_printf("oops\n"));
	}
	fcmList[i] = fcm;
    }
    lastScanned = fcm;

    lock.release();

    for (i = 0; i < numScan; i++ ) {
	rc = DREF(fcmList[i])->giveBack(memLevelState);
    }
}

SysStatus
PMLeaf::giveBack(PM::MemLevelState memLevelState)
{
    if (!nonPageable) {
	scanFCMs(memLevelState);
    }
    return 0;
}

/* virtual */SysStatus 
PMLeafRoot::deRegisterFromFactory()
{
    return DREF_FACTORY_DEFAULT(PMLeaf)->
        deregisterInstance((CORef)getRef());
}

SysStatus
PMLeaf::destroy()
{
    SysStatus rc;
    //err_printf("PMLeaf::destroy for %lx\n", getRef());

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    tassert(FCMList.isEmpty(), err_printf("detach non-empty fcmlist\n"));
    //tassert(summary.total == 0, err_printf("destroy, total non-zero\n"));
    //tassert(summary.free == 0, err_printf("destroy, free !=0\n"));
    tassert(refCount == 0, err_printf("destroy, refcount non-zero\n"));
    tassert(numFCM == 0, err_printf("destroy, numFCM non-zero\n"));

    rc = DREF(parentPM)->detachPM(getRef());
    tassert(_SUCCESS(rc), err_printf("PM detach failed\n"));
    parentPM = uninitPM();

    ((PMLeafRoot *)myRoot)->deRegisterFromFactory();
    tassertMsg(_SUCCESS(rc), "failure to deregister from factory "
               "rc=%ld ref=%p\n", rc, getRef());

    destroyUnchecked();
    return 0;
}

void
PMLeaf::locked_print()
{
    FCMRef   fcm;
    void    *iter;
    Summary fcmSummary;
    uval fcmInUse = 0;

    _ASSERT_HELD(lock);

    err_printf("PMLeaf %p; parent %p, refc %ld; \n", getRef(), parentPM,
	       refCount);
    iter = NULL;
    while ((iter = FCMList.next(iter, fcm)) != NULL) {
	err_printf("\tfcm %p: ", fcm);
	// special case, this funciton aquires no locks, so no deadlock
	DREF(fcm)->getSummary(fcmSummary);
	err_printf(" total %ld\n", fcmSummary.total);
	fcmInUse += fcmSummary.total;
    }
    err_printf("fcmInUse %ld\n", fcmInUse);
}

SysStatus
PMLeaf::print()
{
    lock.acquire();
    locked_print();
    lock.release();
    return 0;
}


/* virtual */ DataTransferObject *
PMLeafRoot::dataTransferExport(DTType dtt, VPSet dtVPSet) {
    passertMsg(dtt == DTT_TEST, "unknown DDT\n");
    err_printf("sending this=%p as DataTransferObject therep=%p\n",
               this, therep);
    return (DataTransferObject *)this;
}

/* virtual */ SysStatus 
PMLeafRoot::dataTranferImport(DataTransferObject *data, DTType,
                              VPSet dtVPSet) {
    passertMsg(0, "NYI");
    return -1;
}
