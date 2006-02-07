/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CPUDomainAnnex.C,v 1.17 2004/09/15 17:39:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines exception-level cpu scheduling machinery.
 * **************************************************************************/

#include "kernIncs.H"
#include "ExceptionLocal.H"
#include "CPUDomainAnnex.H"
#include "trace/traceException.h"
#include "proc/Process.H"
#include <sys/KernelScheduler.H>
#include <sys/ProcessSet.H>
#include <meta/MetaProcessServer.H>
#include <cobj/XHandleTrans.H>
#include <cobj/CObjRootSingleRep.H>

/*static*/ uval
CPUDomainAnnex::TickScale(SysTime inflationInterval)
{
    /*
     * For a continuously running CDA, the unweighted priority mantissa will
     * converge toward twice the inflationInterval (in ticks).  It can
     * transiently approach three times the inflationInterval but will never
     * actually reach that value.  We choose a tickScale value that will keep
     * the weighted priority mantissa from overflowing its field.
     */
    uval64 maxDrag, maxMantissa;
    uval scale;

    maxDrag = KernelScheduler::MAX_WEIGHT / KernelScheduler::MIN_WEIGHT;
    maxMantissa = maxDrag * 3 * inflationInterval;
    scale = 0;
    while ((maxMantissa >> MANTISSA_BITS) > 0) {
	scale++;
	maxMantissa >>= 1;
    }

    return scale;
}

/*static*/ SysStatus
CPUDomainAnnex::Create(CPUContainerRef &ref,
		       uval pclass, uval weight,
		       uval quantumMicrosecs, uval pulseMicrosecs)
{
    CPUDomainAnnex *cda;

    if ((pclass < KernelScheduler::PRIORITY_CLASS_KERNEL) ||
		(pclass > KernelScheduler::PRIORITY_CLASS_UNRUNNABLE)) {
	return _SERROR(2627, 0, EINVAL);
    }
    if ((quantumMicrosecs < KernelScheduler::MIN_QUANTUM_MICROSECS) ||
		(quantumMicrosecs > KernelScheduler::MAX_QUANTUM_MICROSECS)) {
	return _SERROR(2628, 0, EINVAL);
    }
    if ((pulseMicrosecs < KernelScheduler::MIN_PULSE_MICROSECS) ||
		(pulseMicrosecs > KernelScheduler::MAX_PULSE_MICROSECS)) {
	return _SERROR(2628, 0, EINVAL);
    }
    if ((weight < KernelScheduler::MIN_WEIGHT) ||
		(weight > KernelScheduler::MAX_WEIGHT)) {
	return _SERROR(2629, 0, EINVAL);
    }

    cda = new CPUDomainAnnex;
    if (cda == NULL) {
	return _SERROR(2630, 0, ENOMEM);
    }
    cda->init(pclass, weight, quantumMicrosecs, pulseMicrosecs);

    ref = (CPUContainerRef) CObjRootSingleRep::Create(cda);

    return 0;
}

void
CPUDomainAnnex::init(uval pclass, uval wght,
		     uval quantumMicrosecs, uval pulseMicrosecs)
{
    tassertMsg((KernelScheduler::PRIORITY_CLASS_KERNEL <= pclass) &&
		(pclass <= KernelScheduler::PRIORITY_CLASS_UNRUNNABLE),
	       "Bad priority class.\n");
    tassertMsg((KernelScheduler::MIN_QUANTUM_MICROSECS <= quantumMicrosecs) &&
		(quantumMicrosecs <= KernelScheduler::MAX_QUANTUM_MICROSECS),
	       "Bad quantumMicrosecs.\n");
    tassertMsg((KernelScheduler::MIN_PULSE_MICROSECS <= pulseMicrosecs) &&
		(pulseMicrosecs <= KernelScheduler::MAX_PULSE_MICROSECS),
	       "Bad pulseMicrosecs.\n");
    tassertMsg((KernelScheduler::MIN_WEIGHT <= wght) &&
		(wght <= KernelScheduler::MAX_WEIGHT),
	       "Bad weight.\n");

    priority.part.pclass = pclass;
    priority.part.exponent = 0;
    priority.part.mantissa = 0;

    consumedTicks = 0;

    quantum = SchedulerTimer::TicksPerSecond() * quantumMicrosecs / 1000000;
    pulseInterval = SchedulerTimer::TicksPerSecond() * pulseMicrosecs / 1000000;

    weight = wght;
    drag = KernelScheduler::MAX_WEIGHT / weight;

    currentPA = NULL;
    pp = Scheduler::GetVP();

    TraceOSExceptionCDAInit(
		    uval64(this), pclass,
		    quantumMicrosecs, pulseMicrosecs, weight);
}

