#ifndef __FRPA_NON_PAGEABLE_H_
#define __FRPA_NON_PAGEABLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPANonPageable.H,v 1.7 2004/10/05 21:28:19 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that obtains data via the thinwire
 * file system.
 * **************************************************************************/

#include "FRPA.H"

class FRPANonPageable;
typedef FRPANonPageable **FRPANonPageableRef;

class FRPANonPageable : public FRPA  __xbase(FRPA) {
protected:
    DEFINE_GLOBALPADDED_NEW(FRPANonPageable);
    virtual SysStatus locked_getFCM(FCMRef &r);

    virtual SysStatus _fsync();

 public:
    static SysStatus Create(ObjectHandle &oh, ProcessID processID,
			    ObjectHandle file,
			    uval len,
			    uval fileToken,
			    char *name, uval namelen,
			    KernelPagingTransportRef ref);
    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __FRPA_NON_PAGEABLE_H_ */

