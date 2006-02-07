/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitiveKernel.C,v 1.12 2004/07/08 17:15:37 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: zero fill memory FCM - not attached to an FR
 *                   : kernel version is pinned
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKern.H"
#include "mem/FCMPrimitiveKernel.H"
#include "mem/FCMPrimitiveKernelMultiRep.H"
#include "mem/PM.H"
#include <trace/traceMem.h>
#include <cobj/CObjRootSingleRep.H>
#include <sys/KernelInfo.H>

/* static */ SysStatus
FCMPrimitiveKernel::Create(FCMRef &ref)
{
    if (KernelInfo::ControlFlagIsSet(KernelInfo::DONT_DISTRIBUTE_PRMTV_FCM)) {
	FCMPrimitiveKernel *fcm;

	fcm = new FCMPrimitiveKernel;
	if (fcm == NULL) return -1;

	ref = (FCMRef)CObjRootSingleRepPinned::Create(fcm);
	TraceOSMemFCMPrimitiveKernelCreate((uval)ref);
    } else {
	FCMPrimitiveKernelMultiRep::Create(ref);
    }
    return 0;
}
