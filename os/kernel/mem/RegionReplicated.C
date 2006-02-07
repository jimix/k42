/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionReplicated.C,v 1.41 2005/04/15 17:39:37 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for object invocation of Region
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/RegionReplicated.H"
#include "mem/RegionDefault.H"
#include "mem/FCM.H"
#include "mem/FRComputation.H"
#include "mem/FRPlaceHolder.H"
#include "mem/HAT.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>
#include "meta/MetaRegionReplicated.H"
#include <trace/traceMem.h>
#include <cobj/XHandleTrans.H>
#include <meta/MetaProcessServer.H>
#include <sync/MPMsgMgr.H>
#include "mem/FCMPrimitive.H"
#include "mem/FCMPartitionedTrivial.H"
#include "mem/FCMSharedTrivial.H"
#include "mem/PageList.H"
#include "bilge/TestSwitch.H"
#include "mem/PerfStats.H"


RegionReplicated::RegionReplicatedRoot::RegionReplicatedRoot()
{
}

RegionReplicated::RegionReplicatedRoot::RegionReplicatedRoot(RepRef ref,
	CObjRoot::InstallDirective idir)
    : CObjRootMultiRep(ref, 1, idir)
{
}


/* virtual */ SysStatusProcessID
RegionReplicated::getPID()
{
    return DREF(COGLOBAL(proc))->getPID();
}

CObjRep *
RegionReplicated::RegionReplicatedRoot::createRep(VPNum vp)
{
    if (regionState != RegionReplicated::NORMAL) {
	err_printf("FIXME - region rep create root in special state\n");
	breakpoint();
	while (1);
    }

    CObjRep *rep=(CObjRep *)new RegionReplicated;
    //err_printf("RegionReplicatedRoot::createRep()"
//	       ": vp=%d  New rep created rep=%p for ref=%p\n",
//	       Scheduler::GetVP(), rep, _ref);
    return rep;

}


RegionReplicated::RegionReplicated()
{
}

/*
 * unmaps the page from the process this region is attached to
 * argument is the FCM offset - which the Region can convert
 * to a virtual address.
 * See comments in RegionDefault about offset check.
 */
/* virtual */ SysStatus
RegionReplicated::unmapPage(uval offset)
{
    if (COGLOBAL(hat) &&
	((offset-COGLOBAL(fileOffset)) < COGLOBAL(regionSize))) {
	//calculate which vp this offset is "in"
	DREF(COGLOBAL(hat))->
	    unmapPage((offset-COGLOBAL(fileOffset))+COGLOBAL(regionVaddr));
    }
    return 0;
}

/* static */ SysStatus
RegionReplicated::CreateSwitchReplacement(RegionReplicatedRoot *&root,
					  RegionRef regRef)
{
    RegionReplicatedRoot *newRegionRoot = new RegionReplicatedRoot(
	    (RepRef)regRef, CObjRoot::skipInstall);
    root = newRegionRoot;

    return 0;
}

/* static */ SysStatus
RegionReplicated::CreateFixedLen(
    RegionRef& regRef, ProcessRef pRef,uval& vaddr,
    uval size, uval alignmentreq, FRRef frRef, uval writable,
    uval fileOffset, AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionReplicatedRoot *newRegionRoot = new RegionReplicatedRoot;

    retvalue = newRegionRoot->initRegion(
	pRef, vaddr, 0, size, alignmentreq,
	frRef, writable, fileOffset, accessreq, 0, regionType);
    return (retvalue);
}

/* static */ SysStatus
RegionReplicated::CreateFixedAddrLen(
    RegionRef& regRef, ProcessRef pRef, uval vaddr, uval size,
    FRRef frRef, uval writable, uval fileOffset,
    AccessMode::mode accessreq, RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionReplicatedRoot *newRegionRoot = new RegionReplicatedRoot;

    retvalue = newRegionRoot->initRegion(
	pRef, vaddr, 0, size, 0, frRef, writable, fileOffset, accessreq, 1, 
	regionType);
    return (retvalue);
}

