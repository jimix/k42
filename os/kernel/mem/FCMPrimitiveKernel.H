#ifndef __FCMPRIMITIVE_KERNEL_H_
#define __FCMPRIMITIVE_KERNEL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitiveKernel.H,v 1.6 2001/10/05 21:49:53 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A primative FCM that allocates a physical page
 * on each request.  Lock is in base class, as is list of pages.
 * **************************************************************************/

#include "mem/FCMPrimitive.H"
#include "mem/PageSet.H"


class FCMPrimitiveKernel :
    public FCMPrimitive<PageSet<AllocPinnedGlobal>, AllocPinnedGlobal> {
public:
    static SysStatus Create(FCMRef &ref);
    DEFINE_PINNEDGLOBALPADDED_NEW(FCMPrimitiveKernel);
};


#endif /* #ifndef __FCMPRIMITIVE_KERNEL_H_ */
