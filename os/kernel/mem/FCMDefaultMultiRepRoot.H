#ifndef __FCMDEFAULT_MULTI_REP_ROOT_H_
#define __FCMDEFAULT_MULTI_REP_ROOT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefaultMultiRepRoot.H,v 1.19 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic FCM for mapping a file representative.
 * **************************************************************************/

#include <scheduler/Scheduler.H>
#include "mem/FCMCommonMultiRepRoot.H"
#include "mem/PageSet.H"

class FCMDefault;
class FCMDefaultMultiRep;

class FCMDefaultMultiRepRoot :
    public FCMCommonMultiRepRoot<CObjRootMultiRep,
                                 AllocGlobalPadded,AllocLocalStrict> {
    friend class FCMDefaultMultiRep;
protected:
    uval  numanode;			// where to allocate frames from
    uval  pinnedCount;                  // number of pinned frames
    
    SysStatus init(FCMRef &ref, FRRef cr, uval pageable, uval backedBySwap);

    uval nextOffset;			// next file offset to scan
    void locked_pageScan();		// scanning for pages to free

    FCMDefaultMultiRepRoot() { nextOffset = uval(-1); }
    FCMDefaultMultiRepRoot(uval numPages) :
	FCMCommonMultiRepRoot<CObjRootMultiRep, AllocGlobalPadded,
                              AllocLocalStrict>(numPages) {
        nextOffset = uval(-1);
    }

    //FIXME: Talk to getting rid of this in the FCMDefaut. cleanup does not
    //       work this way.
#if 0
    // we can't free timer until there is no chance that the last
    // timer service thread is around - so wait for COBJ cleanup
    virtual SysStatus cleanup() {
	tassert(0, err_printf("oops\n"));
	delete timer;
	return FCMCommonMultiRepRoot<PageSet<AllocGlobal>, AllocGlobal>::cleanup();
	return 0;
    }
#endif /* #if 0 */

    SysStatus doTransferFromDefault(FCMDefault *fcmdef);

    SysStatus doFillAllAndLockLocal(uval fileOffset,
			       PageFaultNotification *skipFn,
			       LocalPageDescData *ld);

    SysStatus doIOCompleteAll(uval fileOffset, SysStatus rc);

    SysStatus doReleasePage(uval fileOffset);

    SysStatus doGetPage(uval fileOffset, uval *paddr);

    void doSetPAddr(uval fileOffset, uval paddr,
		    LocalPageDescData *ld);

    void doNotifyAllAndRemove(LocalPageDescData *pd);


public:
    static SysStatus Create(FCMRef &ref, FRRef cr, uval pageable);

    // ---------- Data-transfer-related methods ----------
    static CObjRoot *CreateRootForSwitch(uval numPages);
    virtual SysStatus getDataTransferExportSet(DTTypeSet *set);
    virtual SysStatus getDataTransferImportSet(DTTypeSet *set);
    virtual DataTransferObject *dataTransferExport(DTType dtt, VPSet dtVPSet);
    virtual SysStatus dataTransferImport(DataTransferObject *dtobj,
					 DTType dtt, VPSet dtVPSet);
    // ---------------------------------------------------

    virtual CObjRep *createRep(VPNum vp);

    virtual SysStatus detachRegion(RegionRef regRef);

    // force all modified pages back to FR
    virtual SysStatus fsync(uval force);

    DEFINE_GLOBALPADDED_NEW(FCMDefaultMultiRepRoot);

    //gcc bug workaround - N.B. can't even inline the body here gcc 2.95.3
    //see doTransferFromDefault
    DHashTableBase::AllocateStatus gccBug(uval fileOffset,
					  MasterPageDescData ** md);
};

#endif /* #ifndef __FCMDEFAULT_MULTI_REP_ROOT_H_ */