SysStatus
RegionReplicated::RegionReplicatedRoot::initRegion(
    ProcessRef pRef, uval &vaddr, uval vaddr2, uval size,
    uval alignreq, FRRef frRef, uval writable, uval fOff,
    AccessMode::mode accessreq, uval useVaddr, RegionType::Type regionType)
{
    SysStatus rc=0;
    PMRef pmRef;
    size = PAGE_ROUND_UP(size);

    tassertWrn(!useVaddr || (PAGE_ROUND_DOWN(vaddr)==vaddr),
	       "creating an unaligned region, vaddr=%lx\n", vaddr);

    rc = DREF(pRef)->getPM(pmRef);
    if (!_SUCCESS(rc)) return rc;

    regionState = CREATING;

    /*
     * we record the actual permission of the FR.  This is so that
     * if a debugger asks to write a readonly mapping, we can decide
     * if its legal.
     * Of course, we also use this to prevent an initial writable
     * mapping of a read only FR.
     */
    writeAllowed = writable;

    rc = DREF(pRef)->getHATProcess(hat);
    tassert(_SUCCESS(rc), err_printf("process destroyed\n"));
    regionVaddr   = vaddr;
    regionSize    = size;
    proc          = pRef;
    access        = accessreq;
    alignment     = alignreq;
    fileOffset    = fOff;

    /* can make regions without an fcm - see redzone for example
     * we attach first so we can ask fcm if it uses shared segments
     */
    if (frRef) {
	rc = DREF(frRef)->attachRegion(fcm, (RegionRef)getRef(), pmRef,
				       accessreq);
	tassertWrn(_SUCCESS(rc), "attach failed\n");
	if (_FAILURE(rc)) {
	    fcm = 0;			// clear our fcm field
	    tassert(0,err_printf("attach failed\n"));
	    // FIXME we going to have a problem if we just call destroy
	    //       because that will attempt to create a
	    //       rep to call the method on, but we are currently holding
	    //       a lock preventing reps from being created
	    (*((RegionRef)getRef()))->destroy(); // destroy ourselves
	    regionState = DESTROYING;
	    return rc;
	}
    } else {
	fcm = NULL;
    }

    // If ok, attach the region to the process
    // attach newly contructed region to process
    if (useVaddr) {
	rc = DREF(pRef)->attachFixedRegion(
	    regionVaddr, regionSize, (RegionRef)getRef(), regionType);
    } else if (vaddr2 == 0) {
	// alignment fix up for shared segments
	if(size >= SEGMENT_SIZE && alignment == 0 && !useVaddr &&
	   fcm && DREF(fcm)->sharedSegments()) {
	    alignment = SEGMENT_SIZE;
	}

	rc = DREF(pRef)->attachDynamicRegion(
	    regionVaddr, regionSize, (RegionRef)getRef(), regionType, alignment);
	// return address allocated by process
	vaddr = regionVaddr;
    } else {
	rc = DREF(pRef)->attachWithinRangeRegion(
	    vaddr, vaddr2, regionSize, (RegionRef)getRef(), regionType, alignment);
	regionVaddr = vaddr;
    }

    if (!_SUCCESS(rc)) {
	// failed - delete it
	tassert(0,err_printf("Region constructor failed\n"));
	if (fcm != NULL) {
	    DREF(fcm)->detachRegion((RegionRef)getRef());
	    fcm = NULL;
	}
	// FIXME we are going to have a problem if we just call destroy
	//       unchecked because that will attempt to create a
	//       rep to call the method on, but we are currently holding
	//       a lock preventing reps from being created
	(*((RegionRef)getRef()))->destroyUnchecked(); // free ref
	regionState = DESTROYING;
	return rc;
    }
    
    // unmap any full segments so shared mappings can be used
    if ((SEGMENT_ROUND_DOWN(vaddr+size)>SEGMENT_ROUND_UP(vaddr)) &&
	fcm && DREF(fcm)->sharedSegments()) {
	rc = DREF(hat)->unmapRange(
	    SEGMENT_ROUND_UP(vaddr),
	    SEGMENT_ROUND_DOWN(vaddr+size)- SEGMENT_ROUND_UP(vaddr),
	    ppset);
	tassert(_SUCCESS(rc), err_printf("oops\n"));
    }
    regionState = NORMAL;
    return rc;
}




