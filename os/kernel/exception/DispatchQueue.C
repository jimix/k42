/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatchQueue.C,v 1.52 2005/02/16 00:06:09 mergen Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    First cut definition of the DispatchQueue for testing only
 *    Very simple "no-priority"-based single queue
 *
 *    TODO: use better queueing mechanism
 * **************************************************************************/

#include "kernIncs.H"
#include "exception/ProcessAnnex.H"
#include "exception/DispatchQueue.H"
#include "proc/Process.H"
#include "sys/Dispatcher.H"
#include "trace/traceException.h"

void
DispatchQueue::Heap::init()
{
    count = 0;
    for (uval i = 0; i < HEAP_SIZE; i++) {
	element[i].priority = 0;
	element[i].cda = NULL;
    }
}

void
DispatchQueue::Heap::clear()
{
    count = 0;
}

CPUDomainAnnex *
DispatchQueue::Heap::getHead()
{
    tassertMsg(count > 0, "Heap empty.\n");
    return element[0].cda;
}

void
DispatchQueue::Heap::deleteHead()
{
    tassertMsg(count > 1, "Heap empty or about to be empty.\n");
    count--;
    replaceHead(element[count].priority, element[count].cda);
}

void
DispatchQueue::Heap::replaceHead(uval64 priority, CPUDomainAnnex *cda)
{
    uval hole, child, alt;
    tassertMsg(count > 0, "Heap empty.\n");
    hole = 0;
    for (;;) {
	child = (2 * hole) + 1;
	alt = child + 1;
	if (alt >= count) {
	    if ((alt == count) && (priority > element[child].priority)) {
		element[hole] = element[child];
		hole = child;
	    }
	    break;
	}
	if (element[alt].priority < element[child].priority) child = alt;
	if (priority <= element[child].priority) break;
	element[hole] = element[child];
	hole = child;
    }
    element[hole].priority = priority;
    element[hole].cda = cda;
}

void
DispatchQueue::Heap::insert(uval64 priority, CPUDomainAnnex *cda)
{
    uval hole, parent;
    passertMsg(count < HEAP_SIZE, "Heap overflow.\n");
    hole = count;
    count++;
    while (hole > 0) {
	parent = (hole - 1) / 2;
	if (priority >= element[parent].priority) break;
	element[hole] = element[parent];
	hole = parent;
    }
    element[hole].priority = priority;
    element[hole].cda = cda;
}

void
DispatchQueue::Heap::remove(CPUDomainAnnex *cda)
{
    CPUDomainAnnex *newCDA;
    uval64 priority;
    tassertMsg(count > 1, "Heap empty or about to be empty.\n");

    // Shorten heap.
    count--;

    // Pick up last element for swap.
    newCDA = element[count].cda;

    // If cda was not the last element, swap newCDA for cda.
    if (newCDA != cda) {
	priority = element[count].priority;
	swap(priority, newCDA, cda);
    }
}

void
DispatchQueue::Heap::swap(uval64 priority, CPUDomainAnnex *newCDA,
			  CPUDomainAnnex *oldCDA)
{
    uval hole, parent, child, alt;
    tassertMsg(count > 0, "Heap empty.\n");

    // Find oldCDA in heap.  Can't do anything better than linear search.
    for (hole = 0; hole < count; hole++) {
	if (element[hole].cda == oldCDA) break;
    }
    tassertMsg(hole < count, "domain not in heap.\n");

    // Move the hole toward the root, if necessary.
    while (hole > 0) {
	parent = (hole - 1) / 2;
	if (priority >= element[parent].priority) break;
	element[hole] = element[parent];
	hole = parent;
    }

    // Move the hole toward the leaves, if necessary.
    for (;;) {
	child = (2 * hole) + 1;
	alt = child + 1;
	if (alt >= count) {
	    if ((alt == count) && (priority > element[child].priority)) {
		element[hole] = element[child];
		hole = child;
	    }
	    break;
	}
	if (element[alt].priority < element[child].priority) child = alt;
	if (priority <= element[child].priority) break;
	element[hole] = element[child];
	hole = child;
    }

    // Install element.
    element[hole].priority = priority;
    element[hole].cda = newCDA;
}

