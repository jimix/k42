#ifndef __FRVA_H_
#define __FRVA_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRVA.H,v 1.72 2004/10/29 16:30:32 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that obtains data via the thinwire
 * file system.
 * **************************************************************************/

#include "mem/FRPA.H"
#include <misc/ListSimpleLocked.H>
#include <meta/MetaFRVA.H>

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class FRVA;
class RegionFSComm;
typedef FRVA           **FRVARef;
typedef RegionFSComm   **RegionFSCommRef;

class FRVA : public FRPA {
protected:
    RegionFSCommRef commRegion;

    // FIXME: check this
    DEFINE_ACCESS_SERVER(
	FRVA, MetaFR::fileSystemAccess|MetaObj::controlAccess|MetaObj::attach,
	MetaObj::none);

    virtual SysStatus convertAddressReadFrom(uval physAddr, uval &vaddr);
    virtual SysStatus convertAddressWriteTo(uval physAddr, uval &vaddr, 
					    IORestartRequests *rr=0);
    virtual SysStatus releaseAddress(uval vaddr);

    SysStatus init(ObjectHandle fileOH,
		   uval len,uval fileToken,
		   RegionFSCommRef rref,
		   char *name, uval namelen,
		   KernelPagingTransportRef ref);

    // FIXME dilma: now that FRVA inherits from FRPA, do I need this define
    // twice?
    DEFINE_REFS(FR);
    DEFINE_GLOBALPADDED_NEW(FRVA);

    virtual SysStatus locked_getFCM(FCMRef &r);

__xprivate:
    /*
     *  Functions called by the file system
     */
    virtual SysStatus _ioComplete(__in uval vaddr, __in uval fileOffset,
				  __in SysStatus rc)
	__xa(fileSystemAccess);

 public:
    static SysStatus Create(ObjectHandle &oh, ProcessID processID,
			    uval transferAddr,
			    ObjectHandle file,
			    uval len,
			    uval fileToken,
			    char *name, uval namelen,
			    KernelPagingTransportRef ref);
    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __FRVA_H_ */
