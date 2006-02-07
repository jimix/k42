/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefaultRoot.C,v 1.6 2002/05/15 19:13:14 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/FCMDefaultRoot.H"
#include "defines/experimental.H"
#include <cobj/DTType.H>
#include "mem/FCMDataXferObj.H"
#include <sys/KernelInfo.H>

#ifdef ENABLE_FCM_SWITCHING
#include "mem/FCMDefaultMultiRepRoot.H"
#endif

/*virtual*/
SysStatus
FCMDefaultRoot::handleMiss(COSTransObject * &co, CORef ref, uval methodNum)
{
    return CObjRootSingleRep::handleMiss(co, ref, methodNum);
}

/*virtual*/
SysStatus
FCMDefaultRoot::getDataTransferExportSet(DTTypeSet *set)
{
    set->addType(DTT_FCM_DEFAULT);
    // use above for normal switching below for testing null hot swap times
    //set->addType(DTT_FCM_NULL_SWAP);
    return 0;
}

/*virtual*/
SysStatus
FCMDefaultRoot::getDataTransferImportSet(DTTypeSet *set)
{
    set->addType(DTT_FCM_DEFAULT);
    // use above for normal switching below for testing null hot swap times
    //set->addType(DTT_FCM_NULL_SWAP);
    return 0;
}

/*virtual*/
DataTransferObject *
FCMDefaultRoot::dataTransferExport(DTType dtt, VPSet dtVPSet)
{
    DataTransferObject *data = 0;

    switch (dtt) {
    case DTT_FCM_DEFAULT:
	data = new DataTransferObjectFCMDefault(dtt, (FCMDefault *)therep);
	break;
    case DTT_FCM_NULL_SWAP:
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("doing null hot swap\n");
	}
	break;
    default:
	tassert(0, err_printf("oops, unhandled export type\n"));
	break;
    }
    return (DataTransferObject *)data;
}

/*virtual*/
SysStatus
FCMDefaultRoot::dataTransferImport(DataTransferObject *dtobj,
				   DTType dtt, VPSet dtVPSet)
{
    if (dtt == DTT_FCM_NULL_SWAP) {
	return 0;
    }

    tassert(dtobj, err_printf("dtobj is NULL.\n"));

    switch (dtt) {
    case DTT_FCM_DEFAULT:
        doTransferFromDefault(((DataTransferObjectFCMDefault *)dtobj)->fcm());
        break;
    default:
	tassert(0, err_printf("FCMDefaultRoot::dataTransferImport: "
			      "unsupported type!\n"));
	break;
    }

    delete dtobj;
    return 0;
}

#ifdef ENABLE_FCM_SWITCHING
SysStatus
FCMDefaultRoot::switchImplToMultiRep(uval csize)
{
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("FCMDefaultRoot::switchImplToMultiRep initiated.\n");
    }
    
    CObjRoot *newRoot = FCMDefaultMultiRepRoot::CreateRootForSwitch(
	    ((FCMDefault *)therep)->pageList.getNumPages());
    if (newRoot == 0) return -1;

    SysStatus rc = COSMgr::switchCObj((CORef)getRef(), newRoot);
    if (_FAILURE(rc)) {
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("switchCObj(%p, newRoot) failed.\n", getRef());
	}
	delete newRoot;
    }
    return rc;
}

SysStatus
FCMDefaultRoot::switchImplToSameRep(FCMDefaultRoot *newRoot)
{
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("FCMDefaultRoot::switchImplToSameRep initiated.\n");
    }
    
    SysStatus rc = COSMgr::switchCObj((CORef)getRef(), newRoot);
    if (_FAILURE(rc)) {
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("switchCObj(%p, newRoot) failed.\n", getRef());
	}
    }

    return rc;
}
#endif

SysStatus
FCMDefaultRoot::doTransferFromDefault(FCMDefault *oldFCM)
{
    FCMDefault *myFCM = (FCMDefault *)therep;

    myFCM->frRef = oldFCM->frRef;
    myFCM->numanode = oldFCM->numanode;
    myFCM->pageable = oldFCM->pageable ? 1 : 0;
    myFCM->backedBySwap = oldFCM->backedBySwap ? 1 : 0;
    myFCM->priv = oldFCM->priv ? 1 : 0;
    myFCM->beingDestroyed = oldFCM->beingDestroyed ? 1 : 0;
    myFCM->pmRef = oldFCM->pmRef;
    myFCM->nextOffset = oldFCM->nextOffset;
    myFCM->referenceCount = oldFCM->referenceCount;

    // handle the lists
    myFCM->pageList = oldFCM->pageList;
    myFCM->regionList = oldFCM->regionList;
    myFCM->segmentHATList = oldFCM->segmentHATList;

    return 0;
}
