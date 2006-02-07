/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessWrapper.C,v 1.95 2005/08/11 20:20:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for a kernel process
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "ProcessWrapper.H"
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>
#include <sys/ProcessSet.H>
#include <stub/StubProcessSetKern.H>
#include <sys/ProcessLinuxClient.H>
#include <usr/ProgExec.H>

/* virtual */ SysStatus
ProcessWrapper::lazyReOpen(sval file, uval &type, ObjectHandle &oh,
			   char *data, uval &dataLen)
{
    return stub._lazyReOpen(file, type, oh, data, dataLen);
}

/* virtual */ SysStatus
ProcessWrapper::lazyClose(sval file)
{
    return stub._lazyClose(file);
}

/* virtual */ SysStatus
ProcessWrapper::lazyGiveAccess(sval file, uval type, ObjectHandle oh,
			       sval closeChain,
			       AccessRights match, AccessRights nomatch,
			       char *data, uval dataLen)
{
    return stub._lazyGiveAccess(file, type, oh, closeChain, match, nomatch,
				data, dataLen);
}

/* virtual */ SysStatus
ProcessWrapper::lazyCopyState(XHandle target)
{
    return stub._lazyCopyState(target);
}

SysStatus
ProcessWrapper::init(ObjectHandle oh, ProcessID myPID)
{
    stub.setOH(oh);
    pid = myPID;
    OSData = 0;
    return 0;
}

SysStatus
ProcessWrapper::init(ObjectHandle oh)
{
    SysStatus rc;
    stub.setOH(oh);
    rc = stub._getPID();
    _IF_FAILURE_RET(rc);
    pid = _SGETPID(rc);
    OSData = 0;
    return 0;
}

SysStatus
ProcessWrapper::registerCallback()
{
    SysStatus rc;
    ObjectHandle oh;

    rc = giveAccessByServer(oh, _KERNEL_PID,
			    MetaObj::controlAccess |
				MetaProcessClient::destroy |
				    MetaProcessClient::search,
			    MetaProcessClient::search);
    _IF_FAILURE_RET(rc);

    rc = stub._registerCallback(oh);
    // The callback registration might fail because of a race with
    // destruction of the underlying process.

    return rc;
}

/*static*/ void
ProcessWrapper::InitMyProcess(VPNum vp, ObjectHandle oh, ProcessID myPID,
    MemoryMgrPrimitive *pa)
{
    if (vp == 0) {
	ProcessWrapper *theProcessWrapper = new(pa) ProcessWrapper;
	theProcessWrapper->init(oh, myPID);
	new(pa) CObjRootSingleRep(theProcessWrapper,
				  (RepRef)GOBJ(TheProcessRef));

    }
}

SysStatus
ProcessWrapper::setOH(ObjectHandle oh)
{
    stub.setOH(oh);
    return 0;
}

SysStatus
ProcessWrapper::postFork(ProcessID myPID)
{
    SysStatus rc;
    // note, this is only called on ones own process, assume no process
    // wrappers replicated on fork
    tassertMsg(GOBJ(TheProcessRef) == (BaseProcessRef)getRef(),
	       "should only be called for TheProcessRef\n");
    /* Fix up the process set.  Currently, the entry for our parents
     * PID is bound to TheProcessRef.  We are overwriting the PID in
     * that wrapper, so we need to remove the old entry and add a new one.
     * There is also a wrapper for our PID not bound to TheProcessRef that
     * our parent made to create us.  This wrapper should have no xobjects
     * on its matched list.  We remove it from the ProcessSet but cant
     * destroy is now because pageable refs dont work yet.  See call
     * late in ForkChildPhase2 of crtInit for destroy.
     */

    rc = DREFGOBJ(TheProcessSetRef)->remove(pid);
    tassertMsg(_SUCCESS(rc), "didn't find parents pid in process set\n");
    rc = DREFGOBJ(TheProcessSetRef)->remove(myPID);
    pid = myPID;
    // put TheProcessRef back in as the ref for our own pid
    DREFGOBJ(TheProcessSetRef)->addEntry(pid, (BaseProcessRef)getRef());
    return 0;
}

