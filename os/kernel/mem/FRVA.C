/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRVA.C,v 1.142 2005/01/10 15:29:08 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that obtains data via the thinwire
 * file system.
 * **************************************************************************/

#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>
#include "mem/FRVA.H"
#include "mem/FCMFile.H"
#include "mem/PageAllocatorKern.H"
#include "mem/FR.H"
#include "mem/RegionFSComm.H"
#include "meta/MetaRegionFSComm.H"
#include "proc/Process.H"
#include <stub/StubVAPageServer.H>
#include <sys/ProcessSet.H>

// convert address to a virtual address in file system's address space
/* virtual */ SysStatus
FRVA::convertAddressReadFrom(uval physAddr, uval &vaddr)
{
    SysStatus rc;
    rc = DREF(commRegion)->attachPage(physAddr, vaddr);
    passert(_SUCCESS(rc), err_printf("couldn't attach page\n"));
    return rc;
}

/* virtual */ SysStatus
FRVA::convertAddressWriteTo(uval physAddr, uval &vaddr, IORestartRequests *rr)
{
    SysStatus rc;
    rc = DREF(commRegion)->attachPagePush(physAddr, vaddr, rr);
    passert((_SUCCESS(rc)||_SCLSCD(rc) == FR::WOULDBLOCK), 
	    err_printf("couldn't attach page\n"));
    return rc;
}

SysStatus
FRVA::init(ObjectHandle fileOH,
	   uval len, uval token, RegionFSCommRef rref,
	   char *name, uval namelen,
	   KernelPagingTransportRef ref)
{
    stubFile = new StubFileHolderImp<StubVAPageServer>(fileOH);
    kptref = ref;

    filelen = len;
    fileToken = token;
    outstanding = 0;
    ohCount = 0;
    commRegion = rref;
    FRCommon::init();

    removed = 0;

#ifdef HACK_FOR_FR_FILENAMES
    (void) initFileName(name, namelen);
#endif //#ifdef HACK_FOR_FR_FILENAMES

    return (0);
}

SysStatus
FRVA::locked_getFCM(FCMRef &r)
{
    _ASSERT_HELD(lock);

    if (beingDestroyed) {
	r = FCMRef(TheBPRef);
	return -1;
    }

    if (fcmRef != 0) {
	r = fcmRef;
	return 0;
    }

    // okay, have to allocate a new fcm
    SysStatus rc = FCMFile::CreateDefault(fcmRef, (FRRef)getRef(), 1, 0);

    tassertWrn(_SUCCESS(rc), "allocation of fcm failed\n");
    r = fcmRef;
    return rc;
}

/* virtual */ SysStatus
FRVA::releaseAddress(uval vaddr)
{
    // unmap virtual address from file systems address space
    SysStatus tmprc = DREF(commRegion)->detachPage(vaddr);
    tassert( _SUCCESS(tmprc), err_printf("woops\n"));
    return 0;
}

/* virtual */ SysStatus
FRVA::_ioComplete(__in uval vaddr, __in uval fileOffset,
		       __in SysStatus rc)
{
    // err_printf("C");
    FetchAndAddSignedVolatile(&outstanding, -1);

    if (kptref) {
	    SysStatus rrc = DREF(kptref)->ioComplete();
	    tassertMsg(_SUCCESS(rrc), "?");
    }

    // unmap virtual address from file systems address space
    SysStatus tmprc = DREF(commRegion)->detachPage(vaddr);
    tassert( _SUCCESS(tmprc), err_printf("woops\n"));

    // Note - this call MUST be made without holding the lock
    return DREF(fcmRef)->ioComplete(fileOffset, rc);
}

/* static */ SysStatus
FRVA::Create(ObjectHandle &oh, ProcessID processID,
	     uval transferAddr,
	     ObjectHandle file,
	     uval len, uval fileToken,
	     char *name, uval namelen,
	     KernelPagingTransportRef ref)
{
    SysStatus rc;
    FRVARef frref;
    ProcessRef pref;
    RegionFSCommRef rref;
    TypeID type;
    FRVA *fr;

    // get process ref for calling file system
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(processID,
						   (BaseProcessRef&)pref);
    tassert(_SUCCESS(rc), err_printf("calling process can't go away??\n"));

    // get region for transfer request
    rc = DREF(pref)->vaddrToRegion(transferAddr, (RegionRef &)rref);
    tassertWrn(_SUCCESS(rc), "couldn't find region for FS\n");
    if (!_SUCCESS(rc)) return rc;

    // verify that region is of correct type
    rc = DREF(rref)->getType(type);
    tassertWrn(_SUCCESS(rc), "region doesn't support getType\n");
    if (!_SUCCESS(rc)) return rc;
    if (!MetaRegionFSComm::isBaseOf(type)) {
	tassertWrn(0, "woops, bad type of region\n");
	return _SERROR(1333, 0, EINVAL);
    }

    fr = new FRVA;
    tassert( (fr!=NULL), err_printf("alloc should never fail\n"));

    rc = fr->init(file, len, fileToken, rref, name, namelen, ref);

    tassert( _SUCCESS(rc), err_printf("woops\n"));

    frref = (FRVARef)CObjRootSingleRep::Create(fr);

    // call giveAccessInternal here to provide fileSystemAccess to
    // the file systems OH
    rc = DREF(frref)->giveAccessInternal(
	oh, processID,
	MetaFR::fileSystemAccess|MetaObj::controlAccess|MetaObj::attach,
	MetaObj::none, 0, 0);

    if (_FAILURE(rc)) return rc;

    return 0;
}

/* static */ void
FRVA::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaFRVA::init();
}
