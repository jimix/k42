/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessSetUser.C,v 1.13 2004/07/11 21:59:25 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ProcessServer.H>
#include <sys/ProcessSet.H>
#include <sys/ProcessSetUser.H>
#include <sys/ProcessWrapper.H>
#include <cobj/CObjRootSingleRep.H>
#include <sync/BlockedThreadQueues.H>

void
ProcessSetUser::ClassInit(VPNum vp)
{
    if (vp!=0) return;

    ProcessSetUser *ps = new ProcessSetUser();

    if(KernelInfo::ControlFlagIsSet(KernelInfo::NO_NONBLOCKING_HASH)) {
	ps->processList.setUseLocking();
    }
    
    CObjRootSingleRep::Create(ps, (RepRef)GOBJ(TheProcessSetRef));

    /*
     * The process I am in is not registered yet, since
     * we didn't have paged memory to initialize ProcessSet then,
     * so we do it here.
     */
    DREFGOBJ(TheProcessSetRef)->addEntry(
	_SGETPID(DREFGOBJ(TheProcessRef)->getPID()), GOBJ(TheProcessRef));
}

SysStatus
ProcessSetUser::getRefFromPID(ProcessID pid, BaseProcessRef &pref)
{
    SysStatus rc = 0;
    uval ret;

    /*
     * N.B. add returns a uval of 1 if added, 0 if found
     * attempt to add a new entry without a pref, which
     * serves as a create wrapper lock.
     */
    pref = 0;
    if (0 == processList.add(pid, pref)) {
	// see if its real or if someone else is creating the wrapper
	if (pref != 0) goto success;
	err_printf("more than one create of same wrapper tell marc\n");
	BlockedThreadQueues::Element qe;
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(
	    &qe, (void *)this);
	FetchAndAdd(&waitingForWrapper, 1);
	while (0 == (ret=processList.add(pid, pref))) {
	    // found it again, see if pref is updated yet
	    if (pref != 0) break;
	    Scheduler::Block();
	}
	FetchAndAdd(&waitingForWrapper, uval(-1));
	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	    &qe, (void*)this);
	if (0 == ret) goto success;
    }

    /*
     * we added the entry with pref 0, so we get to make
     * the wrapper
     */

    /* create will try to create a wrapper.  If successful, it will
     * call addEntry to register the pref/pid association.
     * this call fails if the process underlying pid is already being
     * destroyed when the call is made
     */
    rc = ProcessWrapper::Create(pref, pid);

    if (waitingForWrapper) {
	DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void*)this);
    }
  success:
    tassertMsg(rc!=0 || _SGETPID(DREF(pref)->getPID()) == pid,
	       "Process ref PID (0x%lx) doesn't match requested pid (0x%lx)\n",
	       DREF(pref)->getPID(), pid);
    return rc;
}
