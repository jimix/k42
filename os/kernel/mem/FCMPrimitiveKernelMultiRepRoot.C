/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitiveKernelMultiRepRoot.C,v 1.2 2004/07/08 17:15:37 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/

#include "kernIncs.H"
#include <trace/traceMem.h>
#include "defines/paging.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FR.H"
#include "mem/FCMPrimitiveKernelMultiRep.H"
#include "mem/FCMPrimitiveKernelMultiRepRoot.H"
#include "mem/PageFaultNotification.H"
#include "mem/PM.H"
#include "mem/PageCopy.H"
#include <sys/KernelInfo.H>

/* virtual */ CObjRep *
FCMPrimitiveKernelMultiRepRoot::createRep(VPNum vp)
{
    FCMPrimitiveKernelMultiRep *rep=new FCMPrimitiveKernelMultiRep();
    FCMPrimitiveKernelMultiRep::LHashTable *lt = rep->getLocalDHashTable();
    masterDHashTable.addLTable(vp,clustersize,lt);
    lt->setMasterDHashTable(&masterDHashTable);
    return rep;
}

/* static */ SysStatus
FCMPrimitiveKernelMultiRepRoot::Create(FCMRef &ref)
{
    FCMPrimitiveKernelMultiRepRoot *fcmroot;

    fcmroot = new FCMPrimitiveKernelMultiRepRoot();
    if (fcmroot == NULL) return -1;
    ref=(FCMRef)fcmroot->getRef();
    return 0;
}

SysStatus
FCMPrimitiveKernelMultiRepRoot::doSetPAddrAndIOComplete(uval fileOffset,
                                                        uval paddr,
                                                        LocalPageDescData *ld)
{
    // DoingIO servers as an existence lock for ld and hence we meet
    // the criteria for keeping the local lock
    masterDHashTable.doOp(fileOffset, 
                          &MasterPageDescData::doSetPAddrAndIOComplete,
			  &LocalPageDescData::doSetPAddrAndIOComplete,
			  (DHashTableBase::OpArg)paddr, ld);

    return 0;
}
