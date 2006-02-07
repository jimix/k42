/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCommonMultiRep.C,v 1.15 2004/10/08 21:40:08 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared services for keeping track of regions
 * and pages.  Most FCM's are built on this
 * **************************************************************************/

#include <kernIncs.H>
#include "PageDescData.H"
#include "mem/FCMCommonMultiRep.H"
#include "mem/PerfStats.H"
#include "mem/Region.H"

template <class ROOTBASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRep<ROOTBASE,GALLOC,LALLOC>::detectModified(LocalPageDescData* pg,
				  AccessMode::mode &access,
				  AccessMode::pageFaultInfo pfinfo)
{
#if 0
    cprintf("%lx mapped, access %lx pfinfo %lx dirty %lx\n",
	    pg->paddr, access, pfinfo, pg->dirty);
#endif /* #if 0 */
    setPFBit(DistribFCM);
    tassert(!pg->isFree() && !(pg->isDoingIO()),
	    err_printf("mapping unavailable page\n"));
    // set locally requires a gather if queried
    pg->setPP(Scheduler::GetVP());

    // FIXME:  Move this off the main path.  Perhaps at the expense
    //         of doing CacheSynce with out a fault.  Eg.  move it to
    //         the notify code.
    if (COGLOBAL(mappedExecutable) && !pg->isCacheSynced()) {
	// Machine dependent operation.
	pg->unlock();
	setPFBit(CacheSynced);
	COGLOBAL(doSetCacheSynced(pg));	// FIXME: implement some sort of
	                                //        try mechanism in doOp
	                                //        which would allow us
	                                //        to try the op holding our
	                                //        local lock.
	return -1;
    }

    /********************************************************
     * In the following code:
     * The mapped bit is changed locally.  Thus global operations
     * such as page scan and give back must aggregate the local values.
     * (eg. Or all local mapped bits to determine true state)
     * Locally the free bit is not necessary.
     *
     * The dirty bit is checked and set
     * locally.  Thus it does not independently reflect the global
     * state of the page.  And again must be aggregate
     *******************************************************/

    if (!pg->isMapped()) {
	// mark page mapped
//	pg->unlock();
//	COGLOBAL(doSetMapped(pg));
//	return -1;
	setPFBit(UnMapped);
	pg->setMapped();
	// also mark framearray to indicate page is now mapped
	// PageAllocatorKernPinned::setAccessed(pg->getPAddr());
//	if (pg->free == PageDesc::SET) {
//	    pageList.dequeueFreeList(pg);
//	    pg->free = PageDesc::CLEAR;
//	}
    }


    if ((!AccessMode::isWrite(access)) || pg->isDirty()) return 0;

    if (AccessMode::isWriteFault(pfinfo)) {
	//FIXME:  For the moment in order to have dirty globally correct
        //        (so that fsync perf does not SUCK) we take the easy way
        //         out...retry fault.  However, this can be optimize
        //        with a set of atomic global bits in the master.
	pg->unlock();
	setPFBit(WriteFault);
	COGLOBAL(doSetDirty(pg));	// FIXME: Worst case implement some sort of
	                                //        try mechanism in doOp
	                                //        which would allow us
	                                //        to try the op holding our
	                                //        local lock.
	return -1;
    }

    if (AccessMode::makeReadOnly(access)) {
	setPFBit(MakeRO);
#if 0
	cprintf("              new ro access %lx\n", access);
#endif /* #if 0 */
	return 0;
    }

    // Can't reduce this access mode to read only - so assume
    // frame will be modified and map it

    pg->unlock();
    COGLOBAL(doSetDirty(pg));	// FIXME: Worst case implement some sort of
                                //        try mechanism in doOp
                                //        which would allow us
                                //        to try the op holding our
                                //        local lock.
    return -1;
}

template <class ROOTBASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRep<ROOTBASE,GALLOC,LALLOC>::mapPageInHAT(uval vaddr,
				AccessMode::pageFaultInfo pfinfo,
				AccessMode::mode access, VPNum vp,
				HATRef hat, RegionRef reg,
				LocalPageDescData *pg, uval offset,
				uval *retry)
{
    SysStatus rc;
    setPFBit(DistribFCM);
    uval wasMapped;
    uval acckey = uval(access);	// need uval for list and original access value
    *retry = 0;

    if (!COGLOBAL(mappedExecutable) && AccessMode::isExecute(access)) {
	// transition to executable  - normally there will be
	// almost nothing mapped unless the file has just been written
	passertMsg(0, "implement me\n");
//	locked_syncAll();
	COGLOBAL(mappedExecutable) = 1;
    }


    /*
     * mapped is local information for all the processors servered
     * by this rep.
     */
    wasMapped = pg->isMapped();

    // note, access may be modified by call
    if (detectModified(pg, access, pfinfo) == -1) {
	*retry = 1;
	return -1;
    }

    // we optimize the common case by not even testing for shared segment
    // before trying to map - is this a good trade?
    // notice that if noSharedSegments is true, we always map here,
    // creating a SegmentHATPrivate if necessary
    rc = DREF(hat)->mapPage(pg->getPAddr(), vaddr, pg->getLen(),
			    pfinfo, access, vp,
			    wasMapped, COGLOBAL(noSharedSegments));

    if (rc != HAT::NOSEG) {
	return rc;
    }

    if (!DREF(reg)->isSharedVaddr(vaddr)) {
	setPFBit(CreatSeg);
    	return DREF(hat)->mapPage(pg->getPAddr(), vaddr, pg->getLen(),
				  pfinfo, access, vp,
				  wasMapped, 1 /*create private segment*/);
    }

    setPFBit(MapShrSeg);

    rc = COGLOBAL(noSegMapPageInHAT(
	pg->getPAddr(), vaddr, pg->getLen(),
	pfinfo, access, vp, hat, reg, offset, acckey));
    return (rc);
}

template <class ROOTBASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRep<ROOTBASE,GALLOC,LALLOC>::printStatus(uval kind)
{
    switch (kind) {
    case PendingFaults:

	break;
    default:
	err_printf("kind %ld not known in printStatus\n", kind);
    }
    return 0;
}

template <class ROOTBASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRep<ROOTBASE,GALLOC,LALLOC>::unLockPage(uval token)
{
    LocalPageDescData *ld = (LocalPageDescData *) token;
    tassert(ld && !(ld->isDoingIO()), err_printf("unLock which is doingIO???"));

    ld->unlock();
    return 0;
}

//template instantiation
template class FCMCommonMultiRep<CObjRootMultiRep, AllocGlobalPadded,
				 AllocLocalStrict>;
template class FCMCommonMultiRep<CObjRootMultiRepPinned,
				 AllocPinnedGlobalPadded,
				 AllocPinnedLocalStrict>;

TEMPLATEDHASHTABLE(MasterPageDescData,AllocGlobalPadded,
		   LocalPageDescData,AllocLocalStrict)

TEMPLATEDHASHTABLE(MasterPageDescData,AllocPinnedGlobalPadded,
		   LocalPageDescData,AllocPinnedLocalStrict)
