#ifndef __COBJGLOBALS_H_
#define __COBJGLOBALS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjGlobals.H,v 1.1 2004/07/11 21:59:24 andrewb Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: management of the well-known clustered objects
 * **************************************************************************/

#include <cobj/sys/COSMgr.H>
#include <cobj/ObjectRefs.H>

class CObjGlobals {
    friend class COSMgrObject;

protected:
    // dummy clustered object that generates a breakpoint for all calls
    static RepRef theBreakpointObjectRef; // actual reference variable

public:
    enum {
	   // reserved Entries and their Indices
	   // Leave 0 blank because we want 1-1 correspondence with reserved
	   // XHandles and XHandle 0 is not valid
	   COSMgrIndex              =1,
	   XHandleTransIndex        =2,
	   TypeMgrIndex             =3,
	   PageAllocatorIndex       =4,
	   DispatcherMgrIndex       =5,
	   TTYIndex                 =6,  /* unused */
	   SchedulerServiceIndex    =7,
	   SMTMgrIndex		    =8,
	   MountPointMgrIndex       =9,
	   ProcessIndex             =10,
           ProcessSetIndex          =11,
	   BlockedThreadQueuesIndex =12,
	   /* =13 available*/
	   ProcessLinuxIndex        =14,
	   ResourceManagerIndex     =15,
	   SystemMiscIndex          =16,
	   // reserve entry for console in kernel, since we
	   // need well known ID as target of IPC
	   ReservedForConsole       =17,
	   numReservedEntries       =18
    };

    static uval GlobalXHandleIdx(ObjRef ref)
    {
	if (!IsGlobalRef(ref)) {
	    return 0;
	}
	return (uval)(((LTransEntry *)ref) - COSMgr::indexToRef(0));
    }

    static uval IsGlobalRef(ObjRef ref)
    {
        return ((uval)ref - (uval)COSMgr::indexToRef(0)) <
            (numReservedEntries * sizeof(TransEntry));
    }

    // STATIC Methods identifying well known Clustered Objects
    static COSMgrRef TheCOSMgrRef()
    {
	return (COSMgrRef)COSMgr::indexToRef(COSMgrIndex);
    }

    static RepRef TheBreakpointObjectRef()
    {
	return theBreakpointObjectRef;
    }
    // give it a shorter name for convenience
#define TheBPRef (GOBJ(TheBreakpointObjectRef))

    static XHandleTransRef TheXHandleTransRef()
    {
	return (XHandleTransRef)COSMgr::indexToRef(XHandleTransIndex);
    }

    static TypeMgrRef TheTypeMgrRef()
    {
	return (TypeMgrRef)COSMgr::indexToRef(TypeMgrIndex);
    }

    static PageAllocatorRef ThePageAllocatorRef()
    {
	return (PageAllocatorRef)COSMgr::indexToRef(PageAllocatorIndex);
    }

    static BaseProcessRef TheProcessRef()
    {
        return (BaseProcessRef)COSMgr::indexToRef(ProcessIndex);
    }

    static SchedulerServiceRef TheSchedulerServiceRef()
    {
	return (SchedulerServiceRef)COSMgr::indexToRef(SchedulerServiceIndex);
    }

    static ProcessSetRef TheProcessSetRef()
    {
	return (ProcessSetRef)COSMgr::indexToRef(ProcessSetIndex);
    }

    static BlockedThreadQueuesRef TheBlockedThreadQueuesRef()
    {
	return (BlockedThreadQueuesRef)
               COSMgr::indexToRef(BlockedThreadQueuesIndex);
    }

    static ProcessLinuxRef TheProcessLinuxRef()
    {
        return (ProcessLinuxRef) COSMgr::indexToRef(ProcessLinuxIndex);
    }

    static ResMgrWrapperRef TheResourceManagerRef()
    {
        return (ResMgrWrapperRef)COSMgr::indexToRef(ResourceManagerIndex);
    }

    static SystemMiscWrapperRef TheSystemMiscRef()
    {
        return (SystemMiscWrapperRef)COSMgr::indexToRef(SystemMiscIndex);
    }

    static DispatcherMgrRef TheDispatcherMgrRef()
    {
        return (DispatcherMgrRef)COSMgr::indexToRef(DispatcherMgrIndex);
    }

    static MountPointMgrClientRef TheMountPointMgrRef()
    {
        return (MountPointMgrClientRef)COSMgr::indexToRef(MountPointMgrIndex);
    }
};

#endif /* #ifndef __COBJGLOBALS_H_ */
