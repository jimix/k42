#ifndef __REGION_PER_PROCESSOR_H_
#define __REGION_PER_PROCESSOR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionPerProcessor.H,v 1.31 2003/07/17 19:28:44 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Derived class for PerProcessor Regions.
 *
 * Works like RegionDefault except that the mapping of pages to the FCM
 * is offset by VPNum*RegionSize for each processor
 *
 * **************************************************************************/

#include "mem/RegionDefault.H"

class RegionPerProcessor : public RegionDefault{
protected:
public:
    virtual SysStatus isSharedOffset(uval offset) {
	(void) offset; return 0;}

    virtual SysStatus isSharedVaddr(uval vaddr) {
	(void) vaddr; return 0;}

    /*
     * handle a page table miss (not a tlb miss)
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp);

    virtual SysStatus vaddrToFCM(
	VPNum vpNum, uval vaddr, uval writeAccess,
	FCMRef& fcmRef, uval& offset);

    /*
     * N.B. must have a copy of this because it must call its own
     * CreateFixedAddrLen function.  This is how we clone the correct
     * type of region.
     */
    virtual SysStatus forkCloneRegion(
	ProcessRef pref, RegionType::Type regionType);

    /* unmaps the page at this FCM offset from the process this region
     * is attached to
     */
    virtual SysStatus unmapPage(uval offset);

    DEFINE_GLOBALPADDED_NEW(RegionPerProcessor);

    /*
     * N.B. this form IS NOT supported for PerProcessor Regions
     * since it makes no sense to map an existing FCM in sections, one
     * on each processor.
     *
     * Make a normal region - FCM determines location and size. Such
     * regions may be more efficient. Use static function rather than
     * constructor to deal gracefully with failure.  We delete the bad
     * Region and return NULL and a SysStatus
     */
    static SysStatus
    Create(RegionRef& ref, ProcessRef p, FRRef fr,
	   AccessMode::mode accessreq, uval& v, uval& s,
	   RegionType::Type regionType, RegionRef use_this_ref=0);

    /* map a part of a file to address space                */
    /*    - with a fixed length                             */
    static SysStatus
    CreateFixedLen(
	RegionRef& ref, ProcessRef p, uval& regionVaddr,
	uval regionSize, uval alignmentreq,
	FRRef fr, uval writable, uval fileOffset, AccessMode::mode accessreq,
	RegionType::Type regionType, RegionRef use_this_ref=0);

    /*    - at a fixed address, with a fixed length         */
    static SysStatus
    CreateFixedAddrLen(
	RegionRef& ref, ProcessRef p,
	uval regionVaddr, uval regionSize,
	FRRef fr, uval writable, uval fileOffset,
	AccessMode::mode accessreq, RegionType::Type regionType,
	RegionRef use_this_ref=0);

    // initializes meta information for creating regions of this type
    static void ClassInit(VPNum vp);

__xprivate:
    static SysStatus _CreateFixedLenExt(
	__out uval& vaddr,
	__in uval regSize, __in ObjectHandle frOH, __in uval fileOffset,
	__in uval accessreq, __in XHandle target,
	__in RegionType::Type regionType,
	__CALLER_PID caller);

    static SysStatus _CreateFixedAddrLenExt(
	__in uval vaddr,
	__in uval regSize, __in ObjectHandle frOH, __in uval fileOffset,
	__in uval accessreq, __in XHandle target,
	__in RegionType::Type regionType,
	__CALLER_PID caller);
};

#ifndef _IN_STUBGEN
//FIXME:***** Sorry about the code duplication for RegionPerProcessorKernel
//      but it was a lot easier than trying to fix the RegionDefault tree
//      via templates.  The Stub generator causes pain.
class RegionPerProcessorKernel : public RegionDefaultKernel{
protected:
public:
    virtual SysStatus isSharedOffset(uval offset) {
	(void) offset; return 0;}

    virtual SysStatus isSharedVaddr(uval vaddr) {
	(void) vaddr; return 0;}

    /*
     * handle a page table miss (not a tlb miss)
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp);

    virtual SysStatus vaddrToFCM(
	VPNum vpNum, uval vaddr, uval writeAccess,
	FCMRef& fcmRef, uval& offset);

    /* unmaps the page at this FCM offset from the process this region
     * is attached to
     */
    virtual SysStatus unmapPage(uval offset);
    /*
     * N.B. this form IS NOT supported for PerProcessor Regions
     * since it makes no sense to map an existing FCM in sections, one
     * on each processor.
     *
     * Make a normal region - FCM determines location and size. Such
     * regions may be more efficient. Use static function rather than
     * constructor to deal gracefully with failure.  We delete the bad
     * Region and return NULL and a SysStatus
     */
    static SysStatus
    Create(RegionRef& ref, ProcessRef p, FRRef fr,
	   AccessMode::mode accessreq, uval& v, uval& s,
	   RegionType::Type regionType=RegionType::K42Region,
	   RegionRef use_this_ref=0);

    /* map a part of a file to address space                */
    /*    - with a fixed length                             */
    static SysStatus
    CreateFixedLen(
	RegionRef& ref, ProcessRef p,
	uval& regionVaddr, uval regionSize, uval alignmentreq,
	FRRef fr, uval writable, uval fileOffset, AccessMode::mode accessreq,
	RegionType::Type regionType=RegionType::K42Region,
	RegionRef use_this_ref=0);

    /*    - at a fixed address, with a fixed length         */
    static SysStatus
    CreateFixedAddrLen(
	RegionRef& ref, ProcessRef p,
	uval regionVaddr, uval regionSize,
	FRRef fr, uval writable, uval fileOffset,
	AccessMode::mode accessreq,
	RegionType::Type regionType=RegionType::K42Region,
	RegionRef use_this_ref=0);

    DEFINE_PINNEDGLOBALPADDED_NEW(RegionPerProcessorKernel);
};
#endif /* #ifndef _IN_STUBGEN */
#endif /* #ifndef __REGION_PER_PROCESSOR_H_ */