void
CPUDomainAnnex::addProcessAnnex(ProcessAnnex *pa)
{
    exceptionLocal.dispatchQueue.statPARunnable();
    pa->resetPulse(pulseInterval);
    if (currentPA == NULL) {
	pa->cpuDomainNext = pa;
	currentPA = pa;
	exceptionLocal.dispatchQueue.addCPUDomainAnnex(this);
    } else {
	pa->cpuDomainNext = currentPA->cpuDomainNext;
	currentPA->cpuDomainNext = pa;
    }
}

void
CPUDomainAnnex::removeProcessAnnex(ProcessAnnex *pa)
{
    ProcessAnnex *prev;

    exceptionLocal.dispatchQueue.removeProcessAnnex(pa);

    tassertMsg(currentPA != NULL, "list empty.\n");
    prev = currentPA;
    while (prev->cpuDomainNext != pa) {
	prev = prev->cpuDomainNext;
        tassertMsg(prev != currentPA, "PA not in list.\n");
    }

    if (prev == pa) {
	exceptionLocal.dispatchQueue.removeCPUDomainAnnex(this);
	currentPA = NULL;
    } else {
	prev->cpuDomainNext = pa->cpuDomainNext;
	if (currentPA == pa) {
	    currentPA = pa->cpuDomainNext;
	}
    }
    pa->cpuDomainNext = NULL;
    exceptionLocal.dispatchQueue.statPANotRunnable();
}

uval
CPUDomainAnnex::stillRunnable()
{
    ProcessAnnex *pa, *next;

    /*
     * DispatchQueue won't call us if we have no potentially-runnable
     * processes in our ring.
     */
    tassertMsg(currentPA != NULL, "No process annexes.\n");

    /*
     * The domain is still runnable if its current PA is still runnable,
     * either in its own right or because it is "owned" by a blocked
     * kernel thread.
     */
    pa = currentPA;
    if (pa->stillRunnable() || pa->awaitingDispatchInKernel()) {
	return 1;
    }

    /*
     * Look at the other PAs in the ring.  We don't aggressively notice when
     * PAs become non-runnable, so we deal with them lazily at this point.
     */
    pa = currentPA->cpuDomainNext;
    while (pa != currentPA) {
	if (pa->stillRunnable() || pa->awaitingDispatchInKernel()) {
	    /*
	     * We found a runnable PA.  Remove any PAs we've skipped from the
	     * ring, make the new PA "current", and indicate that the domain
	     * is still runnable.  The old current PA remains in the ring even
	     * though we know it's not runnable.
	     */
	    currentPA->cpuDomainNext = pa;
	    currentPA = pa;
	    return 1;
	}

	/*
	 * Pa is not runnable.  Count it and clear its "next" field so that
	 * makeReady() will know to put it back in the ring when it becomes
	 * runnable again.
	 */
	next = pa->cpuDomainNext;
	pa->cpuDomainNext = NULL;
	exceptionLocal.dispatchQueue.statPANotRunnable();
	pa = next;
    }

    /*
     * There are no runnable PAs.  Mark and count the current PA as unrunnable,
     * clear the ring, and indicate that the domain is not still runnable.
     */
    currentPA->cpuDomainNext = NULL;
    exceptionLocal.dispatchQueue.statPANotRunnable();
    currentPA = NULL;
    return 0;
}

ProcessAnnex *
CPUDomainAnnex::getCurrentProcessAnnex()
{
    /*
     * This method should be called only after a successful call to
     * stillRunnable(), so we can assume that the ring is not empty and that
     * currentPA is runnable.
     */
    tassertMsg(currentPA != NULL, "No process annexes.\n");
    tassertMsg(currentPA->stillRunnable() ||
		    currentPA->awaitingDispatchInKernel(),
	       "Current process annex in domain not runnable.\n");

    return currentPA;
}

/*
 * Exported interfaces.
 */

