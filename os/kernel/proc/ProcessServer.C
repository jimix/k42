/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessServer.C,v 1.22 2005/08/22 14:12:08 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "ProcessServer.H"
#include <meta/MetaProcessServer.H>
#include <cobj/XHandleTrans.H>
#include "mem/FR.H"
#include "proc/ProcessShared.H"
#include "proc/ProcessReplicated.H"
#include <sys/ProcessSet.H>
#include <sys/ProcessClient.H>
#include "mem/HATDefault.H"
#include "mem/PMLeafChunk.H"
#include "bilge/PerfMon.H"
#include <stub/StubProcessServer.H>
#include <stub/StubProcessClient.H>
#include <defines/experimental.H>

// keeps an object handle back to the Process Wrapper that called us
// when it was establishing info about a new client
class ProcessServer::ClientData {
public:
    ObjectHandle procWrapperOH;
    DEFINE_GLOBAL_NEW(ClientData);
    void setOH (ObjectHandle oh) {
	procWrapperOH._commID = oh._commID;
	procWrapperOH._xhandle = oh._xhandle;
    }
};

void
ProcessServer::ClassInit(VPNum vp)
{
#ifdef USE_PROCESS_FACTORIES
    ProcessReplicated::Factory::ClassInit(vp);
    ProcessShared<AllocGlobalPadded>::Factory::ClassInit(vp);
#endif
    if (vp!=0) return;
    MetaProcessServer::init();
}

/* virtual */ SysStatus
ProcessServer::getType(TypeID &id)
{
    id = StubProcessServer::typeID();
    return 0;
}

SysStatus
ProcessServer::_Create(ObjectHandle &oh, uval procType, const char *name,
		       ProcessID &newProcID, __CALLER_PID caller)
{
    // have kernel assign
    ProcessRef newProcRef = 0;

    if (procType == PROCESS_DEFAULT) {
	HATRef href;
	BaseProcessRef spref;
	ProcessRef pref;
	PMRef pmref;

	SysStatus rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(
	    caller, spref);
	if (!_SUCCESS(rc)) return rc;
	pref = (ProcessRef)spref;

	rc = HATDefault::Create(href);
	tassert(_SUCCESS(rc), err_printf("woops\n"));

#ifdef PMLEAF_CHUNK_ALLOCATION
	rc = DREF_FACTORY_DEFAULT(PMLeafChunk)
	    ->create(pmref, GOBJK(ThePMRootRef));
#else 
	rc = DREF_FACTORY_DEFAULT(PMLeaf)
	    ->create(pmref, GOBJK(ThePMRootRef));
#endif
	tassert(_SUCCESS(rc), err_printf("woops\n"));

	if(!KernelInfo::ControlFlagIsSet(KernelInfo::DONT_DISTRIBUTE_PROCESS)) {
#ifdef USE_PROCESS_FACTORIES
	    rc = DREF_FACTORY_DEFAULT(ProcessReplicated)->create(newProcRef,
                                                    href, pmref, pref, name);
#else
	    rc = ProcessReplicated::Create(newProcRef, href, pmref, pref, name);
#endif
	} else {
#ifdef USE_PROCESS_FACTORIES
	    rc = DREF_FACTORY_DEFAULT(ProcessShared<AllocGlobalPadded>)->create(
                                        newProcRef, href, pmref, pref, name);
#else
	    rc = ProcessShared<AllocGlobalPadded>::Create(newProcRef, href,
                                                          pmref, pref, name);
#endif
	}

	tassert(_SUCCESS(rc),err_printf("process constr: failed rc %lx\n",rc));

	rc = DREF(newProcRef)->setRegionsBounds(USER_REGIONS_START,
						USER_REGIONS_ALLOC_START,
						USER_REGIONS_END);
	tassert(_SUCCESS(rc), err_printf("woops\n"));

	SysStatusProcessID id = DREF(newProcRef)->getPID();
	tassert(_SUCCESS(id), err_printf("woops rc %lx\n",rc));
	newProcID = _SGETPID(id);
	rc = DREF(newProcRef)->giveAccessByServer(oh, caller);
    }
    return (0);
}

/* virtual */ SysStatus
ProcessServer::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
				       AccessRights match,
				       AccessRights nomatch, TypeID type)
{
    SysStatus retvalue;
    ClientData *clientData = new ClientData();
    clientData->procWrapperOH.init();
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
			      type, (uval)clientData);
    return (retvalue);
}


