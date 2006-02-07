#ifndef __REGION_RED_ZONE_H_
#define __REGION_RED_ZONE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionRedZone.H,v 1.21 2003/07/17 19:28:45 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Derived class for PerProcessor Regions.
 *
 * Refuses all map request - used to make "red zones" to catch errors,
 * for example the page at location 0.
 *
 * **************************************************************************/

#include "mem/RegionDefault.H"

class RegionRedZone : public RegionDefault{
    DEFINE_GLOBAL_NEW(RegionRedZone);
public:
    /*
     * handle a page table miss (not a tlb miss)
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp);

    /* redzone  - at a fixed address, with a fixed length      */
    static SysStatus
    CreateFixedAddrLen(RegionRef& ref, ProcessRef p,
		       uval regionVaddr, uval regionSize,
		       RegionType::Type regionType);

    // initializes meta information for creating regions of this type
    static void ClassInit(VPNum vp);

__xprivate:
    static SysStatus
    _CreateFixedAddrLen(__in uval regionVaddr, __in uval regionSize,
			__in XHandle target,
			__in RegionType::Type regionType,
			__CALLER_PID caller);
};

class RegionRedZoneKernel: public RegionRedZone {
public:
    DEFINE_PINNEDGLOBAL_NEW(RegionRedZone);

    /* redzone  - at a fixed address, with a fixed length      */
    static SysStatus
    CreateFixedAddrLen(RegionRef& ref, ProcessRef p,
		       uval regionVaddr, uval regionSize);
};

#endif /* #ifndef __REGION_RED_ZONE_H_ */
