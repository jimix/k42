/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TestSwitch.C,v 1.15 2002/11/01 19:58:01 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for testing out dynamic switching
 ***************************************************************************/

#include "kernIncs.H"
#include "bilge/TestSwitch.H"
#include "meta/MetaTestSwitch.H"
#include "stub/StubTestSwitch.H"
//#include "mem/FCMDefault.H"
#include "mem/PageList.H"
#include "mem/FCMPartitionedTrivial.H"
#include "mem/FCMSharedTrivial.H"
#include "mem/RegionReplicated.H"
#include "mem/RegionDefault.H"

void
TestSwitch::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaTestSwitch::init();
}

TestSwitch::TestSwitch()
{
}

/* virtual */ SysStatus
TestSwitch::init()
{
    CObjRootSingleRep::Create(this);
    return 0;
}

/* virtual */ SysStatus
TestSwitch::storeRefs(FCMRef locFcmRef, RegionRef locRegRef,
		      ProcessRef locProcessRef, uval locRegionSize)
{
    fcmRef = locFcmRef;
    regionRef = locRegRef;
    processRef = locProcessRef;
    regionSize = locRegionSize;
#if 0
    err_printf(">>> stored fcmRef(%p) regRef(%p) procRef(%p) regionSize(%ld)...\n",
	       fcmRef, regionRef, processRef, regionSize);
#endif
    return 0;
}

/* static */ SysStatus
TestSwitch::_Create(ObjectHandle &tsOH, __CALLER_PID caller)
{
    TestSwitch *ts = new TestSwitch;

    if (ts == NULL) {
	return -1;
    }
    ts->init();

    return(ts->giveAccessByServer(tsOH, caller));
}

/*static*/ SysStatus
TestSwitch::fcmXferTrivial(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    // My ad hoc data transfer function
    // Old root is FCMSharedTrivial's root
    // New root is FCMPartitionedTrivialRoot
    PageDesc *pageDesc = 0;
    CObjRootSingleRep *const oRoot = (CObjRootSingleRep *)oldRoot;
    FCMSharedTrivial<PageList<AllocGlobal>,AllocGlobal> *const
	fcmDefRep = (FCMSharedTrivial<PageList<AllocGlobal>,AllocGlobal> *)
	oRoot->getRepOnThisVP();
    FCMPartitionedTrivial::FCMPartitionedTrivialRoot *const nRoot =
	(FCMPartitionedTrivial::FCMPartitionedTrivialRoot *)newRoot;

    //err_printf("*** FCM Xfer (Trivial)\n");

    nRoot->attachRegion(fcmDefRep->reg, fcmDefRep->pm,
			AccessMode::writeUserWriteSup);
    FCMPartitionedTrivial *const nRep =
	(FCMPartitionedTrivial *)nRoot->getRepOnThisVP();
#if 0
    uval numDescs = 0;
    pageDesc = fcmDefRep->pageList.getPageListHead();
    while (pageDesc) {
	numDescs++;
#if 1
	if (pageDesc->fileOffset != uval(-1)) {
	    err_printf("Offset = %ld\n", pageDesc->fileOffset);
	}
#endif
	pageDesc = fcmDefRep->pageList.getNext(pageDesc);
    }

    err_printf("**XferTrivial %ld pages\n", numDescs);
#endif
#if 1
    pageDesc = fcmDefRep->pageList.getFirst();
    while (pageDesc) {
	if (pageDesc->fileOffset != uval(-1)) {
	    // insert to the new cache
	    nRep->insertPageDesc(pageDesc);
	}
	pageDesc = fcmDefRep->pageList.getNext(pageDesc);
    }
#endif

    return 0;
}