/*static*/ uval DispatchQueue::StatsRegionAddr = 0;
/*static*/ uval DispatchQueue::StatsRegionSize = 0;
/*static*/ uval DispatchQueue::StatsSize = 0;

void
DispatchQueue::init(VPNum vp, MemoryMgrPrimitive *memory)
{
    uval space;
    uval i;

    lastDispatchTime = exceptionLocal.kernelTimer.kernelClock.getClock();
    // Set inflationInterval to twice the maximum quantum.
    inflationInterval = SchedulerTimer::TicksPerSecond() *
			    (2 * KernelScheduler::MAX_QUANTUM_MICROSECS) /
								    1000000;
    tickScale = CPUDomainAnnex::TickScale(inflationInterval);
    inflation = 0;
    nextInflationTime = lastDispatchTime + inflationInterval;
    // FIXME - should set warningQuantum in terms of cycles, not ticks
    if (_BootInfo->onHV) {
	warningQuantum = 1000000;
    } else if (_BootInfo->onSim) {
	warningQuantum = 100000;
    } else {
	warningQuantum = 10000;
    }

    cdaBorrowersTop = -1;
    for (i = 0; i < CDA_BORROWERS_SIZE; i++) {
	cdaBorrowers[i] = NULL;
    }
    for (i = 0; i < KernelScheduler::NUM_PRIORITY_CLASSES; i++) {
	preemptedPA[i] = NULL;
    }
    heap.init();
    statInit();

    if (vp == 0) {
	StatsSize = ALIGN_UP(sizeof(KernelScheduler::Stats),
			     KernelInfo::SCacheLineSize());
	StatsRegionSize = PAGE_ROUND_UP(KernelInfo::CurPhysProcs() * StatsSize);
	memory->alloc(StatsRegionAddr, StatsRegionSize, PAGE_SIZE);
    }
    stats = (KernelScheduler::Stats *) (StatsRegionAddr + (vp * StatsSize));
    stats->init(inflationInterval);

    /*
     * We need a very-low-priority dummy CDA that will live in the heap
     * forever and keep it from becoming empty.  It should never be chosen
     * to run, but we have to build enough stuff so that it looks runnable.
     */
    dispatchedCDA = new(memory) CPUDomainAnnex;
    dispatchedCDA->init(KernelScheduler::PRIORITY_CLASS_UNRUNNABLE,
		        KernelScheduler::MIN_WEIGHT,
		        KernelScheduler::MAX_QUANTUM_MICROSECS,
			KernelScheduler::MAX_PULSE_MICROSECS);
    memory->alloc(space, sizeof(Dispatcher), PAGE_SIZE);
    Dispatcher *const dsp = (Dispatcher *) space;
    dsp->init(0);
    dsp->hasWork = 1;
    dispatchedPA = new(memory) ProcessAnnex;
    dispatchedPA->init(GOBJK(TheProcessRef), _KERNEL_PID,
		       0, 0, 0, dsp, NULL, 0, NULL, 0);
    dispatchedPA->attach(dispatchedCDA);
    /*
     * For now, the dummy CDA is the dispatched CDA, so we have to get it out
     * of the heap.  We can't use deleteHead() without triggering assertions.
     * The dummy CDA will go back in the heap when the kernel CDA is created.
     */
    tassertMsg(heap.getHead() == dispatchedCDA, "Unexpected head of heap.\n");
    heap.clear();
}

SysTime
DispatchQueue::startDispatch()
{
    SysTime now, delta;

    now = exceptionLocal.kernelTimer.kernelClock.getClock();
    if (now >= nextInflationTime) {
	inflation++;
	nextInflationTime += inflationInterval;
	if (now >= nextInflationTime) {
	    /*
	     * This shouldn't happen, but a lot of time can pass when
	     * debugging, so fix things up as best we can.
	     */
	    nextInflationTime = now + inflationInterval;
	    lastDispatchTime = now - inflationInterval;
	}
	statPublish();
    }

    delta = now - lastDispatchTime;
    lastDispatchTime = now;
    dispatchedPA->addTime(delta);
    dispatchedCDA->adjustQuantum(inflation, tickScale, delta);
    statAdvance(dispatchedCDA, delta);

    return now;
}

