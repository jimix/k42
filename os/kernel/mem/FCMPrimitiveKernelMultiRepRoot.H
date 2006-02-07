#ifndef __FCMPRIMITIVEKERNEL_MULTI_REP_ROOT_H_
#define __FCMPRIMITIVEKERNEL_MULTI_REP_ROOT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitiveKernelMultiRepRoot.H,v 1.2 2002/05/12 20:11:30 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <scheduler/Scheduler.H>
#include "mem/FCMCommonMultiRepRoot.H"
#include "mem/PageSet.H"

#define MINPAGES 128

class FCMPrimitiveKernelMultiRep;

class FCMPrimitiveKernelMultiRepRoot : 
    public FCMCommonMultiRepRoot<CObjRootMultiRepPinned,AllocPinnedGlobalPadded,
                                  AllocPinnedLocalStrict> {
    friend class FCMPrimitiveKernelMultiRep;

    SysStatus doSetPAddrAndIOComplete(uval fileOffset, uval paddr,
                                      LocalPageDescData *ld);

    FCMPrimitiveKernelMultiRepRoot() : 
        FCMCommonMultiRepRoot<CObjRootMultiRepPinned,AllocPinnedGlobalPadded,
                              AllocPinnedLocalStrict>(MINPAGES,MINPAGES)
        {}
public:
    static SysStatus Create(FCMRef &ref);

    virtual CObjRep *createRep(VPNum vp);

    DEFINE_PINNEDGLOBALPADDED_NEW(FCMPrimitiveKernelMultiRepRoot);
};

#endif  /* __FCMPRIMITIVEKERNEL_MULTI_REP_ROOT_H_ */