/*static*/ SysStatus
ProcessWrapper::Create(BaseProcessRef &ref, ProcessID processID)
{
    ObjectHandle oh;
    ProcessWrapper *wrapper;
    SysStatus rc;
    TypeID procType;

    // FIXME eventually we will use type to determine the type
    //       of processWrapper to make, for now we only have one
    //       process type so we don't need it

    rc = StubProcessSetKern::RegisterPIDGetOH(oh, procType, processID);
    if (_FAILURE(rc)) goto cleanup0;

    wrapper = new ProcessWrapper;
    tassertMsg(wrapper != NULL, "woops\n");

    ref = (BaseProcessRef) CObjRootSingleRep::Create(wrapper);

    rc = wrapper->init(oh, processID);
    if (_FAILURE(rc)) goto cleanup1;

    /*
     * On this path, the process set should already have an entry (with a NULL
     * pref) for this pid.  Now install a real ref.
     */
    rc = DREFGOBJ(TheProcessSetRef)->
		installEntry(processID, (BaseProcessRef)(wrapper->getRef()));
    if (_FAILURE(rc)) goto cleanup1;

    rc = wrapper->registerCallback();
    if (_FAILURE(rc)) goto cleanup2;

    return 0;

  cleanup2:
    (void) DREFGOBJ(TheProcessSetRef)->remove(processID);
  cleanup1:
    wrapper->destroy();
  cleanup0:
    return rc;
}

/*
 * N.B. this is only used in user processes, NOT in the kernel
 */

/*static*/ void
ProcessWrapper::CreateKernelWrapper(VPNum vp)
{
    if (vp != 0) return;

    /*
     * On first processor, create a new clustered object, and register with
     * process set for acting as root for xobjects we publish to kernel
     */
    ProcessWrapper *rep = new ProcessWrapper();
    ObjectHandle oh;
    oh.initWithCommID(0,0);		// make sure calls fail
    rep->init(oh, _KERNEL_PID);

    // clustered object initialization, any rep
    CObjRootSingleRep::Create(rep);

    // register with ProcessSet
    DREFGOBJ(TheProcessSetRef)->
	    addEntry(_KERNEL_PID, (BaseProcessRef)rep->getRef());
}


/*static*/ SysStatus
ProcessWrapper::Create(BaseProcessRef &ref, uval procType, const char *name)
{
    SysStatus rc;
    ProcessID pid;
    ObjectHandle oh;
    ProcessWrapper *wrapper;

    rc = StubProcessServer::_Create(oh, PROCESS_DEFAULT, name, pid);
    if (_FAILURE(rc)) goto cleanup0;

    wrapper = new ProcessWrapper;
    tassertMsg(wrapper != NULL, "woops\n");

    ref = (BaseProcessRef) CObjRootSingleRep::Create(wrapper);

    rc = wrapper->init(oh, pid);
    if (_FAILURE(rc)) goto cleanup1;

    /*
     * On this path, the process set should not yet have an entry for this pid.
     */
    rc = DREFGOBJ(TheProcessSetRef)->
		addEntry(pid, (BaseProcessRef)(wrapper->getRef()));
    if (_FAILURE(rc)) goto cleanup1;

    rc = wrapper->registerCallback();
    if (_FAILURE(rc)) goto cleanup2;

    return 0;

  cleanup2:
    (void) DREFGOBJ(TheProcessSetRef)->remove(pid);
  cleanup1:
    wrapper->destroy();
  cleanup0:
    return rc;
}

