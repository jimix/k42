#ifndef __REGION_LIST_H_
#define __REGION_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionList.H,v 1.35 2004/03/18 19:15:27 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A class that keeps pointers to a single region,
 * maintains information that indicates the virtual address range
 * allocated for the region
 * **************************************************************************/
template<class ALLOC> class RegionList;

#include "proc/Process.H"

template<class ALLOC> class RegionHolder {
    friend class RegionList<ALLOC>;

    RegionType::Type type;
    uval vaddr;
    uval size;
    RegionRef reg;
    RegionHolder *next;
#ifdef PROTOTYPE_SUBLIST
    RegionList<ALLOC> *subList;
#endif

public:

    uval getSize()			{ return size; }
    uval getVaddr()			{ return vaddr;}
    RegionRef getReg()			{ return reg;  }
    RegionHolder *getNext()		{ return next; }

    RegionHolder(uval vmbase, uval isize);

#ifdef PROTOTYPE_SUBLIST
    ~RegionHolder() { tassertMsg(subList==0, "non-zero Region subList\n"); }
#endif

    SysStatus attachRegion(RegionRef r, RegionType::Type regionType) {
	type = regionType;
	reg = r; 
	return 0; 
    }

#ifdef PROTOTYPE_SUBLIST
    SysStatus attachSubRegion(uval v, uval s, 
                              RegionRef r, RegionType::Type regionType) {
        tassertMsg(vaddr <= v && (v + s) <= vaddr + size,
                   "v=0x%lx s=0x%lx is not between vaddr=0x%lx"
                   " size=0x%lx\n", v, s, vaddr, size);
        if (!subList) {
            subList = new RegionList<ALLOC>;
            subList->setRegionsBounds(vaddr, vaddr, vaddr+size);
        }
        return subList->attachFixedRegion(v, s, r, regionType);
    } 

    SysStatus dettachSubRegion(uval vaddr) {
        passertMsg(0, "NYI\n");
        return 0;
    }
#endif

    DEFINE_ALLOC_NEW(RegionHolder);
};

/*
 * class for maintaining the list of regions of a program
 */
template<class ALLOC> class RegionList {

    RegionHolder<ALLOC> *regions;
    BLock   lock;
    uval regionsAllocStart; //Lowest address we may allocate for a region
    uval regionsStart;      //Lowest address that may be specified for a region
    uval regionsEnd;

    /*
     * reserves space for region to be added
     * if alignment==0, then RegionList chooses alignment,
     *  otherwise uses alignment requested
     */
    RegionHolder<ALLOC> *reserveSomeSpace(uval isize, SysStatus &rc,
					  uval rangeStart, uval rangeEnd,
					  uval alignment=0);

    /*
     * add region at a specified address and size
     */
    RegionHolder<ALLOC> *reserveFixedSpace(uval size, SysStatus &rc,
					   uval addr);

    // unlocked version for internal use when holding the lock
    void uPrintRegions();

    SysStatus locked_truncate(uval start, uval length, RegionRef &reg);

    SysStatus locked_attachFixedRegion(uval vaddr, uval size, RegionRef reg, 
                                       RegionType::Type regionType);

#ifdef PROTOTYPE_SUBLIST
    SysStatus locked_attachFixedSubRegion(uval vaddr, uval size, RegionRef reg, 
                                          RegionType::Type regionType,
                                          RegionRef &parent);


    RegionHolder<ALLOC> *locked_findAndCheckRegion(uval vmbase, uval isize,
                                                   SysStatus &rc);
#endif
public:
    RegionList();

    DEFINE_ALLOC_NEW(RegionList);

    void setRegionsBounds(uval start, uval allocStart, uval end)
    {
	regionsStart = start;
	regionsAllocStart = allocStart;
	regionsEnd = end;
    }

    SysStatus checkRegion(uval vmbase, uval isize);

    /*
     * attach a region with address determined by regionList
     */
    SysStatus attachDynamicRegion(
	uval &vaddr, uval size, RegionRef reg,
	RegionType::Type regionType, uval alignment=0);

    /*
     * attach an region at fixed address to corresponding RegionHolder
     */
    SysStatus attachFixedRegion(uval vaddr, uval size, RegionRef reg, 
				RegionType::Type regionType) {
        AutoLock<BLock> al(&lock);	// locks now, unlocks on return
        return locked_attachFixedRegion(vaddr, size, reg, regionType);
    }

#ifdef PROTOTYPE_SUBLIST
    /*
     * attach a sub region at fixed address to corresponding RegionHolder
     */
    SysStatus attachFixedSubRegion(uval vaddr, uval size, RegionRef reg, 
                                   RegionType::Type regionType, 
                                   RegionRef &parent) {
        AutoLock<BLock> al(&lock);	// locks now, unlocks on return
        return locked_attachFixedSubRegion(vaddr, size, reg, regionType,
                                           parent);
    }
#endif

    /*
     * attach a region within range with address determined by regionList
     */
    SysStatus attachWithinRangeRegion(
	uval &vaddr, uval vaddr2, uval size,
	RegionRef reg, RegionType::Type regionType,
	uval alignment=0);

    SysStatus detachRegion(RegionRef reg);

    SysStatus deleteRegionsAll();

    SysStatus vaddrToRegion(uval vaddr, RegionRef &reg);

    SysStatus truncate(uval start, uval length, RegionRef &reg) {
        AutoLock<BLock> al(&lock);	// locks now, unlocks on return
        return locked_truncate(start,length,reg);
    }

    SysStatus truncateAndInsertInPlace(uval truncVaddr, uval truncSize,
                                       RegionRef truncReg,
                                       uval newVaddr1, uval newSize1, 
                                       RegionRef newReg1, 
                                       RegionType::Type newType1,
                                       uval newVaddr2, uval newSize2,
                                       RegionRef newReg2,
                                       RegionType::Type newType2);
        
    void printRegions();

    void kosher();

    SysStatus getNextRegion(uval& vaddr, RegionRef &reg);

    SysStatus findRegion(uval start,
			 RegionType::RegionElementInfo& element);

    SysStatus findRegion(uval vaddr, RegionRef &reg, uval &start, uval &size,
                         RegionType::Type &type);

#ifdef PROTOTYPE_SUBLIST
    SysStatus findRegion(uval vaddr, RegionRef &reg, uval &start, uval &size,
                         RegionType::Type &type, RegionList<ALLOC> **subRegs);
#endif

    // THIS IS JUST TO LEARN WHAT'S GOING ON
    SysStatus changeRegionType(uval vaddr, RegionType::Type newType) {
	AutoLock<BLock> al(&lock);	// locks now, unlocks on return
	RegionHolder<ALLOC> *r = regions;
	while (r) {
	    if ((r->vaddr <= vaddr) && (vaddr < (r->vaddr + r->size))) {
		err_printf("found region we want to change\n");
		r->type = newType;
		return 0;
	    }
	    r = r->next;
	}
	
	return _SERROR(2806, 100, EINVAL);
    }

    SysStatus preFork(XHandle childXH, ProcessID callerID);

    SysStatus preExec();

    SysStatus purge();
};

#endif /* #ifndef __REGION_LIST_H_ */