#if 0
/*static*/ SysStatus
TestSwitch::fcmXfer(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    // My ad hoc, kludgy data transfer function
    // Old root is FCMDefault's root
    // New root is FCMPartitionedTrivialRoot
    PageDesc *pageDesc = 0;
    CObjRootSingleRep *const oRoot = (CObjRootSingleRep *)oldRoot;
    FCMDefault *const fcmDefRep = (FCMDefault *)oRoot->getRep(0);
    FCMPartitionedTrivial::FCMPartitionedTrivialRoot *const nRoot =
	(FCMPartitionedTrivial::FCMPartitionedTrivialRoot *)newRoot;

    //err_printf("*** FCM Xfer\n");
    RegionRef regionRef;
    FCMDefault::RegionInfo *dummy;
    if (fcmDefRep->isRegionListEmpty()) {
	err_printf("****** Xfer: region list is empty! ******\n");
	return 0; // FIXME return error?
    }
    (void)fcmDefRep->regionList.getHead(regionRef, dummy);
    tassert(regionRef && dummy, ;);

    nRoot->attachRegion(regionRef, fcmDefRep->pmRef);
    FCMPartitionedTrivial *const nRep =
	(FCMPartitionedTrivial *)nRoot->getRep(Scheduler::GetVP());
    uval numDescs = 0;
#if 0
    pageDesc = fcmDefRep->pageList.getPageListHead();
    while (pageDesc) {
	numDescs++;
	/*
	if (pageDesc->fileOffset != uval(-1)) {
	    err_printf("Offset = %ld\n", pageDesc->fileOffset);
	}*/
	pageDesc = fcmDefRep->pageList.getNext(pageDesc);
    }

    err_printf("**Xfer %ld pages\n", numDescs);
#endif
#if 1
    pageDesc = fcmDefRep->pageList.getPageListHead();
    while (pageDesc) {
	if (pageDesc->fileOffset != uval(-1)) {
	    // insert to the new cache
	    nRep->insertPageDesc(pageDesc);
	}
	pageDesc = fcmDefRep->pageList.getNext(pageDesc);
    }
#endif

    return 0;
}
#endif

/*static*/ SysStatus
TestSwitch::regionXfer(CObjRoot *oldRoot, CObjRoot *newRoot)
{
    // Old root is RegionDefault's root
    // New root is RegionReplicated::RegionReplicatedRoot
    CObjRootSingleRep *const oRoot = (CObjRootSingleRep *)oldRoot;
    RegionDefault *regDefRep =
        (RegionDefault *)oRoot->getRepOnThisVP();
    RegionReplicated::RegionReplicatedRoot *const nRoot =
	(RegionReplicated::RegionReplicatedRoot *)newRoot;

    //err_printf("*** Region Xfer (Default -> Replicated)\n");

    nRoot->regionVaddr = regDefRep->regionVaddr;
    nRoot->regionSize = regDefRep->regionSize;
    nRoot->regionSize = regDefRep->alignment;
    nRoot->hat = regDefRep->hat;
    nRoot->proc = regDefRep->proc;
    nRoot->fcm = regDefRep->fcm;
    nRoot->fileOffset = regDefRep->fileOffset;
    nRoot->access = regDefRep->access;
    nRoot->ppset = regDefRep->ppset;
    nRoot->destroyCount = 0;			     // FIXME: check with bob
    nRoot->regionState = RegionReplicated::NORMAL; // FIXME: check with bob
    // FIXME: check with bob to see if this thing needs to be copied
    //err_printf("*** RegXfer: old &requests = %p\n", &regDefRep->requests);

#if 0
    err_printf("*** Region Xfer (getRep...)\n");
    RegionReplicated *rep = newRoot->getRep(Scheduler::GetVP());
#endif

    return 0;
}

/* virtual */ SysStatus
TestSwitch::_startSwitch()
{
    // FIXME: should pass in regionSize, clpr, assoc
    //err_printf("\n<<< _startSwitch() ");
    SysStatus rc;

    CObjRoot *partitionedFCMRoot = 0;
    RegionReplicated::RegionReplicatedRoot *regionReplicatedRoot = 0;
    rc = FCMPartitionedTrivial::Create(partitionedFCMRoot, fcmRef, regionSize,
				       /* number of cache lines per rep */ 256,
				       /* associativity of each line */    4);
    tassert(_SUCCESS(rc) && partitionedFCMRoot,
	    err_printf("Replicated Trivial fcm create\n"));

    rc = COSMgr::switchCObj((CORef)fcmRef, partitionedFCMRoot/*, fcmXferTrivial*/);
    tassert(_SUCCESS(rc), ;);

    if (regionRef) {
	rc = RegionReplicated::CreateSwitchReplacement(regionReplicatedRoot,
						       regionRef);
	tassert(_SUCCESS(rc) && regionReplicatedRoot,
		err_printf("Replicated region create\n"));
	rc = COSMgr::switchCObj((CORef)regionRef, regionReplicatedRoot/*,
				  regionXfer*/);
	tassert(_SUCCESS(rc), ;);
    }
    return rc;
}
