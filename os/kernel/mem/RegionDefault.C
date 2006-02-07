/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionDefault.C,v 1.158 2005/01/08 17:52:05 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for object invocation of Region
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/RegionDefault.H"
#include "mem/FCM.H"
#include "mem/FCMStartup.H"
#include "mem/FRComputation.H"
#include "mem/FRPlaceHolder.H"
#include "mem/HAT.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>
#include "meta/MetaRegionDefault.H"
#include <trace/traceMem.h>
#include <cobj/XHandleTrans.H>
#include <meta/MetaProcessServer.H>
#include <cobj/CObjRootSingleRep.H>
#include <sync/MPMsgMgr.H>
#include "mem/FCMPrimitive.H"
#include "mem/FCMPartitionedTrivial.H"
#include "mem/PageList.H"
#include "mem/FCMSharedTrivial.H"
#include "bilge/TestSwitch.H"
#include "init/SysFacts.H"
#include "mem/PerfStats.H"
/*
 * unmaps the page from the process this region is attached to
 * argument is the FCM offset - which the Region can convert
 * to a virtual address
 * Note that offset may not be in this fcm - we must check.
 * Unmaps driven by paging can cause this when a region maps
 * only part of an fcm.
 * Because of the way unsigned arithmetic works, the one
 * check below gets offsets that are either too small or too big.
 */
/* virtual */ SysStatus
RegionDefault::unmapPage(uval offset)
{
    if (hat && ((offset-fileOffset) < regionSize)) {
	DREF(hat)->unmapPage((offset-fileOffset)+regionVaddr);
    }
    return 0;
}

/* virtual */ SysStatusProcessID
RegionDefault::getPID()
{
    return DREF(proc)->getPID();
}


SysStatus
RegionDefault::initRegion(ProcessRef pRef, uval &vaddr, uval vaddr2, uval size,
			  uval alignreq, FRRef frRef, uval writable, uval fOff,
			  AccessMode::mode accessreq, uval useVaddr, 
			  RegionType::Type regionType, uval skipProcessAttach)
{
    SysStatus rc=0;
    PMRef pmRef;
    pageSize = PAGE_SIZE;
    size = PAGE_ROUND_UP(size);
    uval largePageRegion = 0;

    tassertWrn(!useVaddr || (PAGE_ROUND_DOWN(vaddr)==vaddr),
	       "creating an unaligned region, vaddr=%lx\n", vaddr);

    rc = DREF(pRef)->getPM(pmRef);
    if (!_SUCCESS(rc)) return rc;

    // we pseudo-lock it to handle callbacks/destruction during construction
    if (requests.stop() < 0) {
	tassert(0, err_printf("Region Destroyed during create\n"));
    }

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
    attachSize    = size;
    proc          = pRef;
    access        = accessreq;
    alignment     = alignreq;
    fileOffset    = fOff;

    /* can make regions without an fcm - see redzone for example
     * we attach first so we can ask fcm if it uses shared segments
     */
    if (frRef) {
	rc = DREF(frRef)->attachRegion(fcm, getRef(), pmRef, accessreq);
	tassertWrn(_SUCCESS(rc), "attach failed\n");
	if (_FAILURE(rc)) {
	    fcm = 0;			// clear our fcm field
	    requests.restart();		// release region
	    destroy();			// destroy ourselves
	    return rc;
	}
	pageSize = _SGETUVAL(DREF(frRef)->getPageSize());
    } else {
	fcm = NULL;
    }

    // FIXME maa 1/28/2004
    // PowerPC large page mess - we can only support one page size
    // per segment.  So we insist that the region start on a
    // segment boundary and round up the size to segment size
    // This is terrible - but gets us up and running
    if (pageSize != PAGE_SIZE) {
	attachSize = SEGMENT_ROUND_UP(regionSize);
	alignment = SEGMENT_SIZE;
	if(useVaddr) {
	    if(SEGMENT_ROUND_DOWN(regionVaddr) != regionVaddr) {
		return _SERROR(2795, 0, EINVAL);
	    }
	}
        largePageRegion = 1;
    }

    // If ok, attach the region to the process
    // attach newly contructed region to process
    if (!skipProcessAttach) {
        if (useVaddr) {
            rc = DREF(pRef)->attachFixedRegion(regionVaddr, attachSize,
                                               getRef(), regionType);
        } else if (vaddr2 == 0) {
            // alignment fix up for shared segments
            if(size >= SEGMENT_SIZE && alignment == 0 &&
               fcm && DREF(fcm)->sharedSegments()) {
                alignment = SEGMENT_SIZE;
            }

            rc = DREF(pRef)->attachDynamicRegion(regionVaddr, attachSize,
					     getRef(), regionType, alignment);
            // return address allocated by process
            vaddr = regionVaddr;
        } else {
            rc = DREF(pRef)->attachWithinRangeRegion(vaddr, vaddr2,
						     attachSize,
                                                     getRef(),
                                                     regionType, alignment);
            regionVaddr = vaddr;
        }
    }

    if (!_SUCCESS(rc)) {
        // failed - delete it
        tassertWrn(0,"Region constructor failed\n");
        // now detach from the FCM
        if (fcm != NULL) {
            //err_printf("Region %p detaching from fcm %p\n", getRef(), fcm);
            DREF(fcm)->detachRegion(getRef());
        }
        
	requests.restart();		// release region
	// condensed destroy because not fully initialized
	closeExportedXObjectList();
	destroyUnchecked();		// free ref
	return rc;
    }

    // unmap any full segments so shared mappings can be used
    if (!skipProcessAttach && 
        (SEGMENT_ROUND_DOWN(vaddr+size)>SEGMENT_ROUND_UP(vaddr)) &&
	fcm && DREF(fcm)->sharedSegments()) {
	rc = DREF(hat)->unmapRange(
	    SEGMENT_ROUND_UP(vaddr),
	    SEGMENT_ROUND_DOWN(vaddr+size)-SEGMENT_ROUND_UP(vaddr),
	    ppset);
	tassert(_SUCCESS(rc), err_printf("oops\n"));
    }

    if (largePageRegion) {
        // if we are using large pages un map any segments that might
        // be kicking around... being lazy .. rather than looking
        // for compatablity simply have the segments destroy themselves
        // though unmapping.  We ignore return codes as there may not
        // be segements present.
        (void) DREF(hat)->unmapRange(regionVaddr, attachSize, ppset);
    }

    requests.restart();			// release region

    TraceOSMemRegDefInitFixed(uval(this), regionVaddr, regionSize);

    return rc;
}