void
DispatchQueue::finishDispatch(SysTime now)
{
    SysTime cdaTimeout, paTimeout;

    TraceOSExceptionDispatch(
		    uval64(dispatchedCDA), dispatchedPA->commID);

    if (dispatchedPA->pulseNeeded()) {
	TraceOSExceptionPulse(dispatchedPA->commID);
	dispatchedPA->setInterrupt(SoftIntr::PULSE);
	dispatchedPA->reschedulePulse(dispatchedCDA->getPulseInterval());
    }

    cdaTimeout = dispatchedCDA->getTimeout();
    paTimeout = dispatchedPA->getTimeout();
    exceptionLocal.kernelTimer.setDispatchTime(now, MIN(cdaTimeout,paTimeout));
    resetCDABorrowers(dispatchedPA);
}

void
DispatchQueue::removeProcessAnnex(ProcessAnnex *pa)
{
    tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");

    CPUDomainAnnex * const cda = pa->getCDA();

    if (pa == dispatchedPA) {
	// Switch to kernelProcessAnnex and its domain.  It must be running.
	tassertMsg(exceptionLocal.currentProcessAnnex ==
			exceptionLocal.kernelProcessAnnex,
		   "Not in kernel process!\n");
	SysTime now;
	now = startDispatch();
	dispatchedPA = exceptionLocal.currentProcessAnnex;
	dispatchedCDA = dispatchedPA->getCDA();
	dispatchedPA->makeReady();
	dispatchedCDA->makeCurrent(dispatchedPA);
	heap.swap(cda->getPriority(), cda, dispatchedCDA);
	finishDispatch(now);
    } else {
	uval const pclass = cda->getPriorityClass();
	if (preemptedPA[pclass] == pa) {
	    preemptedPA[pclass] = NULL;
	}
    }
}

void
DispatchQueue::removeCPUDomainAnnex(CPUDomainAnnex *cda)
{
    tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");

    // This call should have been preceded by a call to removeProcessAnnex()
    // which would not have left cda as the current dispatched CDA.
    tassertMsg(cda != dispatchedCDA, "Removing dispatched CDA.\n");
    heap.remove(cda);
    cda->flushQuantum(inflation, tickScale);
    statCDANotRunnable(cda);
}

void
DispatchQueue::addCPUDomainAnnex(CPUDomainAnnex *cda)
{
    tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
    tassertMsg(cda->hasFreshQuantum(),
	       "Newly runnable CDA should have a fresh quantum.\n");

    statCDARunnable(cda);

    heap.insert(cda->getPriority(), cda);
    if ((cda == heap.getHead()) && cda->hasHigherPriorityThan(dispatchedCDA)) {
	/*
	 * cda landed at the top of the heap and has higher priority than
	 * the currently running CPU domain.  Force a reschedule the next
	 * time we leave the kernel.
	 */
	clearCDABorrowers();
    }
}

void
DispatchQueue::dispatchTimeout()
{
    clearCDABorrowers();
}

void
DispatchQueue::cleanHeap()
{
    CPUDomainAnnex *head;

    head = heap.getHead();
    while (!head->stillRunnable()) {
	head->flushQuantum(inflation, tickScale);
	statCDANotRunnable(head);
	heap.deleteHead();
	head = heap.getHead();
    }
}

/*
 * Called when the current ProcessAnnex has been put to bed cleanly and we're
 * looking for another one to run.
 */
