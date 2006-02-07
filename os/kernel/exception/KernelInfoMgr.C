/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelInfoMgr.C,v 1.9 2004/07/11 21:59:26 andrewb Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "KernelInfoMgr.H"
#include <exception/ExceptionLocal.H>
#include <cobj/CObjRootSingleRep.H>
#include <sync/MPMsgMgr.H>

/*
 * On vp 0, we initialize the master copy from exception local.
 * On other vp's, we initialize exception local from the master copy
 */

/* static */ void
KernelInfoMgr::ClassInit(VPNum vp)
{
    if (vp != 0) {
	DREFGOBJK(TheKernelInfoMgrRef)->initVP(vp);
	return;
    }
    // this part called just if vp == 0
    KernelInfoMgr *ki = new KernelInfoMgr;
    CObjRootSingleRepPinned::Create(ki, (RepRef)GOBJK(TheKernelInfoMgrRef));

    ki->systemGlobal = exceptionLocal.kernelInfoPtr->systemGlobal;
    ki->vpset.addVP(vp);

    return;
}

/* virtual */ SysStatus
KernelInfoMgr::initVP(VPNum vp)
{
    lock.acquire();			// so we copy a consistent set
    //N.B. this is not a full publish - it just sets this vp
    exceptionLocal.kernelInfoPtr->systemGlobal = systemGlobal;
    vpset.addVP(vp);
    lock.release();
    return 0;
}

/* virtual */ SysStatus
KernelInfoMgr::lockAndGetPtr(KernelInfo::SystemGlobal*& sgp)
{
    lock.acquire();
    sgp = &systemGlobal;
    return 0;
}

/* virtual */ SysStatus
KernelInfoMgr::unlock()
{
    lock.release();
    return 0;
}

struct PublishRequestMsg : public MPMsgMgr::MsgSync {
    KernelInfo::SystemGlobal* sgp;
    virtual void handle() {
	//disable so values in systemGlobal are updated at once and
	//stay consistent.
	disableHardwareInterrupts();
	exceptionLocal.kernelInfoPtr->systemGlobal = *sgp;
	enableHardwareInterrupts();
	reply();
    }
};

/* virtual */ SysStatus
KernelInfoMgr::publishAndUnlock()
{
    VPNum vp, thisvp;
    thisvp = Scheduler::GetVP();
    MPMsgMgr::MsgSpace msgSpace;
    PublishRequestMsg *const msg =
	new(Scheduler::GetEnabledMsgMgr(), msgSpace)
	PublishRequestMsg;
    msg->sgp = &systemGlobal;
    
    for(vp=0;vp<Scheduler::VPLimit;vp++) {
	if(vp != thisvp && vpset.isSet(vp)) {
	    msg->send(SysTypes::DSPID(0, vp));
	}
    }

    exceptionLocal.kernelInfoPtr->systemGlobal = systemGlobal;
    lock.release();
    return 0;
}

/* static */ void
KernelInfoMgr::SetControl(uval ctrlFlags)
{
    KernelInfo::SystemGlobal *sgp;
    DREFGOBJK(TheKernelInfoMgrRef)->lockAndGetPtr(sgp);
    sgp->controlFlags = ctrlFlags;
    DREFGOBJK(TheKernelInfoMgrRef)->publishAndUnlock();
}

/* static */ void
KernelInfoMgr::SetControlBit(uval ctrlBit, uval value)
{
    KernelInfo::SystemGlobal *sgp;
    DREFGOBJK(TheKernelInfoMgrRef)->lockAndGetPtr(sgp);
    if (value) {
	sgp->controlFlags |= (uval(1) << ctrlBit);
    } else {
	sgp->controlFlags &= ~(uval(1) << ctrlBit);
    }
    DREFGOBJK(TheKernelInfoMgrRef)->publishAndUnlock();
}
