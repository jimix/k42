/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShMemClnt.C,v 1.3 2003/07/17 19:27:42 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements shared memory transport client interface
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "ShMemClnt.H"
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubFRComputation.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

ShMemClnt::ImpHash* ShMemClnt::imports = NULL;
void
ShMemClnt::ClassInit(VPNum vp)
{
    if (vp) return;
    imports = new ShMemClnt::ImpHash;
}

SysStatus
ShMemClnt::Get(ObjectHandle oh, ShMemClntRef &smcRef)
{
    SysStatus rc = 0;
    ProcessID pid = oh.pid();
    imports->acquireLock();
    if (!imports->locked_find(pid,smcRef)) {
	rc = Create(oh, smcRef);
	if (_FAILURE(rc)) goto abort;

	imports->locked_add(pid, smcRef);
    }
abort:
    imports->releaseLock();
    return rc;
}

/* static */ SysStatus
ShMemClnt::Create(ObjectHandle oh, ShMemClntRef &ref) {
    SysStatus rc = 0;
    ShMemClnt *smc = new ShMemClnt;
    rc = smc->init(oh);

    if (_FAILURE(rc)) {
	smc->destroy();
    } else {
	ref = smc->getRef();
    }
    return rc;
}

/* virtual */ SysStatus
ShMemClnt::init(ObjectHandle oh)
{

    SysStatus rc = 0;
    lock.init();

    CObjRootSingleRep::Create(this);

    stub.setOH(oh);

    rc = stub._registerClient(frOH, size);

    _IF_FAILURE_RET(rc);

    rc=StubRegionDefault::_CreateFixedLenExt(addr, size, SEGMENT_SIZE,
					     frOH, 0,
					     AccessMode::writeUserWriteSup, 0,
					     RegionType::K42Region);

    return rc;
}


/*static*/ void
ShMemClnt::PostFork()
{
    if (!imports) {
	return;
    }
    uval restart = 0;
    ProcessID key;
    ShMemClntRef smc;
    while (imports->removeNext(key, smc, restart)) {
	DREF(smc)->forkDestroy();
	restart = 0;
    }
}


/* virtual */ SysStatus
ShMemClnt::forkDestroy()
{
    SysStatus rc;
    rc = lockIfNotClosingExportedXObjectList();
    tassertMsg(_SUCCESS(rc), "shouldn't be closing\n");
    XHandle xhandle = getHeadExportedXObjectList();
    while (xhandle != XHANDLE_NONE) {
	uval clientData;
	unlockExportedXObjectList();
	rc = XHandleTrans::Demolish(xhandle, clientData);
	rc = lockIfNotClosingExportedXObjectList();
	tassertMsg(_SUCCESS(rc), "shouldn't be closing\n");
	xhandle = getHeadExportedXObjectList();
    }
    unlockExportedXObjectList();
    return destroyUnchecked();
}

/* virtual */ SysStatus
ShMemClnt::unShare(uval ptr)
{
    AutoLock<LockType> al(&lock);
    if (ptr-addr >= size) {
	return _SERROR(2594, 0, EINVAL);
    }

    return stub._unShare(ptr - addr);
}

/* virtual */ SysStatus
ShMemClnt::offsetToAddr(uval offset, uval &ptr)
{
    if (offset >= size) {
	return _SERROR(2600, 0, EINVAL);
    }
    ptr = offset + addr;
    return 0;
}