#include "defines/MLSStatistics.H"

/* virtual */ SysStatusUval
RegionReplicated::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
			      PageFaultNotification *pn, VPNum vp)
{
    SysStatusUval rc;
    ScopeTime timer(RegionHandlerTimer);
    MLSStatistics::StartTimer(3);
    rc =  RegionReplicated::handleFaultInternal(
	pfinfo, vaddr, pn, vp, COGLOBAL(fileOffset));
    MLSStatistics::DoneTimer(3);
    return rc;
}

#if 0
static uval stopAddr = (uval)0x2b88c;
#endif /* #if 0 */

SysStatusUval
RegionReplicated::handleFaultInternal(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp, uval fileOffset)
{
    SysStatus rc;
    uval user;
    uval firstAccessOnPP;
    uval myvp = Scheduler::GetVP();

    if (requests.enter() < 0) {
	//FIXME return correct error for can't map page
	//N.B. when enter fails, DO NOT leave()
	return _SERROR(1402, 0, EFAULT);
    }

#if 0
    err_printf("enter %lx %ld\n",vaddr, vp);
    if (vaddr == stopAddr) {
	breakpoint();
    }
#endif /* #if 0 */

    rc = DREF(COGLOBAL(proc))->getUserMode(user);
    if (rc) goto leave;

    if ((vaddr < COGLOBAL(regionVaddr)) ||
	(vaddr >= (COGLOBAL(regionVaddr) + COGLOBAL(regionSize))) ||
	!AccessMode::verifyAccess(user, COGLOBAL(access), pfinfo)) {
	if ((vaddr < COGLOBAL(regionVaddr)) ||
	    (vaddr >= (COGLOBAL(regionVaddr) + COGLOBAL(regionSize)))) {
	    rc = _SERROR(1403, 0, EFAULT);
	} else {
	    rc = _SERROR(1404, 0, EACCES);
	}
	goto leave;
    }

    firstAccessOnPP = 0;
    // atomically set bit if not already set
    if (!(COGLOBAL(ppset)).isSet(myvp)) {
	(COGLOBAL(ppset)).atomicAddVP(myvp);
	firstAccessOnPP = 1;
    }

    vaddr = vaddr&(-PAGE_SIZE);		// convert to page address
    rc = DREF(COGLOBAL(fcm))->mapPage(fileOffset, COGLOBAL(regionVaddr),
			    COGLOBAL(regionSize), pfinfo, vaddr,
			    COGLOBAL(access), COGLOBAL(hat), vp, getRef(),
			    firstAccessOnPP, pn);
    tassert(_SUCCESS(rc),
	    err_printf("Can't allocate a page frame in RegionReplicated\n"));

    //N.B. all exits must do a leave - even errors
 leave:
    requests.leave();
#if 0
    err_printf("leave %lx %ld\n",vaddr, vp);
#endif /* #if 0 */

    return (rc);
}

SysStatus
RegionReplicated::unmapRange(uval start, uval size)
{
    SysStatus rc;

    //err_printf("RegionReplicated::unmapRange: %lx/%lx, ppset %lx\n",
    //       start, size, uval(ppset));

    if (start < COGLOBAL(regionVaddr)) {
	size -= (COGLOBAL(regionVaddr)-start);
	start = COGLOBAL(regionVaddr);
    }

    if (size > COGLOBAL(regionSize)) {
	size = COGLOBAL(regionSize);
    }

    rc = DREF(COGLOBAL(hat))->unmapRange(start, size, COGLOBAL(ppset));
    //err_printf("RegionReplicated::unmapRange all done\n");

    return rc;
}

