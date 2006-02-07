/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ResMgrWrapper.C,v 1.20 2004/07/11 21:59:25 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for a kernel process
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "ResMgrWrapper.H"
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubResMgr.H>
#include <mem/Access.H>
#include <stub/StubRegionDefault.H>

/*static*/ SysStatus
ResMgrWrapper::Create(ObjectHandle serverOH)
{
    ResMgrWrapper *wrapper = new ResMgrWrapper;
    tassertMsg(wrapper != NULL, "failed to create res mgr wrapper\n");

    wrapper->stub.setOH(serverOH);

    new CObjRootSingleRep(wrapper, (RepRef)GOBJ(TheResourceManagerRef));
//;;cprintf("successfully created res manager wrapper\n");
    return 0;
}

/*static*/ SysStatus
ResMgrWrapper::Create()
{
    ObjectHandle oh;
    SysStatus rc;

    ResMgrWrapper *wrapper = new ResMgrWrapper;
    tassert(wrapper != NULL, err_printf("failed to create res mgr wrapper\n"));

    rc = StubResMgr::_Create(oh);
    passertMsg(_SUCCESS(rc), "failed to create resource manager wrapper.\n");
    wrapper->stub.setOH(oh);

    new CObjRootSingleRep(wrapper, (RepRef)GOBJ(TheResourceManagerRef));
//;;cprintf("successfully created res manager wrapper\n");
    return 0;
}

/*static*/ SysStatus
ResMgrWrapper::CreateAndRegisterFirstDispatcher()
{
    ObjectHandle oh;
    SysStatus rc;

    ResMgrWrapper *wrapper = new ResMgrWrapper;
    tassert(wrapper != NULL, err_printf("failed to create res mgr wrapper\n"));

    rc = StubResMgr::_CreateAndRegisterFirstDispatcher(oh);
    passertMsg(_SUCCESS(rc), "failed to create resource manager wrapper.\n");
    wrapper->stub.setOH(oh);

    new CObjRootSingleRep(wrapper, (RepRef)GOBJ(TheResourceManagerRef));
//;;cprintf("successfully created res manager wrapper\n");
    return 0;
}

/*static*/ SysStatus
ResMgrWrapper::Create(ResMgrWrapperRef& ref)
{
    ObjectHandle oh;
    SysStatus rc;

    rc = StubResMgr::_Create(oh);
    passertMsg(_SUCCESS(rc), "failed to create resource manager wrapper.\n");

    ResMgrWrapper *wrapper = new ResMgrWrapper;
    passertMsg(wrapper != NULL, "failed to create res mgr wrapper\n");
    wrapper->stub.setOH(oh);
    new CObjRootSingleRep(wrapper);
    ref = (ResMgrWrapperRef)(wrapper->getRef());
//;;cprintf("successfully created res manager wrapper\n");
    return 0;
}

/*virtual*/ SysStatus
ResMgrWrapper::postFork()
{
    SysStatus rc;
    ObjectHandle oh;

    rc = StubResMgr::_Create(oh);

    passert(_SUCCESS(rc), err_printf("postFork ResMgrWrapper should not fail\n"));

    stub.setOH(oh);
    return 0;
}

/*virtual*/ SysStatus
ResMgrWrapper::assignDomain(uval uid)
{
//;;cprintf("WrapassignDomain\n");
    SysStatus rc;

    rc = stub._assignDomain(uid);
    return rc;
}

/*virtual*/ SysStatus
ResMgrWrapper::createFirstDispatcher(ObjectHandle childOH,
				     EntryPointDesc entry, uval dispatcherAddr,
				     uval initMsgLength, char *initMsg)
{
//;;cprintf("WrapcreateFirstDispatcher\n");
    return (stub._createFirstDispatcher(childOH, entry, dispatcherAddr,
					initMsgLength, initMsg));
}

/*virtual*/ SysStatus
ResMgrWrapper::createDispatcher(DispatcherID dspid,
				EntryPointDesc entry, uval dispatcherAddr,
				uval initMsgLength, char *initMsg)
{
//;;cprintf("WrapcreateDispatcher\n");
    return (stub._createDispatcher(dspid, entry, dispatcherAddr,
				   initMsgLength, initMsg));
}

/*virtual*/ SysStatus
ResMgrWrapper::execNotify()
{
//;;cprintf("WrapexecNotify\n");
    return (stub._execNotify());
}

/* virtual */ SysStatus
ResMgrWrapper::setStatsFlag(uval val)
{
    return (stub._setStatsFlag(val));
}

/* virtual */ SysStatus
ResMgrWrapper::toggleStatsFlag()
{
    return (stub._toggleStatsFlag());
}


// FIXME:  this interface to the res mgr is for testing and should go away.
/*virtual*/ SysStatus
ResMgrWrapper::migrateVP(VPNum vpNum, VPNum suggestedPP)
{
    return (stub._migrateVP(vpNum, suggestedPP));
}

// FIXME:  this interface should be restricted to privileged clients.
/*virtual*/ SysStatus
ResMgrWrapper::mapKernelSchedulerStats(uval &statsRegionAddr,
				       uval &statsRegionSize,
				       uval &statsSize)
{
    SysStatus rc;
    ObjectHandle statsFROH;

    rc = stub._accessKernelSchedulerStats(statsFROH,
					  statsRegionSize,
					  statsSize);
    _IF_FAILURE_RET(rc);

    rc = StubRegionDefault::_CreateFixedLenExt(
	statsRegionAddr, statsRegionSize, 0, statsFROH, 0,
	AccessMode::readUserReadSup, 0,
	RegionType::K42Region);
    
    if (_FAILURE(rc)) {
	Obj::ReleaseAccess(statsFROH);
    }

    return rc;
}
