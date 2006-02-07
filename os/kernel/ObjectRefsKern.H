#ifdef __OBJECT_REFS_KERN_H_
<<<< base include ObjectRefsKern.H is included twice >>>>
#endif /* #ifdef __OBJECT_REFS_KERN_H_ */

#ifndef __OBJECT_REFS_KERN_H_
#define __OBJECT_REFS_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ObjectRefsKern.H,v 1.25 2005/06/15 04:49:12 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Internal references to kernel objects
 * **************************************************************************/

/* classes refered to using object refereces */

class XObj;
class HAT;
class SegmentHAT;
class PageAllocatorKern;
class PageAllocatorKernPinned;
class LMM;
class GLPList;
class SaveAreaList;
class MallocArray;
class HWInterrupt;
class PM;
class FSSwap;
class KernelInfoMgr;
class HWPerfMon;
class SyncService;

/* internal references to kernel objects */
typedef HAT 	             **HATRef;
typedef SegmentHAT           **SegmentHATRef;
typedef PageAllocatorKern    **PageAllocatorKernRef;
typedef PageAllocatorKernPinned    **PageAllocatorKernPinnedRef;
typedef LMM                  **LMMRef;
typedef GLPList		     **GLPListRef;
typedef SaveAreaList	     **SaveAreaListRef;
typedef MallocArray	     **MallocArrayRef;
typedef HWInterrupt          **HWInterruptRef;
typedef PM                   **PMRef;
typedef FSSwap               **FSSwapRef;
typedef KernelInfoMgr        **KernelInfoMgrRef;
typedef HWPerfMon            **HWPerfMonRef;
typedef SyncService          **SyncServiceRef;

#define GOBJK(OBJ)      (CObjGlobalsKern::OBJ())
#define DREFGOBJK(OBJ)  (DREF(GOBJK(OBJ)))

/* temporarily define the stuff necessary to keep moving in terms of XRefs */

typedef uval XHandle;

#endif /* #ifndef __OBJECT_REFS_KERN_H_ */