/*virtual*/ SysStatus
RegionReplicated::vaddrToFCM(VPNum vpNum, uval vaddr, uval writeAccess,
			     FCMRef& fcmRef, uval& offset)
{
    (void) vpNum; // only needed in perprocessor regions
    SysStatus rc;
    uval user;
    AccessMode::pageFaultInfo pfinfo;

    rc = DREF(COGLOBAL(proc))->getUserMode(user);
    if (_FAILURE(rc)) return rc;

    // fabricate pfinfo
    pfinfo = writeAccess?AccessMode::writeFault:AccessMode::readFault;

    if ((vaddr < COGLOBAL(regionVaddr)) ||
	(vaddr >= (COGLOBAL(regionVaddr) + COGLOBAL(regionSize))) ||
	!AccessMode::verifyAccess(user, COGLOBAL(access), pfinfo)) {
	if ((vaddr < COGLOBAL(regionVaddr)) ||
	    (vaddr >= (COGLOBAL(regionVaddr) + COGLOBAL(regionSize)))) {
	    rc = _SERROR(2080, 0, EFAULT);
	} else {
	    rc = _SERROR(2081, 0, EACCES);
	}
	return rc;
    }

    offset = COGLOBAL(fileOffset) + (vaddr&(-PAGE_SIZE))-COGLOBAL(regionVaddr);
    fcmRef = COGLOBAL(fcm);
    return 0;
}

SysStatus
RegionReplicated::RegionReplicatedRoot::startDestroyAll()
{
    void *curr;
    RegionReplicated *rep = 0;

    lockReps();
    if (regionState == RegionReplicated::DESTROYING) {
	// FIXME return real code
	return 1;
    }
    regionState = RegionReplicated::DESTROYING;
    destroyCount = 0;

    for (curr = nextRep(0, (CObjRep *&)rep); curr;
	 curr = nextRep(curr, (CObjRep *&)rep)) {

	// FIXME make fetch_inc
	FetchAndAddSignedSynced(&destroyCount, 1);

	// FIXME make asynchronous
	rep->destroyLocal();
    }
    unlockReps();
    return 0;
}


SysStatus
RegionReplicated::destroyLocal()
{
    if (requests.shutdown() != 0) {
	passert(0,
		err_printf("Help somebody called us and they shouldn't have\n"));
    }
    FetchAndAddSignedSynced(&(COGLOBAL(destroyCount)), -1);
    return 0;
}


/*
 * no locking.  The worst that can happen is that the FCM
 * is already deleted when the caller tries to use it,
 * and we can't prevent that in any case.
 */
SysStatus
RegionReplicated::destroy()
{
    SysStatus rc;
    //err_printf("Region %p, destroying\n", getRef());

    rc = COGLOBAL(startDestroyAll());
    // FIXME see above
    if (rc == 1) return 0;

    {   // remove all ObjRefs to this object
	SysStatus rc2 = closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc2)) {
	    return _SCLSCD(rc2)==1?0:rc2;
	}
    }

    //err_printf("Region %p, unmaprange a %lx, s %lx\n", getRef(),
    //       COGLOBAL(regionVaddr), COGLOBAL(regionSize));
    unmapRange(COGLOBAL(regionVaddr), COGLOBAL(regionSize));

    // now detach from the FCM
    if (COGLOBAL(fcm) != NULL) {
	//err_printf("Region %p detaching from fcm %p\n", getRef(), COGLOBAL(fcm));
	DREF(COGLOBAL(fcm))->detachRegion(getRef());
    }

    // detach from Process - do this last.  Until this point, no new
    // region can reuse the virtual addresses involved.
    // because all outstanding requests are done, no new mappings will happen
    if (COGLOBAL(proc) != NULL) {
	//err_printf("Region %p detaching from proc %p\n", getRef(), proc);
	DREF(COGLOBAL(proc))->detachRegion(getRef());
    }

    //N.B. don't set proc or fcm to null - there may be other calls in flight.
    //they will stay around, returning error, long enough to cover this
    //possibility.

    destroyUnchecked();

    return (0);
}

/* static */ SysStatus
RegionReplicated::_CreateFixedLenExt(uval& regionVaddr, uval regionSize,
				     uval alignmentreq, ObjectHandle frOH,
				     uval fileOffset, uval accessreq,
				     XHandle target,
				     RegionType::Type regionType,
				     __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FRRef frRef;
    uval writable;

    rc = RegionDefault::CheckCaller(target, callerPID, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);
    _IF_FAILURE_RET(rc);

    rc = RegionReplicated::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, writable, fileOffset,
	(AccessMode::mode)accessreq, regionType);

    return rc;
}