/*static*/ SysStatus
ProcessWrapper::Create(BaseProcessRef &ref, uval procType, ObjectHandle oh)
{
    SysStatus rc;
    ProcessWrapper *wrapper;

    wrapper = new ProcessWrapper;
    tassertMsg(wrapper != NULL, "woops\n");

    ref = (BaseProcessRef) CObjRootSingleRep::Create(wrapper);

    rc = wrapper->init(oh);
    if (_FAILURE(rc)) goto cleanup1;

    /*
     * On this path, the process set should not yet have an entry for this pid.
     */
    rc = DREFGOBJ(TheProcessSetRef)->
		addEntry(wrapper->pid, (BaseProcessRef)(wrapper->getRef()));
    if (_FAILURE(rc)) goto cleanup1;

    rc = wrapper->registerCallback();
    if (_FAILURE(rc)) goto cleanup2;

    return 0;

  cleanup2:
    (void) DREFGOBJ(TheProcessSetRef)->remove(wrapper->pid);
  cleanup1:
    wrapper->destroy();
    return rc;
}

/* virtual */ SysStatus
ProcessWrapper::addMatchedXObj(XHandle xhandle)
{
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    tassert((pid != _KERNEL_PID),
	    err_printf("registering kernel: this results in a "
		       " performance bottleneck, so we removed for now\n"));
#endif
    matched.locked_add(xhandle);
    return 0;
}

/* virtual */ SysStatus
ProcessWrapper::removeMatchedXObj(XHandle xhandle)
{
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    tassert((pid != _KERNEL_PID),
	    err_printf("registering kernel: this results in a "
		       " performance bottleneck, so we removed for now\n"));
#endif
    matched.locked_remove(xhandle);
    return 0;
}

SysStatus
ProcessWrapper::waitForTermination()
{
    return stub._waitForTermination();
}

SysStatus
ProcessWrapper::kill()
{
    return stub._kill();
}

SysStatus
ProcessWrapper::regress()
{
    return stub._regress();
}

/* virtual */ SysStatus
ProcessWrapper::perfMon(__in uval action, __in uval ids)
{
    return stub._perfMon(action, ids);
}


SysStatus
ProcessWrapper::breakpoint()
{
    return stub._breakpoint();
}

SysStatus
ProcessWrapper::userHandleFault(uval vaddr, VPNum vp)
{
    return stub._userHandleFault(vaddr, vp);
}

/* virtual */ SysStatus
ProcessWrapper::preFork(ProcessID childPID)
{
    return stub._preFork(childPID);
}

/* virtual */ SysStatus
ProcessWrapper::preExec()
{
    return stub._preExec();
}

/* virtual */ SysStatus
ProcessWrapper::findRegion(uval start, RegionType::RegionElementInfo& element)
{
    return stub._findRegion(start, element);
}

/* virtual */ SysStatus
ProcessWrapper::regionDestroy(uval regionAddr)
{
    return stub._regionDestroy(regionAddr);
}

/* virtual */ SysStatus
ProcessWrapper::regionTruncate(uval start, uval length)
{
    return stub._regionTruncate(start, length);
}

SysStatus
ProcessWrapper::unmapRange(uval start, uval size)
{
    return stub._unmapRange(start, size);
}

SysStatus
ProcessWrapper::sendInterrupt(DispatcherID dspid, SoftIntr::IntrType i)
{
    return stub._sendInterrupt(dspid, i);
}

/*virtual*/ SysStatus
ProcessWrapper::destroy()
{

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    // **************************************************************
    // From here on, only one destroy can be in progress.  see
    // closeExportedXObjectList for implementation of this syncronization

    // remove all ObjRefs owned by the process
    SysStatus rc = closeMatchedXObjectList();
    tassert(_SUCCESS(rc),
	    err_printf("Free of process owned ObjRefs failed %p\n", this));

    // remove reference to us from processset - must be done after
    // closeMatchedXObjectList since XBaseObjs record ProcID's and must
    // be able to convert them to ProcessRefs.
    rc = DREFGOBJ(TheProcessSetRef)->remove(getPID());
    tassertWrn(_SUCCESS(rc), "Removing pid failed.\n");

    // tell Linux that this process is gone
    (void) DREFGOBJ(TheProcessLinuxRef)->destroyOSData(OSData);

    // schedule the object for deletion
    destroyUnchecked();

    return (0);
}

// called by kernel when process this represents is going away
/*virtual*/ SysStatus
ProcessWrapper::_destructionCallback()
{
    return destroy();
}
