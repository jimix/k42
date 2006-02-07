/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PrivilegedService.C,v 1.5 2003/12/03 15:23:10 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Kernel services exported to privileged servers.
 * **************************************************************************/

#include "kernIncs.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>
#include <meta/MetaProcessServer.H>
#include <cobj/XHandleTrans.H>
#include "exception/CPUDomainAnnex.H"
#include "exception/ExceptionLocal.H"
#include "exception/KernelInfoMgr.H"
#include <proc/kernRunProcess.H>
#include <mem/FCMStartup.H>
#include <mem/FRPlaceHolder.H>
#include "PrivilegedService.H"

/*static*/ PrivilegedService::PrivilegedServiceRoot*
		    PrivilegedService::ThePrivilegedServiceRoot = NULL;

PrivilegedService::PrivilegedServiceRoot::PrivilegedServiceRoot()
{
    /* empty body */
}

PrivilegedService::PrivilegedServiceRoot::PrivilegedServiceRoot(RepRef ref)
    : CObjRootMultiRep(ref)
{
    /* empty body */
}

/*virtual*/ CObjRep *
PrivilegedService::PrivilegedServiceRoot::createRep(VPNum vp)
{
    CObjRep *rep = (CObjRep *) new PrivilegedService;
    return rep;
}

/*static*/ void
PrivilegedService::ClassInit(VPNum vp)
{
    // all initialization on processor zero
    if (vp != 0) {
	return;
    }

    MetaPrivilegedService::init();
}

/*
 * The kernel accepts the first call to _Create() and no others.  The
 * assumption is that an initial server, launched by the kernel and running
 * before anything else can even exist, will acquire access to the
 * privileged services and can then forever mediate access to those
 * services.
 */
/*static*/ SysStatus
PrivilegedService::_Create(__out ObjectHandle &oh, __CALLER_PID caller)
{
    PrivilegedServiceRef ref;

    if (ThePrivilegedServiceRoot != NULL) {
	// Access has already been granted.  Refuse any further attempts.
	return _SERROR(2386, 0, EEXIST);
    }

    ThePrivilegedServiceRoot = new PrivilegedServiceRoot;
    passertMsg(ThePrivilegedServiceRoot != NULL,
	       "new PrivilegedServiceRoot failed.\n");
    ref = (PrivilegedServiceRef) (ThePrivilegedServiceRoot->getRef());

    return DREF(ref)->giveAccessByServer(oh, caller);
}

/*virtual*/ SysStatus
PrivilegedService::_setProcessOSData(XHandle procXH, uval data,
				     __CALLER_PID caller)
{
    SysStatus rc;
    ObjRef ref;
    TypeID type;

    rc = XHandleTrans::XHToInternal(procXH, caller, 0, ref, type);
    _IF_FAILURE_RET(rc);

    if (!MetaProcessServer::isBaseOf(type)) {
	return _SERROR(2390, 0, EINVAL);
    }
    return DREF(ProcessRef(ref))->setOSData(data);
}

/*virtual*/ SysStatus
PrivilegedService::_setTimeOfDay(uval sec, uval usec)
{
    KernelInfo::SystemGlobal* sgp;

    DREFGOBJK(TheKernelInfoMgrRef)->lockAndGetPtr(sgp);
    exceptionLocal.kernelTimer.setTOD(sgp, sec, usec);
    DREFGOBJK(TheKernelInfoMgrRef)->publishAndUnlock();

    return 0;
}

/*virtual*/ SysStatus
PrivilegedService::_launchProgram(__inbuf(*) char *name,
				  __inbuf(*) char *arg1,
				  __inbuf(*) char *arg2,
				  __in uval wait)
{
    return kernRunInternalProcess(name, arg1, arg2, wait);
}

/*static*/ SysStatus
PrivilegedService::CreateServerDispatcherLocal(DispatcherID dspid,
					       EntryPointDesc entry,
					       uval dispatcherAddr,
					       uval initMsgLength,
					       char *initMsg,
					       ProcessID caller)
{
    return exceptionLocal.serverCDA->_createDispatcher(caller, dspid, entry,
						       dispatcherAddr,
						       initMsgLength, initMsg);
}

struct PrivilegedService::CreateDispatcherMsg : MPMsgMgr::MsgSync {
    DispatcherID dspid;
    EntryPointDesc entry;
    uval dispatcherAddr;
    uval initMsgLength;
    char *initMsg;
    ProcessID caller;
    SysStatus rc;