/*virtual*/ SysStatus
ProcessServer::_registerCallback(__in ObjectHandle callbackOH,
				 __XHANDLE xhandle)
{
    ClientData *clientData;

    // make sure caller is providing on oh to himself for callback
    if (XHandleTrans::GetOwnerProcessID(xhandle) !=
       SysTypes::PID_FROM_COMMID(callbackOH.commID())) {
	return _SERROR(1924, 0, EACCES);
    }


    // the giveAccess in ProcessSetKern created clientData for the xobj we're
    //   giving back to the caller, set the client data of xojb
    //   to contain the objectHandle to allow us to call that
    //   processWrapper on destruction of the PID it asked us about
    clientData = (ClientData*)(XHandleTrans::GetClientData(xhandle));
    clientData->setOH(callbackOH);

    return (0);
}

#define STUB_FROM_OH(ltype, var, oh) \
    ltype var(StubObj::UNINITIALIZED); \
    var.setOH(oh);


/*static*/ void
ProcessServer::BeingFreed(XHandle xhandle)
{
    ClientData *clientData;
    clientData = (ClientData*)(XHandleTrans::GetClientData(xhandle));
    delete clientData;
}

/*
 * Override this to free the client state and do upcalls.  Note, we
 * are doing a really bizzar thing here.  When an xobject goes away (due
 * to a releaseAccess) we are telling that client that the process is
 * being destroyed.  This is bizzar because 1) it may not the process
 * being deleted, it may be a server that explicitly did a
 * _releaseAccess, and 2) in that case the upcall will fail.  We do this
 * because it allows us to depend on the loop that goes through xobjects
 * to intiate the upcall for us, i.e., we don't need a whole other set
 * of data structures.  Also, this operation will only succeed if the
 * process is being destroyed, i.e., if this server did not do an
 * explicit _releaseAccess ;-)
 */
/* virtual */ SysStatus
ProcessServer::handleXObjFree(XHandle xhandle)
{
    ClientData *clientData;
    clientData = (ClientData*)(XHandleTrans::GetClientData(xhandle));

    XHandleTrans::SetBeingFreed(xhandle, BeingFreed);

    // tell client this is going away, this is actually used to tell
    // servers to garbage collect all state specific to this process

    if (clientData->procWrapperOH.valid()) {
	SysStatus rc;
	uval interval;
	STUB_FROM_OH(StubProcessClient, stubProcClient,
		     clientData->procWrapperOH);
	/*
	 * ignore errors since this callback will fail if: 1) the
	 * upcall is to the process being destroyed, or 2) if the
	 * server has itself done the _releaseAccess but deal with
	 * async quueue full.  for now just retry here
	 *
	 * FIXME maa do we need a better retry for async full
	 *
	 * What's going on here?  We try to send the async upcall.
	 * If it fails because the target process has no room in its
	 * upcall queue, we delay and try again.  Each time, we lengthen
	 * the delay, until we get to more than 100 sec, when we quit
	 */

	interval = 200;
	do {
	    rc = stubProcClient._destructionCallback();
	    if (_SUCCESS(rc) || (_SGENCD(rc) != EBUSY)) {
		break;
	    };
	    /* we need to avoid waiting to deliver the callback
	     * to the process being destroyed - since it may
	     * never wake up again.  So check here.
	     * Do it after the first failure to improve path length
	     * of the normal case
	     */
	    if (_SGETPID(getPID()) == clientData->procWrapperOH.pid()) {
		break;
	    }
	    // err_printf("Tell Marc a process destroy callback is waiting\n");
	    Scheduler::DelayUntil(
		(interval*Scheduler::TicksPerSecond())/1000000,
		TimerEvent::relative);
	    interval = interval*10;
	    /*FIXME MAA 2004/1/8 - we should not panic because an
	     *        upcall fails but for now we want to know.
	     *        Really need a better solution for upcall retry.
	     */
	    passertMsg(interval <1500000000,
		       "could not deliver termination upcall to pid %ld (0x%lx)\n",
		       _SGETPID(getPID()) ,_SGETPID(getPID()));
	} while (interval <1500000000);
    }
    return 0;
}

/* virtual */ SysStatus
ProcessServer::_kill(__CALLER_PID caller)
{
    SysStatus rc;

    rc = kill();

    if (caller == _SGETPID(getPID())) {
	/*
	 * There's no one to reply to.  We could return anyway and let the
	 * reply fail, but we can save a few cycles by avoiding the failure.
	 */
	Scheduler::DeactivateSelf();
	Scheduler::Exit();
	/* NOTREACHED */
    }

    return rc;
}