/* static */ SysStatus
RegionDefault::CreateFixedLen(
    RegionRef& regRef, ProcessRef pRef, uval& vaddr,
    uval size, uval alignmentreq, FRRef frRef, uval writable,
    uval fileOffset, AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionDefault* reg = new RegionDefault;
    TraceOSMemRegCreateFix(vaddr, size);
    regRef = (RegionRef)CObjRootSingleRep::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr, 0, size, alignmentreq, frRef, 
			       writable, fileOffset, accessreq, 0, regionType);
    return (retvalue);
}

#ifdef PROTOTYPE_SUBLIST
/* static */ SysStatus
RegionDefault::CreateFixedLenSubRegion(
    RegionRef& regRef, ProcessRef pRef, uval& vaddr,
    uval size, uval alignmentreq, FRRef frRef, uval writable,
    uval fileOffset, AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionDefault* reg = new RegionDefault;
    TraceOSMemRegCreateFix(vaddr, size);
    regRef = (RegionRef)CObjRootSingleRep::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr, 0, size, alignmentreq, frRef, 
			       writable, fileOffset, accessreq, 
                               1, /* useVaddr */
                               regionType,
                               0, /* skipProcessAttach */
                               1  /* useSubRegion */);
    return (retvalue);
}
#endif

/* static */ SysStatus
RegionDefaultKernel::CreateFixedLen(
    RegionRef& regRef, ProcessRef pRef,
    uval& vaddr, uval size, uval alignmentreq,
    FRRef frRef, uval writable, uval fileOffset,
    AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionDefaultKernel* reg = new RegionDefaultKernel;
    regRef = (RegionRef)CObjRootSingleRepPinned::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr, 0, size, alignmentreq,
			       frRef, writable, fileOffset, accessreq, 0, 
			       regionType);
    return (retvalue);
}


/* static */ SysStatus
RegionDefault::CreateFixedLenWithinRange(
    RegionRef& regRef, ProcessRef pRef,
    uval& vaddr1, uval vaddr2, uval size, uval alignmentreq,
    FRRef frRef, uval writable, uval fileOffset,
    AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionDefault* reg = new RegionDefault;
    regRef = (RegionRef)CObjRootSingleRep::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr1, vaddr2, size, alignmentreq,
			       frRef, writable, fileOffset, accessreq, 0, regionType);
    return (retvalue);
}

