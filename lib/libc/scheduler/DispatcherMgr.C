/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherMgr.C,v 1.8 2004/07/11 21:59:25 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Keeps track of all dispatchers.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <sys/KernelInfo.H>
#include "DispatcherDefault.H"
#include "DispatcherMgr.H"

void
DispatcherMgr::init()
{
    RDNum rd;
    VPNum vp;

    lock.init();
    for (rd = 0; rd < Scheduler::RDLimit; rd++) {
	for (vp = 0; vp < Scheduler::VPLimit; vp++) {
	    dispatcher[rd][vp] = NULL;
	}
    }
    published.xhandleTable = NULL;
    published.xhandleTableLimit = 0;
}

void
DispatcherMgr::locked_publish()
{
    RDNum rd;
    VPNum vp;

    _ASSERT_HELD(lock);
    for (rd = 0; rd < Scheduler::RDLimit; rd++) {
	for (vp = 0; vp < Scheduler::VPLimit; vp++) {
	    if (dispatcher[rd][vp] != NULL) {
		((DispatcherDefault *) dispatcher[rd][vp])->published =
								published;
	    }
	}
    }
}

/*virtual*/ SysStatus
DispatcherMgr::enter(DispatcherID dspid, Dispatcher *dsp)
{
    AutoLock<LockType> al(&lock);	// locks now, unlocks on return

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);
    tassert((rd < Scheduler::RDLimit) &&
	    (vp < Scheduler::VPLimit) &&
	    (dispatcher[rd][vp] == NULL),
	    err_printf("Bad or duplicate dspid.\n"));

    dispatcher[rd][vp] = dsp;
    ((DispatcherDefault *) dsp)->published = published;

    return 0;
}

/*virtual*/ SysStatus
DispatcherMgr::publishXHandleTable(XBaseObj *table, uval limit)
{
    AutoLock<LockType> al(&lock);	// locks now, unlocks on return

    published.xhandleTable = table;
    published.xhandleTableLimit = limit;

    locked_publish();

    return 0;
}

/*virtual*/ SysStatus
DispatcherMgr::publishProgInfo(ProcessID procID, char *name)
{
    AutoLock<LockType> al(&lock);	// locks now, unlocks on return

    /*
     * Program info is stored in the Dispatcher base class, not in the
     * DispatcherDefault extension (where the "published" structure resides),
     * so we can't use the locked_publish() machinery.
     */

    for (RDNum rd = 0; rd < Scheduler::RDLimit; rd++) {
	for (VPNum vp = 0; vp < Scheduler::VPLimit; vp++) {
	    if (dispatcher[rd][vp] != NULL) {
		(dispatcher[rd][vp])->storeProgInfo(procID, name);
	    }
	}
    }

    return 0;
}

/*virtual*/ SysStatus
DispatcherMgr::postFork()
{
    VPNum vp;
    RDNum rd;

    tassertMsg(dispatcher[0][0] == extRegsLocal.dispatcher,
	       "Bad dispatcher\n");

    for (rd = 1; rd < Scheduler::RDLimit; rd++) {
	if (dispatcher[rd][0] != NULL) {
	    // FIXME:  Should reclaim dispatcher storage, including threads
	    dispatcher[rd][0] = NULL;
	}
    }

#ifndef NDEBUG
    for (vp = 1; vp < Scheduler::VPLimit; vp++) {
	for (rd = 0; rd < Scheduler::RDLimit; rd++) {
	    tassertMsg(dispatcher[rd][vp] == NULL,
		       "Unexpected dispatcher on non-zero VP.\n");
	}
    }
#endif

    ((DispatcherDefault *) extRegsLocal.dispatcher)->published = published;
    return 0;
}

/*static*/ void
DispatcherMgr::ClassInit(DispatcherID dspid)
{
    SysStatus rc;

    if (dspid == SysTypes::DSPID(0,0)) {
	DispatcherMgr *const rep = new DispatcherMgr;
	tassert(rep != NULL, err_printf("new DispatcherMgr failed\n"));
	rep->init();
	CObjRootSingleRep *const root =
	    new CObjRootSingleRep(rep, RepRef(GOBJ(TheDispatcherMgrRef)));
	tassert(root != NULL, err_printf("new CObjRootSingleRep failed\n"));
    }

    rc = DREFGOBJ(TheDispatcherMgrRef)->enter(dspid, extRegsLocal.dispatcher);
    tassert(_SUCCESS(rc), err_printf("enter failed.\n"));
}