/*virtual*/ SysStatus
CPUDomainAnnex::_setPriorityClass(__in uval pclass)
{
    if (Scheduler::GetVP() != pp) {
	return _SERROR(2631, 0, EINVAL);
    }
    if ((pclass < KernelScheduler::PRIORITY_CLASS_KERNEL) ||
		(pclass > KernelScheduler::PRIORITY_CLASS_UNRUNNABLE)) {
	return _SERROR(2632, 0, EINVAL);
    }

    InterruptState is;
    disableHardwareInterrupts(is);

    priority.part.pclass = pclass;
    if (currentPA != NULL) {
	exceptionLocal.dispatchQueue.removeCPUDomainAnnex(this);
	exceptionLocal.dispatchQueue.addCPUDomainAnnex(this);
    }

    enableHardwareInterrupts(is);

    return 0;
}

/*virtual*/ SysStatus
CPUDomainAnnex::_setWeight(__in uval wght)
{
    if (Scheduler::GetVP() != pp) {
	return _SERROR(2633, 0, EINVAL);
    }
    if ((wght < KernelScheduler::MIN_WEIGHT) ||
		    (wght > KernelScheduler::MAX_WEIGHT)) {
	return _SERROR(2634, 0, EINVAL);
    }

    InterruptState is;
    disableHardwareInterrupts(is);

    weight = wght;
    drag = KernelScheduler::MAX_WEIGHT / weight;

    enableHardwareInterrupts(is);

    return 0;
}

/*virtual*/ SysStatus
CPUDomainAnnex::_createFirstDispatcher(__in ObjectHandle childOH,
				       __in ProcessID parentPID,
				       __in EntryPointDesc entry,
				       __in uval dispatcherAddr,
				       __in uval initMsgLength,
				       __inbuf(initMsgLength) char *initMsg)
{
    SysStatus rc;
    ObjRef oRef;
    TypeID type;
    ProcessRef pref;

    // FIXME add correct authentication - it's not really attach
    rc = XHandleTrans::XHToInternal(childOH.xhandle(), parentPID,
				    MetaObj::attach, oRef, type);

    tassertWrn(_SUCCESS(rc),
	       "createFirstDispatcher failed childOH translation\n");
    _IF_FAILURE_RET(rc);

    // verify that type is cool
    if (!MetaProcessServer::isBaseOf(type)) {
	tassertWrn(0, "invalid proc OH in PrivilegedService\n");
	return _SERROR(1825, 0, EINVAL);
    }
    pref = (ProcessRef) oRef;

    return DREF(pref)->createDispatcher(this, SysTypes::DSPID(0,0),
					entry, dispatcherAddr,
					initMsgLength, initMsg);
}

/*virtual*/ SysStatus
CPUDomainAnnex::_createDispatcher(__in ProcessID pid,
				  __in DispatcherID dspid,
				  __in EntryPointDesc entry,
				  __in uval dispatcherAddr,
				  __in uval initMsgLength,
				  __inbuf(initMsgLength) char *initMsg)
{
    SysStatus rc;
    ProcessRef pref;

    if (Scheduler::GetVP() != pp) {
	return _SERROR(2635, 0, EINVAL);
    }

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, (BaseProcessRef&)pref);
    _IF_FAILURE_RET(rc);

    return DREF(pref)->createDispatcher(this, dspid, entry, dispatcherAddr,
					initMsgLength, initMsg);
}

/*virtual*/ SysStatus
CPUDomainAnnex::_detachDispatcher(__in ProcessID pid,
				  __in DispatcherID dspid)
{
    SysStatus rc;
    ProcessRef pref;

    if (Scheduler::GetVP() != pp) {
	return _SERROR(2636, 0, EINVAL);
    }

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, (BaseProcessRef&)pref);
    _IF_FAILURE_RET(rc);

    return DREF(pref)->detachDispatcher(this, dspid);
}

/*virtual*/ SysStatus
CPUDomainAnnex::_attachDispatcher(__in ProcessID pid,
				  __in DispatcherID dspid)
{
    SysStatus rc;
    ProcessRef pref;

    if (Scheduler::GetVP() != pp) {
	return _SERROR(2637, 0, EINVAL);
    }

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, (BaseProcessRef&)pref);
    _IF_FAILURE_RET(rc);

    return DREF(pref)->attachDispatcher(this, dspid);
}