/* static */ SysStatus
RegionDefaultKernel::CreateFixedLenWithinRange(
    RegionRef& regRef, ProcessRef pRef,
    uval& vaddr1, uval vaddr2, uval size, uval alignmentreq,
    FRRef frRef, uval writable, uval fileOffset,
    AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    RegionDefaultKernel* reg = new RegionDefaultKernel;
    regRef = (RegionRef)CObjRootSingleRepPinned::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr1, vaddr2, size, alignmentreq,
			       frRef, writable, fileOffset, accessreq, 0, regionType);
    return (retvalue);
}


/* static */ SysStatus
RegionDefault::CreateFixedAddrLen(RegionRef& regRef, ProcessRef pRef,
				  uval vaddr, uval size,
				  FRRef frRef, uval writable, uval fileOffset,
				  AccessMode::mode accessreq,
				  RegionType::Type regionType,
				  RegionRef use_this_ref)
{
    SysStatus retvalue;
    TraceOSMemRegCreateFix(vaddr, size);
    RegionDefault* reg = new RegionDefault;
    regRef = (RegionRef)CObjRootSingleRep::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr, 0, size, 0, frRef, writable,
			       fileOffset, accessreq, 1, regionType);
    return (retvalue);
}

/* static */ SysStatus
RegionDefaultKernel::CreateFixedAddrLen(
    RegionRef& regRef, ProcessRef pRef, uval vaddr, uval size,
    FRRef frRef, uval writable,
    uval fileOffset, AccessMode::mode accessreq, 
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    SysStatus retvalue;
    TraceOSMemRegCreateFix(vaddr, size);
    RegionDefaultKernel* reg = new RegionDefaultKernel;
    regRef = (RegionRef)CObjRootSingleRepPinned::Create(reg, use_this_ref);
    retvalue = reg->initRegion(pRef, vaddr, 0, size, 0, frRef, writable,
			       fileOffset, accessreq, 1, regionType);
    return (retvalue);
}



#include "defines/MLSStatistics.H"
/* virtual */ SysStatusUval
RegionDefault::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
			   PageFaultNotification *pn, VPNum vp)
{
    SysStatusUval rc;

    MLSStatistics::StartTimer(3);
    rc =  RegionDefault::handleFaultInternal(
	pfinfo, vaddr, pn, vp, fileOffset);
    MLSStatistics::DoneTimer(3);
    return rc;
}

#ifdef STOPADDR
uval stopAddr=0x10050000000;
#endif

SysStatusUval
RegionDefault::handleFaultInternal(AccessMode::pageFaultInfo pfinfo,
				   uval vaddr, PageFaultNotification *pn,
				   VPNum vp, uval fileOffset)
{
    SysStatus rc;
    uval user;
    uval firstAccessOnPP;
    uval mypp = Scheduler::GetVP();

    if (requests.enter() < 0) {
	//FIXME return correct error for can't map page
	//N.B. when enter fails, DO NOT leave()
	return _SERROR(1214, 0, EFAULT);
    }

#ifdef STOPADDR
    if ((vaddr&(-PAGE_SIZE)) == stopAddr) {
	err_printf("this %p\n", this);
    }
#endif /* #if 0 */

    rc = DREF(proc)->getUserMode(user);
    if (rc) goto leave;

    if ((vaddr < regionVaddr) ||
	(vaddr >= (regionVaddr + regionSize)) ||
	!AccessMode::verifyAccess(user, access, pfinfo)) {
	if ((vaddr < regionVaddr) ||
	    (vaddr >= (regionVaddr + regionSize))) {
	    if ((vaddr < regionVaddr) ||
		(vaddr >= (regionVaddr + attachSize))) {
		/*
		 * out of the address range this region is supposed
		 * to cover - caller needs to retry since regionList
		 * is being changed to cause this.
		 */
		rc = _SERROR(1506, AddressOutOfRange, EFAULT);
	    } else {
		/*
		 * in range covered but out of range supported
		 */
		rc = _SERROR(1506, 0, EFAULT);
	    }
	} else {
	    rc = _SERROR(1102, 0, EACCES);
	}
	goto leave;
    }

    firstAccessOnPP = 0;
    // atomically set bit if not already set
    if (!ppset.isSet(mypp)) {
	ppset.atomicAddVP(mypp);
	firstAccessOnPP = 1;
    }
    vaddr = vaddr&(-PAGE_SIZE);		// convert to page address
    rc = DREF(fcm)->mapPage(fileOffset, regionVaddr, regionSize,
			    pfinfo, vaddr, access, hat, vp,
			    getRef(), firstAccessOnPP, pn);
    tassertWrn(_SUCCESS(rc),"Can't allocate a page frame in RegionDefault\n");

    //N.B. all exits must do a leave - even errors
 leave:
    requests.leave();
#if 0
    err_printf("leave %lx %ld\n",vaddr, vp);
#endif /* #if 0 */

    return (rc);
}

