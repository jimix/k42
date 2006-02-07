/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionPerProcessor.C,v 1.50 2004/07/08 17:15:38 gktse Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "mem/RegionPerProcessor.H"
#include "meta/MetaRegionPerProcessor.H"
#include "mem/FCM.H"
#include "mem/HAT.H"
#include "proc/Process.H"
#include <trace/traceMem.h>
#include <cobj/XHandleTrans.H>
#include "mem/FRComputation.H"
#include "mem/FRLTransTable.H"
#include <cobj/CObjRootSingleRep.H>
#include "mem/PerfStats.H"

//FIXME:***** Sorry about the code duplication for RegionPerProcessorKernel
//      but it was a lot easier than trying to fix the RegionDefault tree
//      via templates.  The Stub generator causes pain.

SysStatus
RegionPerProcessor::Create(RegionRef& ref, ProcessRef p, FRRef fr,
			   AccessMode::mode accessreq, uval& v, uval& s,
			   RegionType::Type regionType,
			   RegionRef use_this_ref)
{
    (void) ref; (void) p;
    (void) fr; (void) accessreq; (void) v; (void) s; (void) regionType;

    return _SERROR(2533, 0, EINVAL);
}

SysStatus
RegionPerProcessorKernel::Create(RegionRef& ref, ProcessRef p, FRRef fr,
                                 AccessMode::mode accessreq, uval& v, uval& s,
				 RegionType::Type regionType,
                                 RegionRef use_this_ref)
{
    (void) ref; (void) p;
    (void) fr; (void) accessreq; (void) v; (void) s; (void) regionType;

    return _SERROR(1113, 0, EINVAL);
}


SysStatusUval
RegionPerProcessor::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
				PageFaultNotification *pn, VPNum vp)
{
    SysStatusUval retvalue;

    ScopeTime timer(RegionHandlerTimer);

    /*
     * We do per-processor memory by mapping the region into successive
     * sections of the underlying file for each processor.
     * vp*RegionSize if the offset for the vp'th processor.
     *
     * We call an internal, non-virtual, inlined method of regionDefault
     * to avoid a double method dispatch on this path.
     */
    retvalue = RegionDefault::handleFaultInternal(
	pfinfo, vaddr, pn, vp, fileOffset+vp*regionSize);
    return (retvalue);
}

SysStatusUval
RegionPerProcessorKernel::handleFault(AccessMode::pageFaultInfo pfinfo,
                                      uval vaddr,
                                      PageFaultNotification *pn, VPNum vp)
{
    SysStatusUval retvalue;

    /*
     * We do per-processor memory by mapping the region into successive
     * sections of the underlying file for each processor.
     * vp*RegionSize if the offset for the vp'th processor.
     *
     * We call an internal, non-virtual, inlined method of regionDefault
     * to avoid a double method dispatch on this path.
     */
    retvalue = RegionDefault::handleFaultInternal(
	pfinfo, vaddr, pn, vp, fileOffset+vp*regionSize);
    return (retvalue);
}

/*
 * no locking.  The worst that can happen is that the FCM
 * is already deleted when the caller tries to use it,
 * and we can't prevent that in any case.
 */
/*virtual*/ SysStatus
RegionPerProcessor::vaddrToFCM(
    VPNum vpNum, uval vaddr, uval writeAccess, FCMRef& fcmRef, uval& offset)
{
    SysStatus rc;
    rc = RegionDefault::vaddrToFCM(vpNum,
                                          vaddr, writeAccess, fcmRef, offset);
    offset += vpNum*regionSize;
    return rc;
}

/*
 * no locking.  The worst that can happen is that the FCM
 * is already deleted when the caller tries to use it,
 * and we can't prevent that in any case.
 */
/*virtual*/ SysStatus
RegionPerProcessorKernel::vaddrToFCM(
    VPNum vpNum, uval vaddr, uval writeAccess, FCMRef& fcmRef, uval& offset)
{
    SysStatus rc;
    rc = RegionDefault::vaddrToFCM(vpNum,
                                          vaddr, writeAccess, fcmRef, offset);
    offset += vpNum*regionSize;
    return rc;
}

/*
 * unmaps the page from the process this region is attached to
 * argument is the FCM offset - which the Region can convert
 * to a virtual address
 * we find the specific vp this offset is in.
 * see size check comments in RegionDefault.
 */
SysStatus
RegionPerProcessor::unmapPage(uval offset)
{
    if (hat && ((offset-fileOffset) < (regionSize*Scheduler::VPLimit))) {
	VPNum vp;
	//calculate which vp this offset is "in"
	vp = (offset-fileOffset)/regionSize;
	/* vp should be local vp, although not having the vp is ok as well.
	 * this actually may not be fatal if for some reason we share these
	 */
	VPNum pp;
	tassert(_FAILURE(DREF(proc)->vpnumToPpnum(vp, pp))
		|| (pp == Scheduler::GetVP()),
		err_printf("RPP::unmapPageLocal wrong proc: %ld != %ld\n",
			   uval(pp), uval(Scheduler::GetVP())));
	DREF(hat)->unmapPage((offset-fileOffset)-vp*regionSize+regionVaddr);
    }
    return 0;
}

/*
 * unmaps the page from the process this region is attached to
 * argument is the FCM offset - which the Region can convert
 * to a virtual address
 * we find the specific vp this offset is in
 */
