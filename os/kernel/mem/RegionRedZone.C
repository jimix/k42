/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionRedZone.C,v 1.32 2004/07/08 17:15:38 gktse Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "mem/RegionRedZone.H"
#include "meta/MetaRegionRedZone.H"
#include "mem/FCM.H"
#include "mem/HAT.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>
#include <trace/traceMem.h>
#include <cobj/XHandleTrans.H>
#include <meta/MetaProcessServer.H>
#include <cobj/CObjRootSingleRep.H>


SysStatusUval
RegionRedZone::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
			   PageFaultNotification */*pn*/, VPNum vp)
{
    /*
     * Accesses to red zone memory always faults
     */
    return _SERROR(1249, 0, EFAULT);
}

/* static */ SysStatus
RegionRedZone::CreateFixedAddrLen(
    RegionRef& ref, ProcessRef p, uval v, uval s, RegionType::Type regionType)
{
    SysStatus retvalue;
    TraceOSMemRegCreateFix(v, s);
    RegionRedZone* reg = new RegionRedZone;
    ref = (RegionRef)CObjRootSingleRep::Create(reg);
    retvalue = reg->initRegion(
	p, v, 0, s, 0, (FRRef)0, 1, 0,
	/*N.B. AccessMode doesn't matter but compiler insists */
	AccessMode::writeUserWriteSup, 1, regionType);
    return retvalue;
}

SysStatus
RegionRedZoneKernel::CreateFixedAddrLen(
    RegionRef& ref, ProcessRef p, uval v, uval s)
{
    SysStatus retvalue;
    TraceOSMemRegCreateFix(v, s);
    RegionRedZoneKernel* reg = new RegionRedZoneKernel;
    ref = (RegionRef)CObjRootSingleRepPinned::Create(reg);
    retvalue = reg->initRegion(
	p, v, 0, s, 0, (FRRef)0, 1, 0,
	/*N.B. AccessMode doesn't matter but compiler insists */
	AccessMode::writeUserWriteSup, 1, RegionType::K42Region);
    return retvalue;
}


/* static */ SysStatus
RegionRedZone::_CreateFixedAddrLen(
    uval regionVaddr, uval regionSize, XHandle target,
    RegionType::Type regionType, __CALLER_PID caller)
{
    RegionRef ref;
    ProcessRef pref=0;
    SysStatus rc = PrefFromTarget(target, caller, pref);
    tassertWrn( _SUCCESS(rc), "woops\n");
    if (!_SUCCESS(rc)) return rc;

    return RegionRedZone::CreateFixedAddrLen(
	ref, pref, regionVaddr, regionSize, regionType);

}

void
RegionRedZone::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaRegionRedZone::init();
}