SysStatus
RegionDefault::unmapRange(uval start, uval size)
{
    SysStatus rc;

    //err_printf("RegionDefault::unmapRange: %lx/%lx, ppset %lx\n",
    //       start, size, uval(ppset));

    if (start < regionVaddr) {
	size -= (regionVaddr-start);
	start = regionVaddr;
    }

    if (size > regionSize) {
	size = regionSize;
    }

    rc = DREF(hat)->unmapRange(start, size, ppset);
    //err_printf("RegionDefault::unmapRange all done\n");

    return rc;
}

/*virtual*/ SysStatus
RegionDefault::vaddrToFCM(VPNum vpNum, uval vaddr, uval writeAccess,
			  FCMRef& fcmRef, uval& offset)
{
    (void) vpNum; // only needed in perprocessor regions
    SysStatus rc;
    uval user;
    AccessMode::pageFaultInfo pfinfo;

    rc = DREF(proc)->getUserMode(user);
    if (_FAILURE(rc)) return rc;

    // fabricate pfinfo

    pfinfo = writeAccess?AccessMode::writeFault:AccessMode::readFault;

    if ((vaddr < regionVaddr) ||
	(vaddr >= (regionVaddr + regionSize)) ||
	!AccessMode::verifyAccess(user, access, pfinfo)) {
	if ((vaddr < regionVaddr) ||
	    (vaddr >= (regionVaddr + regionSize))) {
	    rc = _SERROR(2078, 0, EFAULT);
	} else {
	    rc = _SERROR(2079, 0, EACCES);
	}
	return rc;
    }

    offset = fileOffset + (vaddr&(-PAGE_SIZE)) - regionVaddr;
    fcmRef = fcm;
    return 0;
}

/*
 * no locking.  The worst that can happen is that the FCM
 * is already deleted when the caller tries to use it,
 * and we can't prevent that in any case.
 */
SysStatus
RegionDefault::destroy()
{
    //err_printf("Region %p, destroying\n", getRef());
    TraceOSMemDestroyReg((uval64)getRef(),
	      regionVaddr, regionSize);
    if (requests.shutdown() != 0) {
	return 0;			// already destroyed
    }

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    //err_printf("Region %p, unmaprange a %lx, s %lx\n", getRef(),
    //       regionVaddr, regionSize);
    unmapRange(regionVaddr, regionSize);

    // now detach from the FCM
    if (fcm != NULL) {
	//err_printf("Region %p detaching from fcm %p\n", getRef(), fcm);
	DREF(fcm)->detachRegion(getRef());
    }

    // detach from Process - do this last.  Until this point, no new
    // region can reuse the virtual addresses involved.
    // because all outstanding requests are done, no new mappings will happen
    if (proc != NULL) {
	//err_printf("Region %p detaching from proc %p\n", getRef(), proc);
	DREF(proc)->detachRegion(getRef());
    }

    //N.B. don't set proc or fcm to null - there may be other calls in flight.
    //they will stay around, returning error, long enough to cover this
    //possibility.

    destroyUnchecked();

    return (0);
}

/*
 * internal routine
 * if target specified, verify it and get ProcessRef from XHandle
 * Otherwise, attach to calling process, which is always allowed
 */
/* static */ SysStatus
RegionDefault::PrefFromTarget(
    XHandle target, ProcessID callerPID, ProcessRef& pref)
{
    ObjRef tmp;
    SysStatus rc;
    TypeID type;

    if (target) {
	rc = XHandleTrans::XHToInternal(target, callerPID, MetaObj::attach,
					tmp, type);
	tassertWrn( _SUCCESS(rc), "woops\n");
	if (!_SUCCESS(rc)) return rc;

	// verify that type is cool
	if (!MetaProcessServer::isBaseOf(type)) {
	    tassertWrn(0, "woops\n");
	    return _SERROR(2548, 0, EINVAL);
	}
	pref = (ProcessRef)tmp;
    } else {
	// attach to caller
	rc = DREFGOBJ(TheProcessSetRef)
	    ->getRefFromPID(callerPID, (BaseProcessRef&)pref);
	if (!_SUCCESS(rc)) return rc;
    }
    return 0;
}


