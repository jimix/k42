/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessSetKern.C,v 1.18 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sync/atomic.h>
#include <sys/BaseProcess.H>
#include <cobj/CObjRootSingleRep.H>
#include "ProcessSetKern.H"
#include "meta/MetaProcessSetKern.H"
#include <cobj/XHandleTrans.H>
#include "ProcessServer.H"

void
ProcessSetKern::ClassInit(VPNum vp)
{
    if(vp!=0) return;

    MetaProcessSetKern::init();

    ProcessSetKern *ps = new ProcessSetKern();
    CObjRootSingleRepPinned::Create(ps, (RepRef)GOBJ(TheProcessSetRef));

    /*
     * The process I am in is not registered yet, since
     * we didn't have paged memory to initialize ProcessSet then,
     * so we do it here.
     */
    DREFGOBJ(TheProcessSetRef)->addEntry(
	_SGETPID(DREFGOBJ(TheProcessRef)->getPID()), GOBJ(TheProcessRef));
}

/* virtual */ SysStatus
ProcessSetKern::getNextProcessID(ProcessID &processID)
{
    processID = FetchAndAdd(&nextProcessID, 1);

    tassert(processID <= SysTypes::PID_MASK,
	    err_printf("too many processes\n"));

    return 0;
}

// a server wants to add this processID to it's process set
// we want to find it in the kernel process set list and give the
// server back an object handle
// FIXME eventually we will use type to determine the type
//       of processWrapper to make, for now we only have one
//       process type so we don't need it
/*static*/ SysStatus
ProcessSetKern::RegisterPIDGetOH(__out ObjectHandle &oh, __out TypeID &procType,
				 __in ProcessID processID,
				 __CALLER_PID caller)
{
    SysStatus rc;
    BaseProcessRef pref;

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(processID, pref);
    if (_FAILURE(rc)) {
	cprintf("register PID failed to get ref from PID\n");
	return rc;
    }

    rc = DREF(pref)->getType(procType);
    _IF_FAILURE_RET(rc);

    //FIXME MAA restrict permissions on the oh to read
    rc = DREF(pref)->giveAccessByServer(oh, caller);

    if (_FAILURE(rc)) {
	cprintf("register PID failed to give access\n");
	return rc;
    }

    return 0;
}