#if 0
/* static */ SysStatus
RegionReplicated::_CreateFixedLenExtDyn(uval& regionVaddr, uval regionSize,
					uval alignmentreq, ObjectHandle frOH,
					uval fileOffset, uval accessreq,
					XHandle target, ObjectHandle tsOH,
					RegionType::Type regionType,
					__CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FCMRef fcmRef;
    TypeID type;
    ObjRef objRef;
    FRRef frRef;

    rc = RegionDefault::PrefFromTarget(target, callerPID, pref);
    _IF_FAILURE_RET(rc);

    rc = XHandleTrans::XHToInternal(frOH.xhandle(), callerPID,
				    MetaObj::attach, objRef, type);
    tassertWrn(_SUCCESS(rc), "RegionReplicated failed XHToInternal\n");
    _IF_FAILURE_RET(rc);

    // verify that type is of FRComp - we'd really like just FR
// FIXMEXX talk to Orran, the brilliant, charming, ..
    if (!MetaFR::isBaseOf(type)) {
	tassertWrn(0, "object handle <%lx,%lx> not of correct type\n",
		   frOH.commID(), frOH.xhandle());
	return _SERROR(1542, 0, EINVAL);
    }

    //FIXME check xobj to make sure it matches our processID

    frRef = (FRRef)objRef;

    rc = RegionReplicated::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, fileOffset,
	(AccessMode::mode)accessreq);

    // FIXME: Kludge for enabling dyn-switch
#if 0
    CObjRoot *partitionedFCMRoot = 0;
    rc = FCMPartitionedTrivial::Create(partitionedFCMRoot, fcmRef, regionSize,
				       /* number of cache lines per rep */ 256,
				       /* associativity of each line */    4);
    tassert(_SUCCESS(rc), err_printf("Replicated Trivial fcm create\n"));

    PMRef pmRef;
    rc = DREF(pref)->getPM(pmRef);
    tassert(_SUCCESS(rc), ;);
    ((FCMPartitionedTrivial::FCMPartitionedTrivialRoot *)partitionedFCMRoot)->
	attachRegion(ref, pmRef);
#endif /* #if 0 */

    SysStatus rc2 = XHandleTrans::XHToInternal(tsOH.xhandle(),
					       callerPID, MetaObj::attach,
					       objRef, type);
#if 0
    tassertWrn(_SUCCESS(rc2), "XHToInternal failed\n");
#else /* #if 0 */
    rc2 = rc2; // FIXME: do something about the tassertWrn
#endif /* #if 0 */
    TestSwitchRef tsRef = (TestSwitchRef)objRef;

    DREF(tsRef)->storeRefs(fcmRef, regionSize);

    // FIXME: end kludge

    return rc;
}
#else /* #if 0 */
/* static */ SysStatus
RegionReplicated::_CreateFixedLenExtKludgeDyn(uval& regionVaddr,
					      uval regionSize,
					      uval alignmentreq,
					      ObjectHandle /*frOH*/,
					      uval fileOffset, uval accessreq,
					      XHandle target, ObjectHandle tsOH,
					      RegionType::Type regionType,
					      __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FCMRef fcmRef;
    FRRef frRef;
    TypeID type;
    ObjRef objRef;

    rc = RegionDefault::PrefFromTarget(target, callerPID, pref);
    _IF_FAILURE_RET(rc)

    // FIXME: OK heres my big Gross Kludge
    rc = FCMSharedTrivial<PageList<AllocGlobal>,AllocGlobal>::Create(fcmRef);
    tassert(_SUCCESS(rc), err_printf("Shared Trivial fcm create\n"));

    rc = FRPlaceHolder::Create(frRef);
    _IF_FAILURE_RET(rc);

    DREF(frRef)->installFCM(fcmRef);

    rc = RegionReplicated::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, 0/*fixme*/, fileOffset,
	(AccessMode::mode)accessreq, regionType);

    // Kludge for enabling dyn-switch
    SysStatus rc2 = XHandleTrans::XHToInternal(tsOH.xhandle(),
					       callerPID, MetaObj::attach,
					       objRef, type);