/* static */ SysStatus
RegionDefault::GetFR(
    XHandle target, ProcessID callerPID, ProcessRef& pref,
    ObjectHandle frOH, FRRef& frRef, uval &mrights)
{
    SysStatus rc;
    ObjRef objRef;
    TypeID type;
    rc = PrefFromTarget(target, callerPID, pref);
    _IF_FAILURE_RET(rc);

    rc = XHandleTrans::XHToInternal(frOH.xhandle(), callerPID,
				    MetaObj::attach, objRef, type);
    tassertMsg(_SUCCESS(rc), "RegionDefault failed XHToInternal\n");
    _IF_FAILURE_RET(rc);

    // verify that type is of FR
    if (!MetaFR::isBaseOf(type)) {
	tassertWrn(0, "object handle <%lx,%lx> not of correct type\n",
		   frOH.commID(), frOH.xhandle());
	return _SERROR(1539, 0, EINVAL);
    }

    frRef = (FRRef)objRef;

    // verify that the fr is "owned" by the target process and
    // check for write permissions
    ProcessID frPID, targetPID;
    uval urights;
    XHandleTrans::GetRights(frOH.xhandle(), frPID, mrights, urights);

    /*
     * one process can make regions for another it controls. Fork and
     * long exec work this way.  We allow the create of the fr is owned
     * by either the calling process or the target process.
     */
    if (frPID != callerPID &&
       frPID != (targetPID = _SGETPID(DREF(pref)->getPID()))) {
	err_printf("wrong pid caller %lx target %lx fr owner %lx\n",
		   callerPID, targetPID, frPID );
	return _SERROR(2369, 0, EPERM);
    }
    return 0;
}

/* static */ SysStatus
RegionDefault::CheckCaller(
    XHandle target, ProcessID callerPID, ProcessRef& pref,
    ObjectHandle frOH, AccessMode::mode accessreq,
    FRRef& frRef, uval& writable)
{
    uval mrights;
    SysStatus rc;
    rc = GetFR(target, callerPID, pref, frOH, frRef, mrights);
    _IF_FAILURE_RET(rc);

    writable = mrights & MetaObj::write;

    if (!writable && AccessMode::isWriteUser(accessreq)) {
//	err_printf("not writable\n");
    }

    return 0;
}


/* static */ SysStatus
RegionDefault::_CreateFixedLenExt(uval& regionVaddr, uval regionSize,
				  uval alignmentreq, ObjectHandle frOH,
				  uval fileOffset, uval accessreq,
				  XHandle target, RegionType::Type regionType, 
				  __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FRRef frRef;
    uval writable;

    rc = CheckCaller(target, callerPID, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);
    _IF_FAILURE_RET(rc);

    rc = RegionDefault::CreateFixedLen(ref, pref, regionVaddr,
				       regionSize, alignmentreq,
				       frRef, writable, fileOffset,
				       (AccessMode::mode)accessreq,
				       regionType);

    return rc;
}