ProcessAnnex *
DispatchQueue::getNextProcessAnnex()
{
    SysTime now;
    CPUDomainAnnex *head, *preemptedCDA;
    ProcessAnnex *pa;
    uval pclass;

    tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");

    now = startDispatch();

    cleanHeap();
    head = heap.getHead();
    if (dispatchedCDA->stillRunnable()) {
	if (head->hasHigherPriorityThan(dispatchedCDA)) {
	    heap.replaceHead(dispatchedCDA->getPriority(), dispatchedCDA);
	    dispatchedCDA = head;
	}
    } else {
	dispatchedCDA->flushQuantum(inflation, tickScale);
	statCDANotRunnable(dispatchedCDA);
	heap.deleteHead();
	dispatchedCDA = head;
    }
    dispatchedPA = dispatchedCDA->getCurrentProcessAnnex();

    pclass = dispatchedCDA->getPriorityClass();
    if (preemptedPA[pclass] != NULL) {
	if (dispatchedPA != preemptedPA[pclass]) {
	    preemptedCDA = preemptedPA[pclass]->getCDA();
	    if (dispatchedCDA != preemptedCDA) {
		heap.swap(dispatchedCDA->getPriority(), dispatchedCDA,
			  preemptedCDA);
		dispatchedCDA = preemptedCDA;
	    }
	    dispatchedPA = preemptedPA[pclass];
	}
	preemptedPA[pclass] = NULL;
    }

    pa = dispatchedPA;
    if (pa->awaitingDispatchInKernel()) {
	/*
	 * If pa is "owned" by a blocked kernel thread, we unblock the kernel
	 * thread and return the kernel's ProcessAnnex so that the thread
	 * will run.
	 */
	pa->signalDispatch();
	pa = exceptionLocal.kernelProcessAnnex;
    }

    dispatchedPA->clearWarning();

    finishDispatch(now);

    return pa;
}

void
DispatchQueue::awaitDispatch(ProcessAnnex *newPA)
{
    SysTime now;
    CPUDomainAnnex *head;

    tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
    tassertMsg(exceptionLocal.currentProcessAnnex ==
		    exceptionLocal.kernelProcessAnnex,
	       "Not in kernel process!\n");
    tassertMsg(newPA != exceptionLocal.kernelProcessAnnex,
	       "Rescheduling kernel!\n");
    tassertMsg(Scheduler::GetCurThreadPtr() == newPA->reservedThread,
	       "Rescheduling from wrong thread!\n");

    CPUDomainAnnex * const newCDA = newPA->getCDA();
    uval const newPClass = newCDA->getPriorityClass();

    now = startDispatch();
Retry:
    if (newPA->terminationPending()) {
	newPA->resumeTermination();
	// NOTREACHED
    }

    if (dispatchedPA != newPA) {
	newPA->makeReady();
	newCDA->makeCurrent(newPA);
	if (dispatchedCDA != newCDA) {
	    if (dispatchedCDA->stillRunnable()) {
		heap.swap(dispatchedCDA->getPriority(), dispatchedCDA, newCDA);
	    } else {
		dispatchedCDA->flushQuantum(inflation, tickScale);
		statCDANotRunnable(dispatchedCDA);
		heap.remove(newCDA);
	    }
	    dispatchedCDA = newCDA;
	}
	dispatchedPA = newPA;
    }

    cleanHeap();
    head = heap.getHead();

    if ((head->getPriorityClass() < newPClass) ||
	(preemptedPA[newPClass] != NULL) ||
	newPA->warningExpired())
    {
	// Hard preempt.  Switch to kernelProcessAnnex and its domain.
	dispatchedPA = exceptionLocal.currentProcessAnnex;
	dispatchedPA->makeReady();
	dispatchedCDA = dispatchedPA->getCDA();
	dispatchedCDA->makeCurrent(dispatchedPA);

	heap.swap(newCDA->getPriority(), newCDA, dispatchedCDA);

	if ((preemptedPA[newPClass] == NULL) && !newPA->warningExpired()) {
	    preemptedPA[newPClass] = newPA;
	}

	finishDispatch(now);
	newPA->waitForDispatch();
	now = startDispatch();
	goto Retry;
    }

    if (!head->hasHigherPriorityThan(newCDA) && newCDA->isCurrent(newPA)) {
	newPA->clearWarning();
    } else if (!newPA->hasWarning()) {
	newPA->setInterrupt(SoftIntr::PREEMPT);
	newPA->setWarning(warningQuantum);
    }

    finishDispatch(now);
}
