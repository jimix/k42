/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRCRW.C,v 1.17 2005/04/15 17:39:37 dilma Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "FRCRW.H"
#include "FCMComputation.H"
#include "meta/MetaFRCRW.H"
#include <trace/traceMem.h>
#include <proc/Process.H>
#include <cobj/XHandleTrans.H>
#include <exception/KernelInfoMgr.H>

/* static */ SysStatus
FRCRW::_Create(
    __out ObjectHandle &crwFrOH, __in ObjectHandle baseFrOH,
    __CALLER_PID caller)
{
    SysStatus rc;
    ObjRef objRef;
    TypeID type;
    FRRef baseFrRef;

    rc = XHandleTrans::XHToInternal(baseFrOH.xhandle(), caller,
				    MetaObj::attach, objRef, type);
    tassertWrn(_SUCCESS(rc), "RegionDefault failed XHToInternal\n");
    _IF_FAILURE_RET(rc);

    // verify that type is FR
    if (!MetaFR::isBaseOf(type)) {
	tassertWrn(0, "object handle <%lx,%lx> not of correct type\n",
		   baseFrOH.commID(), baseFrOH.xhandle());
	return _SERROR(1276, 0, EINVAL);
    }

    baseFrRef = (FRRef)objRef;

    FRCRW *frCrw = new FRCRW;

    if (frCrw == NULL) {
	return -1;
    }

    frCrw->init(baseFrRef, PageAllocator::LOCAL_NUMANODE);

    frCrw->pageSize = _SGETUVAL(DREF(baseFrRef)->getPageSize());
    
    // always grant read and write access to copy on write fr
    rc = frCrw->giveAccessByServer(
	crwFrOH, caller,
	MetaFRCRW::read|MetaFRCRW::write|MetaObj::controlAccess|MetaObj::attach,
	MetaFRCRW::none);
    if (_FAILURE(rc)) {
	// if we can't return an object handle with object can never
	// be referenced.  Most likely cause is that caller has terminated
	// while this create was happening.
	frCrw->destroy();
    }
    return rc;
}

/*static*/ SysStatus
FRCRW::Create(FRRef& newFRRef, FRRef baseFRRef)
{
    FRCRW *frCrw = new FRCRW;

    if (frCrw == NULL) {
	return -1;
    }

    frCrw->init(baseFRRef, PageAllocator::LOCAL_NUMANODE);
    newFRRef = (FRRef)(frCrw->getRef());
    return 0;
}    

void
FRCRW::init(FRRef baseFR, uval _numanode)
{
    //N.B. this call creates the ref
    FRComputation::init(_numanode);
    FCMComputation::Create(
	fcmRef, (FRRef)getRef(), baseFR, pageSize, pageable);
}

void
FRCRW::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaFRCRW::init();
}