#if 0
/* static */ SysStatus
RegionDefault::_CreateFixedLenExtDyn(uval& regionVaddr, uval regionSize,
				     uval alignmentreq, ObjectHandle frOH,
				     uval fileOffset, uval accessreq,
				     XHandle target, ObjectHandle tsOH,
				     RegionType::Type regionType,
				     __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    TypeID type;
    ObjRef objRef;
    FRRef frRef;

    rc = PrefFromTarget(target, callerPID, pref);
    _IF_FAILURE_RET(rc);

    rc = XHandleTrans::XHToInternal(frOH.xhandle(), callerPID,
				    MetaObj::attach, objRef, type);
    tassertWrn(_SUCCESS(rc), "RegionDefault failed XHToInternal\n");
    _IF_FAILURE_RET(rc)

    // verify that type is of FRComp - we'd really like just FR
// FIXMEXX talk to Orran, the brilliant, charming, ..
	if (!MetaFR::isBaseOf(type)) {
	tassertWrn(0, "object handle <%lx,%lx> not of correct type\n",
		   frOH.commID(), frOH.xhandle());
	return _SERROR(1540, 0, EINVAL);
    }

    //FIXME check xobj to make sure it matches our processID

    frRef = (FRRef)objRef;

    rc = RegionDefault::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, fileOffset,
	(AccessMode::mode)accessreq, regionType);

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
RegionDefault::_CreateFixedLenExtKludgeDyn(
    uval& regionVaddr, uval regionSize, uval alignmentreq, ObjectHandle frOH,
    uval fileOffset, uval accessreq, XHandle target, ObjectHandle tsOH,
    RegionType::Type regionType, __CALLER_PID callerPID)
{
    RegionRef ref=0;
    ProcessRef pref=0;
    SysStatus rc;
    FCMRef fcmRef;
    FRRef frRef;
    TypeID type;
    ObjRef objRef;

    rc = PrefFromTarget(target, callerPID, pref);
    _IF_FAILURE_RET(rc)

    // FIXME: OK heres my big Gross Kludge
    rc = FCMSharedTrivial<PageList<AllocGlobal>,AllocGlobal>::Create(fcmRef);
    tassert(_SUCCESS(rc), err_printf("Shared Trivial fcm create\n"));

    rc = FRPlaceHolder::Create(frRef);
    _IF_FAILURE_RET(rc);

    DREF(frRef)->installFCM(fcmRef);

    rc = RegionDefault::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, 1, fileOffset,
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

#if 1
    // switch the region also
    DREF(tsRef)->storeRefs(fcmRef, ref, pref, regionSize);
#else /* #if 1 */
    DREF(tsRef)->storeRefs(fcmRef, 0, pref, regionSize);
#endif /* #if 1 */

    // end kludge

    return rc;
}
#endif /* #if 0 */

/* static */ SysStatus
RegionDefault::_CreateFixedLenExtKludge(
    uval& regionVaddr, uval regionSize, uval alignmentreq, uval fileOffset,
    uval accessreq, XHandle target, uval partitioned, 
    RegionType::Type regionType, __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FRRef frRef;
    FCMRef fcmRef;

    rc = PrefFromTarget(target, callerPID, pref);
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

    rc = RegionDefault::CreateFixedLen(
	ref, pref, regionVaddr,
	regionSize, alignmentreq, frRef, 1, fileOffset,
	(AccessMode::mode)accessreq, regionType);

    return rc;
}

/* static */ SysStatus
RegionDefault::_CreateFixedAddrLenExt(uval regionVaddr, uval regionSize,
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

    rc = CheckCaller(target, callerPID, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);
    _IF_FAILURE_RET(rc);

    return (RegionDefault::CreateFixedAddrLen(
	regRef, pref, regionVaddr, regionSize, frRef, writable, fileOffset, 
	(AccessMode::mode)accessreq, regionType));
}

/* static */ SysStatus
RegionDefault::_CreateFixedLenWithinRangeExt(
    uval& regionVaddr, uval regionVaddr2, uval regionSize,
    uval alignmentreq, ObjectHandle frOH, uval fileOffset,
    uval accessreq, XHandle target, RegionType::Type regionType, 
    __CALLER_PID callerPID)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc;
    FRRef frRef;
    uval writable;

    rc = CheckCaller(target, callerPID, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);
    _IF_FAILURE_RET(rc);

    rc = RegionDefault::CreateFixedLenWithinRange(
	ref, pref, regionVaddr, regionVaddr2,
	regionSize, alignmentreq, frRef, writable, fileOffset,
	(AccessMode::mode)accessreq, regionType);

    return rc;
}

