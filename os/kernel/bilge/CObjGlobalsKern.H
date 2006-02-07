#ifndef __COBJGLOBALS_KERN_H_
#define __COBJGLOBALS_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjGlobalsKern.H,v 1.2 2005/06/15 04:49:12 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: management of the kernel's well-known clustered objects
 * **************************************************************************/
#include <cobj/CObjGlobals.H>

class CObjGlobalsKern : public CObjGlobals {
public:
    enum {
	   ConsoleIndex             =ReservedForConsole,
	   KernPageAllocIndex       =numReservedEntries+1,
	   // this slot available for reuse
	   KernHatIndex             =KernPageAllocIndex+1,
	   KernOTRegionIndex        =KernHatIndex+1,
	   KernFRRealIndex          =KernOTRegionIndex+1,
	   PMRootIndex              =KernFRRealIndex+1,
	   FSSwapIndex              =PMRootIndex+1,
	   KernelInfoMgrIndex       =FSSwapIndex+1,
	   ResourceManagerIndex     =KernelInfoMgrIndex+1,
           HWPerfMonIndex           =ResourceManagerIndex+1,
	   SyncServiceIndex         =HWPerfMonIndex+1,
	   numReservedEntriesKern   =SyncServiceIndex+1
    };

    // just type cheat the normal application's process ref
    static ProcessRef TheProcessRef()
	{return (ProcessRef)GOBJ(TheProcessRef);}

    static PageAllocatorKernPinnedRef ThePinnedPageAllocatorRef()
	{return (PageAllocatorKernPinnedRef)
                COSMgr::indexToRef(KernPageAllocIndex);}

    static HATRef TheKernelHATRef()
        {return (HATRef)COSMgr::indexToRef(KernHatIndex);}

    static RegionRef TheKernelOTRegionRef()
        {return (RegionRef)COSMgr::indexToRef(KernOTRegionIndex);}

    static FRRef TheFRRealRef()
        {return (FRRef)COSMgr::indexToRef(KernFRRealIndex);}

    static PMRef ThePMRootRef()
        {return (PMRef)COSMgr::indexToRef(PMRootIndex);}

    static FSSwapRef TheFSSwapRef()
        {return (FSSwapRef) COSMgr::indexToRef(FSSwapIndex);}

    static KernelInfoMgrRef TheKernelInfoMgrRef()
        {return (KernelInfoMgrRef) COSMgr::indexToRef(KernelInfoMgrIndex);}

    static ResMgrWrapperRef TheResourceManagerRef()
        {return (ResMgrWrapperRef)COSMgr::indexToRef(ResourceManagerIndex);}

    static HWPerfMonRef TheHWPerfMonRef()
        {return (HWPerfMonRef)COSMgr::indexToRef(HWPerfMonIndex);}

    static SyncServiceRef TheSyncServiceRef()
        {return (SyncServiceRef)COSMgr::indexToRef(SyncServiceIndex);}

    static FileLinuxRef TheConsoleRef()
	{ return (FileLinuxRef)COSMgr::indexToRef(ConsoleIndex);}
};


#endif /* #ifndef __COBJGLOBALS_KERN_H_ */
