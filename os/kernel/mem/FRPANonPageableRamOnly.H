#ifndef __FRPA_NON_PAGEABLE_RAM_ONLY_H_
#define __FRPA_NON_PAGEABLE_RAM_ONLY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPANonPageableRamOnly.H,v 1.3 2004/10/05 21:28:19 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that supports ram only files.  New page
 *                     requests are never sent to the file system.
 * **************************************************************************/

#include "FRPANonPageable.H"

class FRPANonPageableRamOnly;
typedef FRPANonPageableRamOnly **FRPANonPageableRamOnlyRef;

class FRPANonPageableRamOnly : public FRPANonPageable  __xbase(FRPA) {
protected:
    DEFINE_GLOBALPADDED_NEW(FRPANonPageableRamOnly);

 public:
    static SysStatus Create(ObjectHandle &oh, ProcessID processID,
			    ObjectHandle file,
			    uval len,
			    uval fileToken, 
			    char *name, uval namelen,
			    KernelPagingTransportRef ref);


    virtual SysStatusUval startFillPage(uval physAddr, uval objOffset);
    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __FRPA_NON_PAGEABLE_RAM_ONLY_H_ */