/* static */ SysStatus
RegionDefault::_CreateRebootImage(__out uval& regionVaddr,
				  __out uval& regionSize,
				  __CALLER_PID callerPID)
{
    SysStatus rc;
    RegionRef ref;
    ProcessRef pref;
    FCMRef fcmRef;
    FRRef frRef;
    uval imageAddr, imageSize;

    SysFacts::GetRebootImage(imageAddr, imageSize);
    if (imageSize == 0) {
	return _SERROR(1873, 0, ENOSYS);
    }

    tassert(PAGE_ROUND_DOWN(imageAddr) == imageAddr,
	    err_printf("rebootImageAddr not page-aligned\n"));
    tassert(PAGE_ROUND_DOWN(imageSize) == imageSize,
	    err_printf("rebootImageSize not page-aligned\n"));

    rc = PrefFromTarget(0, callerPID, pref);
    _IF_FAILURE_RET(rc);
    rc = FRPlaceHolder::Create(frRef);
    _IF_FAILURE_RET(rc);
    rc = FCMStartup::Create(fcmRef, imageAddr, imageSize);
    _IF_FAILURE_RET(rc);

    DREF(frRef)->installFCM(fcmRef);

    rc = RegionDefault::CreateFixedLen(ref, pref, regionVaddr, imageSize,
				       PAGE_SIZE, frRef, 1, 0,
				       AccessMode::writeUserWriteSup,
				       RegionType::K42Region);
    _IF_FAILURE_RET(rc);

    regionSize = imageSize;
    return 0;
}

void
RegionDefault::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaRegionDefault::init();
}

SysStatus
RegionDefault::forkCopy(FRRef &newFRRef)
{
    SysStatus rc;
    // lock out all requests, including destroy, until
    // the copy is finished.
    // requests in progress will finish before stop returns.
    rc = requests.stop();
    if (rc < 0) return _SERROR(1408, 0, EBUSY);
    rc = DREF(fcm)->forkCopy(newFRRef);
    requests.restart();
    return rc;
}


SysStatus
RegionDefault::forkCloneRegion(ProcessRef pref, RegionType::Type regionType)
{
    FRRef newFRRef;
    RegionRef ref;
    SysStatus rc;
    rc = requests.stop();
    if (rc < 0) return _SERROR(1346, 0, EBUSY);
    rc = DREF(fcm)->forkCloneFCM(newFRRef, regionType);
    requests.restart();
    if (_FAILURE(rc)) return rc;
    return CreateFixedAddrLen(
	ref, pref, regionVaddr, regionSize, newFRRef,
	writeAllowed, fileOffset, access, regionType);
}

/* virtual */
SysStatus
RegionDefault::cloneUnattachedFixedAddrLen(RegionRef& regRef,
                                           uval addr, uval sz, uval off,
                                           RegionType::Type regionType)
{
    SysStatus retvalue;
    FRRef frRef;

    retvalue = DREF(fcm)->getFRRef(frRef);
    tassertMsg(_SUCCESS(retvalue), "%ld: getFRRef failed how could this "
               "happen\n",retvalue);

    RegionDefault* reg = new RegionDefault;
    regRef = (RegionRef)CObjRootSingleRep::Create(reg);
    retvalue = reg->initRegion(proc, addr, 0, sz, 0, frRef, writeAllowed,
			       off, access, 1, regionType, 1);
    return (retvalue);
}

SysStatus
RegionDefault::truncate(uval start, uval size)
{
    SysStatus rc;

    if (pageSize != PAGE_SIZE) {
	// we don't implement large page segment resize yet
	return _SERROR(2796, 0, EINVAL);
    }

    /*
     * stop the world since the region bounds are being readjusted
     * and we don't want a map request in flight to undo the unmaps
     * below.
     * N.B. remember to restart on every exit path.
     */
    rc = requests.stop();
    if (rc < 0) return _SERROR(2728, 0, EBUSY);

    if ((regionVaddr != start) &&
       (regionVaddr+regionSize) != (start+size)) {
	requests.restart();
	return _SERROR(2358, 0, EINVAL);
    }

    if ((regionVaddr == start) && (regionSize == size)) {
	requests.restart();
	return destroy();
    }

    /*
     * this truncate may convert a shared to a private mapping by
     * truncating part of what was a complete segment.  If that
     * happens, we must unmap the whole (shared) segment, not just
     * the range being truncated.
     */
    uval unmapStart, unmapEnd;
    unmapStart = isSharedVaddr(start)?SEGMENT_ROUND_DOWN(start):start;
    unmapEnd = isSharedVaddr(start+size-1)?
	SEGMENT_ROUND_UP(start+size):start+size;
    tassertMsg(unmapStart>=regionVaddr && (unmapEnd-unmapStart) <= regionSize,
	       "confusion adjusting truncate addresses for shared segments\n");
    rc = DREF(hat)->unmapRange(unmapStart, unmapEnd-unmapStart, ppset);

    regionSize -= size;
    if (regionVaddr == start) {
	// truncating from the left
	regionVaddr+=size;
	fileOffset+=size;
    }

    requests.restart();
    return rc;
}