    virtual void handle() {
	rc = CreateServerDispatcherLocal(dspid, entry, dispatcherAddr,
					 initMsgLength, initMsg, caller);
	reply();
    }
};

/*virtual*/ SysStatus
PrivilegedService::_createServerDispatcher(__in DispatcherID dspid,
					   __in EntryPointDesc entry,
					   __in uval dispatcherAddr,
					   __in uval initMsgLength,
					   __inbuf(initMsgLength)
						       char *initMsg,
					   __CALLER_PID caller)
{
    SysStatus rc;
    VPNum vp;

    vp = SysTypes::VP_FROM_DSPID(dspid);

    if (vp == Scheduler::GetVP()) {
	rc = CreateServerDispatcherLocal(dspid, entry, dispatcherAddr,
					 initMsgLength, initMsg, caller);
    } else {
	MPMsgMgr::MsgSpace msgSpace;
	CreateDispatcherMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr(), msgSpace) CreateDispatcherMsg;
	tassertMsg(msg != NULL, "message allocate failed.\n");

	msg->dspid = dspid;
	msg->entry = entry;
	msg->dispatcherAddr = dispatcherAddr;
	msg->initMsgLength = initMsgLength;
	msg->initMsg = initMsg;
	msg->caller = caller;

	rc = msg->send(SysTypes::DSPID(0, vp));
	tassertMsg(_SUCCESS(rc), "send failed\n");

	rc = msg->rc;
    }

    return rc;
}

/*virtual*/ SysStatus
PrivilegedService::_accessKernelSchedulerStats(__out ObjectHandle &statsFROH,
					       __out uval &statsRegionSize,
					       __out uval &statsSize,
					       __CALLER_PID caller)
{
    SysStatus rc;
    uval statsRegionAddr, statsRegionOffset;
    FCMRef statsFCM;
    FRRef statsFR;

    exceptionLocal.dispatchQueue.getStatsRegion(statsRegionAddr,
						statsRegionSize, statsSize);
    statsRegionOffset = PageAllocatorKernPinned::virtToReal(statsRegionAddr);

    rc = FCMStartup::Create(statsFCM, statsRegionOffset, statsRegionSize);
    passertMsg(_SUCCESS(rc), "FCMStartup::Create failed.\n");

    rc = FRPlaceHolder::Create(statsFR);
    passertMsg(_SUCCESS(rc), "RFPlaceHolder::Create failed.\n");

    rc = DREF(statsFR)->installFCM(statsFCM);
    passertMsg(_SUCCESS(rc), "fr->installFCM failed.\n");

    rc = DREF(statsFR)->giveAccessByServer(statsFROH, caller);
    passertMsg(_SUCCESS(rc), "fr->giveAccessByServer failed.\n");

    return 0;
}

/*virtual*/ SysStatus
PrivilegedService::_createCPUContainer(__out ObjectHandle& cpuContainerOH,
				       __in uval priorityClass,
				       __in uval weight,
				       __in uval quantumMicrosecs,
				       __in uval pulseMicrosecs,
				       __CALLER_PID caller)
{
    SysStatus rc;
    CPUContainerRef container;

    rc = CPUDomainAnnex::Create(container, priorityClass,
				weight, quantumMicrosecs, pulseMicrosecs);
    _IF_FAILURE_RET(rc);

    rc = DREF(container)->giveAccessByServer(cpuContainerOH, caller);
    return rc;
}


/*virtual*/ SysStatus
PrivilegedService::_pidFromProcOH(__in ObjectHandle procOH,
				  __in ProcessID parentPID,
				  __out ProcessID &pid,
				  __CALLER_PID caller)
{
    SysStatus rc;
    ObjRef oRef;
    TypeID type;
    ProcessRef pref;

    // FIXME add correct authentication - it's not really attach
    rc = XHandleTrans::XHToInternal(procOH.xhandle(), parentPID,
				    MetaObj::attach, oRef, type);

    tassertWrn(_SUCCESS(rc),
	       "pidFromProcOH failed procOH translation\n");
    _IF_FAILURE_RET(rc);

    // verify that type is correct
    if (!MetaProcessServer::isBaseOf(type)) {
	tassertWrn(0, "invalid proc OH in PrivilegedService\n");
	return _SERROR(2734, 0, EINVAL);
    }
    pref = (ProcessRef) oRef;
    pid = DREF(pref)->getPID();

    return rc;
}