SysStatus
RegionPerProcessorKernel::unmapPage(uval offset)
{
    if (hat) {
	VPNum vp;
	//calculate which vp this offset is "in"
	vp = (offset-fileOffset)/regionSize;
	/* vp should be local vp, although not having the vp is ok as well.
	 * this actually may not be fatal if for some reason we share these
	 */
	VPNum pp;
	tassert(_FAILURE(DREF(proc)->vpnumToPpnum(vp, pp))
		|| (pp == Scheduler::GetVP()),
		err_printf("RPP::unmapPageLocal wrong proc: %ld != %ld\n",
			   uval(pp), uval(Scheduler::GetVP())));
	DREF(hat)->unmapPage((offset-fileOffset)-vp*regionSize+regionVaddr);
    }
    return 0;
}

SysStatus
RegionPerProcessor::forkCloneRegion(
    ProcessRef pref, RegionType::Type regionType)
{
    FRRef newFRRef;
    RegionRef ref;
    SysStatus rc;
    rc = requests.stop();
    if (rc < 0) return _SERROR(2712, 0, EBUSY);
    rc = DREF(fcm)->forkCloneFCM(newFRRef, regionType);
    requests.restart();
    if (_FAILURE(rc)) return rc;
    return CreateFixedAddrLen(
	ref, pref, regionVaddr, regionSize, newFRRef,
	writeAllowed, fileOffset, access, regionType);
}

SysStatus
RegionPerProcessor::CreateFixedLen(
    RegionRef& ref, ProcessRef p,
    uval& v, uval s, uval alignmentreq,
    FRRef fr, uval writable, uval fO, AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    TraceOSMemRegCreateFix(v, s);
    RegionPerProcessor* reg = new RegionPerProcessor;
    ref = (RegionRef)CObjRootSingleRep::Create(reg);
    return reg->initRegion(p, v, 0, s, alignmentreq, fr, writable,
			   fO, accessreq, 0, regionType);
}

SysStatus
RegionPerProcessor::CreateFixedAddrLen(
    RegionRef& ref, ProcessRef p,
    uval v, uval s,
    FRRef fr, uval writable, uval fO,
    AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    TraceOSMemRegCreateFix(v, s);
    RegionPerProcessor *reg = new RegionPerProcessor;
    ref = (RegionRef)CObjRootSingleRep::Create(reg);
    return reg->initRegion(p, v, 0, s, 0, fr, writable, fO, accessreq, 1,
			   regionType);
}

SysStatus
RegionPerProcessorKernel::CreateFixedLen(
    RegionRef& ref, ProcessRef p,
    uval& v, uval s, uval alignmentreq,
    FRRef fr, uval writable, uval fO, AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    TraceOSMemRegCreateFix(v, s);
    RegionPerProcessorKernel* reg = new RegionPerProcessorKernel;
    ref = (RegionRef)CObjRootSingleRepPinned::Create(reg);
    return reg->initRegion(p, v, 0, s, alignmentreq, fr, writable,
			   fO, accessreq, 0, regionType);
}

SysStatus
RegionPerProcessorKernel::CreateFixedAddrLen(
    RegionRef& ref, ProcessRef p,
    uval v, uval s,
    FRRef fr, uval writable, uval fO,
    AccessMode::mode accessreq,
    RegionType::Type regionType,
    RegionRef use_this_ref)
{
    TraceOSMemRegCreateFix(v, s);
    RegionPerProcessorKernel *reg = new RegionPerProcessorKernel;
    ref = (RegionRef)CObjRootSingleRepPinned::Create(reg);
    return reg->initRegion(p, v, 0, s, 0, fr, writable, fO, accessreq, 1,
			   regionType);
}

/* static */ SysStatus
RegionPerProcessor::_CreateFixedLenExt(
    uval& vaddr, uval regSize, ObjectHandle frOH,
    uval fileOffset, uval accessreq, XHandle target, 
    RegionType::Type regionType,
    __CALLER_PID caller)
{
    RegionRef regRef;
    ProcessRef pref=0;
    FRRef frRef;
    uval writable;
    SysStatus rc;

    rc = CheckCaller(target, caller, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);

    RegionPerProcessor* reg = new RegionPerProcessor;

    regRef = (RegionRef)CObjRootSingleRep::Create(reg);

    rc = reg->initRegion(
	pref, vaddr, 0, regSize, 0, frRef, writable, fileOffset,
	(AccessMode::mode)accessreq, 0, regionType);
    return (rc);
}

/* static */ SysStatus
RegionPerProcessor::_CreateFixedAddrLenExt(
    uval vaddr, uval regSize, ObjectHandle frOH,
    uval fileOffset, uval accessreq, XHandle target, 
    RegionType::Type regionType,
    __CALLER_PID caller)
{
    RegionRef regRef;
    ProcessRef pref=0;
    FRRef frRef;
    uval writable;
    SysStatus rc;

    rc = CheckCaller(target, caller, pref, frOH,
		     (AccessMode::mode)accessreq, frRef, writable);

    RegionPerProcessor* reg = new RegionPerProcessor;

    regRef = (RegionRef)CObjRootSingleRep::Create(reg);

    rc = reg->initRegion(
	pref, vaddr, 0, regSize, 0, frRef, writable, fileOffset,
	(AccessMode::mode)accessreq, 1, regionType);
    return (rc);
}

void
RegionPerProcessor::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaRegionPerProcessor::init();
}

