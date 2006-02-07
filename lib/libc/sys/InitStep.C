/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InitStep.C,v 1.1 2003/10/14 17:56:05 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: An interface for registering and invoking a step of the
 *		       system initialization process.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/TypeMgr.H>
#include <cobj/BaseObj.H>
#include <stub/StubInitServer.H>
#include <meta/MetaInitStep.H>
#include "InitStep.H"
#include <cobj/CObjRootSingleRep.H>

/* static */ void
InitStep::ClassInit()
{
    MetaInitStep::init();
}

/* virtual */ SysStatus
InitStep::init(uval id)
{
    StubInitServer stub(StubObj::UNINITIALIZED);
    ObjectHandle oh;
    CObjRootSingleRep::Create(this);
    SysStatus rc = giveAccessByServer(oh, _KERNEL_PID);
    tassertMsg(_SUCCESS(rc), "Could not give access to kernel: %lx\n",rc);

    _IF_FAILURE_RET(rc);

    rc = stub._DefineAction(id, oh);

    tassertMsg(_SUCCESS(rc),
	       "Could not define system initialization action: %lx\n",rc);

    return rc;
}

/* virtual */ SysStatus
InitStep::_doAction(ObjectHandle reply, __XHANDLE xh)
{

    SysStatus rc = action();
    releaseAccess(xh);

    StubInitServer sis(StubObj::UNINITIALIZED);
    sis.setOH(reply);
    sis._complete(rc);
    return rc;
}

/*virtual*/ SysStatus
InitStep::exportedXObjectListEmpty()
{
    return destroy();
}
