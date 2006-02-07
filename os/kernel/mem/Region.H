#ifndef __REGION_H_
#define __REGION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Region.H,v 1.45 2004/01/28 21:29:01 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for object invocation of Region
 * **************************************************************************/

#include "mem/Access.H"
#include "mem/RegionType.H"

#ifdef _IN_STUBGEN
struct HAT;
typedef HAT **HATRef;
#else
#include "mem/HAT.H"
#endif /* #ifndef _IN_STUBGEN */

class PageFaultNotification;
/*
 * The trivial region class that is bound to a contiguous portion
 * of physical memory
 */
class Region : public Obj {
public:

    enum ClassCodes { AddressOutOfRange=1 };
    
    DEFINE_REFS(Region);
    virtual SysStatus getFCM(FCMRef &ref) = 0;
    
    /* FIXME: DANGEROUS BIG KLUDGE */
    virtual SysStatus setFCM(FCMRef ref) = 0;

    virtual SysStatus getVaddr(uval &vaddr) = 0;

    virtual void      setVaddr(uval v) = 0;

    virtual SysStatus getOffset(uval &offset) = 0;

    virtual void      setOffset(uval v) = 0;
    
    virtual SysStatus getSize(uval &size)=0;

    virtual SysStatus isSharedOffset(uval offset)=0;

    virtual SysStatus isSharedVaddr(uval vaddr)=0;

    virtual SysStatus isAccessMode(AccessMode::mode m)=0;

    virtual SysStatus  coverVaddr(uval vaddr)=0;
\
    /*
     * handle a page table miss (not a tlb miss)
     * ==0 in-core, > 0 asynch, < 0 error
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp) = 0;

    /* unmaps the page at this FCM offset from the process this region
     * is attached to (only for local processor).
     */
    virtual SysStatus unmapPage(uval offset)=0;

    /* unmaps range from start vaddr with given size across all processors
     */
    virtual SysStatus unmapRange(uval start, uval size)=0;

    virtual SysStatus vaddrToFCM(
	VPNum vpNum, uval vaddr, uval writeAccess,
	FCMRef& fcmRef, uval& offset)=0;

    /*
     * regions can only exists attached to a Process and an FCM.
     * both the process and the FCM may use destroy to "detach"
     * a region.
     * destroy is always coordiniated by the region.  Even if called
     * from a process or FCM, there will be a call back to actually
     * detach from the process and FCM.  Thus the process or FCM must
     * call destroy in a lock state which allows the call back.
     */
    virtual SysStatus destroy() = 0;

    virtual SysStatus forkCloneRegion(
	ProcessRef pref, RegionType::Type regionType)=0;

    virtual SysStatus forkCopy(FRRef& fr)=0;

    virtual SysStatus cloneUnattachedFixedAddrLen(RegionRef& regRef,
                                                  uval addr, uval sz, uval off,
                                                  RegionType::Type regionType)
        =0;

    virtual SysStatusUval stall()=0;
    virtual SysStatus     unStall()=0;

    // shrink region
    virtual SysStatus truncate(uval start, uval length)=0;
    /*
     * useful for debugging in FCMs
     * also called by FCM when paging is to servers to get the pid of 
     * the process to see if it is a base server that cannot be 
     * paged.
     */
    virtual SysStatusProcessID getPID()=0;
};



#endif /* #ifndef __REGION_H_ */