#if 0
    tassertWrn(_SUCCESS(rc2), "XHToInternal failed\n");
#else /* #if 0 */
    rc2 = rc2; // FIXME: do something about the tassertWrn
#endif /* #if 0 */
    TestSwitchRef tsRef = (TestSwitchRef)objRef;

    DREF(tsRef)->storeRefs(fcmRef, 0, pref, regionSize);
    passert(0, err_printf("\nCall the RegionDefault version: "
			  "let's test region switching also!\n"));

    // end kludge

    return rc;
}
#endif /* #if 0 */

/* static */ SysStatus
RegionReplicated::_CreateFixedLenExtKludge(
    uval& regionVaddr, uval regionSize,
    uval alignmentreq, uval fileOffset,
    uval accessreq, XHandle target,
    uval partitioned, 
    RegionType::Type regionType,
    __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FRRef frRef;
    FCMRef fcmRef;

    rc = RegionDefault::PrefFromTarget(target, callerPID, pref);
    _IF_FAILURE_RET(rc)

    // FIXME: OK heres my big Gross Kludge
#if 1
    if (partitioned) {
	rc = FCMPartitionedTrivial::Create(fcmRef,
					   regionSize,
				    /* number of cache lines per rep */ 256,
				    /* associativity of each line */    4);
    } else {
	rc = FCMSharedTrivial<PageList<AllocGlobal>,AllocGlobal>::
	    Create(fcmRef);
    }
#else /* #if 1 */
    rc = FCMPartitionedTrivial::Create(fcmRef, regionSize,
			       /* number of cache lines per rep */ 256,
			       /* associativity of each line */    4);
#endif /* #if 1 */
    tassert(_SUCCESS(rc), err_printf("Partitioned Trivial fcm create\n"));

    rc = FRPlaceHolder::Create(frRef);
    _IF_FAILURE_RET(rc);

    DREF(frRef)->installFCM(fcmRef);

    //FIXME where does writable come from?
    rc = RegionReplicated::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, 0/*fixme*/, fileOffset,
	(AccessMode::mode)accessreq, regionType);

    return rc;
}

/* static */ SysStatus
RegionReplicated::_CreateFixedAddrLenExt(uval regionVaddr, uval regionSize,
					 ObjectHandle frOH, uval fileOffset,
					 uval accessreq, XHandle target,
					 RegionType::Type regionType,
					 __CALLER_PID callerPID)
{
    RegionRef regRef;
    ProcessRef pref;
    SysStatus rc;
    FRRef frRef;
    uval writable;

    rc = RegionDefault::CheckCaller(target, callerPID, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);
    _IF_FAILURE_RET(rc);

    return (RegionReplicated::CreateFixedAddrLen(
	regRef, pref, regionVaddr, regionSize,
	frRef, writable, fileOffset, (AccessMode::mode)accessreq, regionType));
}

void
RegionReplicated::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaRegionReplicated::init();
}

SysStatus
RegionReplicated::forkCopy(FRRef &newFRRef)
{
    SysStatus rc;
    // lock out all requests, including destroy, until
    // the copy is finished.
    // requests in progress will finish before stop returns.
    rc = requests.stop();
    if (rc < 0) return _SERROR(1408, 0, EBUSY);
    rc = DREF(COGLOBAL(fcm))->forkCopy(newFRRef);
    requests.restart();
    return rc;
}

SysStatus
RegionReplicated::forkCloneRegion(ProcessRef pref, RegionType::Type regionType)
{
    FRRef newFRRef;
    RegionRef ref;
    SysStatus rc;
    rc = requests.stop();
    if (rc < 0) return _SERROR(1408, 0, EBUSY);
    rc = DREF(COGLOBAL(fcm))->forkCloneFCM(newFRRef, regionType);
    requests.restart();
    if (_FAILURE(rc)) return rc;
    return CreateFixedAddrLen(
	ref, pref, COGLOBAL(regionVaddr),
	COGLOBAL(regionSize), newFRRef,
	COGLOBAL(writeAllowed),
	COGLOBAL(fileOffset),
	COGLOBAL(access), regionType);
}

