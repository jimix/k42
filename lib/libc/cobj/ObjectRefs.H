#ifndef __OBJECT_REFS_H_
#define __OBJECT_REFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ObjectRefs.H,v 1.51 2004/07/13 13:34:08 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the types of some basic clustered objects
 * **************************************************************************/

#define DEFINE_REFTYPE_NAME(CLASS,TYPE) class CLASS; typedef CLASS **TYPE;
#define DEFINE_REFTYPE(CLASS)           DEFINE_REFTYPE_NAME(CLASS, CLASS##Ref)

DEFINE_REFTYPE_NAME(CObjRep, RepRef) // special case: RepRef, not CObjRepRef
DEFINE_REFTYPE(BlockedThreadQueues)
DEFINE_REFTYPE(COSMgr)
DEFINE_REFTYPE(DispatcherMgr)
DEFINE_REFTYPE(FileLinux)
DEFINE_REFTYPE(IO)
DEFINE_REFTYPE(MountPointMgrClient)
DEFINE_REFTYPE(NameTree)
DEFINE_REFTYPE(Obj)
DEFINE_REFTYPE(PageAllocator)
DEFINE_REFTYPE(PageAllocatorUser)
DEFINE_REFTYPE(ProcessLinux)
DEFINE_REFTYPE(ResMgrWrapper)
DEFINE_REFTYPE(SchedulerService)
DEFINE_REFTYPE(SystemMiscWrapper)
DEFINE_REFTYPE(TypeMgr)
DEFINE_REFTYPE(XHandleTrans)

/* kernel objects needed at user level */
DEFINE_REFTYPE(BaseProcess)
DEFINE_REFTYPE(FCM)
DEFINE_REFTYPE(FR)
DEFINE_REFTYPE(Process)
DEFINE_REFTYPE(ProcessSet)
DEFINE_REFTYPE(Region)
DEFINE_REFTYPE(SystemMisc)
DEFINE_REFTYPE(TestScheduler)
DEFINE_REFTYPE(TestSwitch)

#define DREF(OBJ)	(*(OBJ))	// Normal clustered object deref
#define GOBJ(OBJ)	(CObjGlobals::OBJ())
#define DREFGOBJ(OBJ)   DREF(GOBJ(OBJ))

#endif /* #ifndef __OBJECT_REFS_H_ */
