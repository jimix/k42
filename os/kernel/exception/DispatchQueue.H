#ifndef __DISPATCH_QUEUE_H_
#define __DISPATCH_QUEUE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatchQueue.H,v 1.20 2004/09/15 17:39:36 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    First cut definition of the DispatchQueue and Dispatcher Save
 * **************************************************************************/

#include "CPUDomainAnnex.H"
#include "ProcessAnnex.H"
#include "sys/KernelScheduler.H"
#include <alloc/MemoryMgrPrimitive.H>

#include <trace/traceDbg.h>

class DispatchQueue {
    friend void genConstants(void);

public:
    class Heap {
	static uval const HEAP_SIZE = 16;
	uval count;
	struct {
	    uval64 priority;
	    CPUDomainAnnex *cda;
	} element[HEAP_SIZE];
    public:
	void init();
	void clear();
	CPUDomainAnnex *getHead();
	void deleteHead();
	void replaceHead(uval64 priority, CPUDomainAnnex *cda);
	void insert(uval64 priority, CPUDomainAnnex *cda);
	void remove(CPUDomainAnnex *cda);
	void swap(uval64 priority, CPUDomainAnnex *newCDA,
		  CPUDomainAnnex *oldCDA);
    };

    struct CurrentStats {
	uval dispatchTimeAccum;
	uval runnableWeightAccum;
	uval runnableWeight;

	void init() {
	    dispatchTimeAccum = 0;
	    runnableWeightAccum = 0;
	    runnableWeight = 0;
	}

	void updateRunnableWeight(sval weightDelta) {
	    runnableWeight += weightDelta;
	}

	void advance(SysTime delta) {
	    runnableWeightAccum += (delta * runnableWeight);
	    dispatchTimeAccum += delta;
	}

	void publish(KernelScheduler::Stats::Smooth *smooth) {
	    smooth->update(dispatchTimeAccum, runnableWeightAccum);
	    dispatchTimeAccum = 0;
	    runnableWeightAccum = 0;
	}
    };

private:
    static uval const LOG_CDA_BORROWERS_SIZE = 3;
    static uval const CDA_BORROWERS_SIZE = (uval(1) << LOG_CDA_BORROWERS_SIZE);

    static uval StatsRegionAddr;
    static uval StatsRegionSize;
    static uval StatsSize;

    CPUDomainAnnex *dispatchedCDA;
    ProcessAnnex *dispatchedPA;
    SysTime lastDispatchTime;
    uval inflation;
    SysTime nextInflationTime;
    SysTime inflationInterval;
    uval tickScale;
    KernelScheduler::Stats *stats;
    CurrentStats currentStats[KernelScheduler::NUM_PRIORITY_CLASSES];
    SysTime warningQuantum;
    sval cdaBorrowersTop;
    ProcessAnnex *cdaBorrowers[CDA_BORROWERS_SIZE];
    ProcessAnnex *preemptedPA[KernelScheduler::NUM_PRIORITY_CLASSES];
    Heap heap;

public:
    void init(VPNum vp, MemoryMgrPrimitive *memory);
    SysTime startDispatch();
    void finishDispatch(SysTime now);
    void removeProcessAnnex(ProcessAnnex *pa);
    void removeCPUDomainAnnex(CPUDomainAnnex *cda);
    void addCPUDomainAnnex(CPUDomainAnnex *cda);
    void dispatchTimeout();
    void cleanHeap();
    ProcessAnnex *getNextProcessAnnex();
    void awaitDispatch(ProcessAnnex *newPA);

    uval cdaBorrowersEmpty() {
	return (cdaBorrowersTop < 0);
    }

    void clearCDABorrowers() {
	cdaBorrowersTop = -1;
    }

    void resetCDABorrowers(ProcessAnnex *pa) {
	cdaBorrowersTop = 0;
	cdaBorrowers[cdaBorrowersTop] = pa;
    }

    void pushCDABorrower(ProcessAnnex *pa) {
	cdaBorrowersTop = (cdaBorrowersTop + 1) % CDA_BORROWERS_SIZE;
	cdaBorrowers[cdaBorrowersTop] = pa;
    }

    void popCDABorrower() {
	if (cdaBorrowersTop >= 0) cdaBorrowersTop--;
    }

    ProcessAnnex *currentCDABorrower() {
	return (cdaBorrowersTop < 0) ? NULL : cdaBorrowers[cdaBorrowersTop];
    }

    void getStatsRegion(uval &statsRegionAddr,
			uval &statsRegionSize,
			uval &statsSize) {
	statsRegionAddr = StatsRegionAddr;
	statsRegionSize = StatsRegionSize;
	statsSize = StatsSize;
    }

    void statCDARunnable(CPUDomainAnnex *cda) {
	currentStats[cda->getPriorityClass()].
			    updateRunnableWeight(+ sval(cda->getWeight()));
	stats->incRunnableCPUDomainCount();
    }

    void statCDANotRunnable(CPUDomainAnnex *cda) {
	currentStats[cda->getPriorityClass()].
			    updateRunnableWeight(- sval(cda->getWeight()));
	stats->decRunnableCPUDomainCount();
    }

    void statAdvance(CPUDomainAnnex *cda, SysTime delta) {
	currentStats[cda->getPriorityClass()].advance(delta);
    }

    void statPublish() {
	uval pclass;
	for (pclass = KernelScheduler::PRIORITY_CLASS_KERNEL;
	     pclass <= KernelScheduler::PRIORITY_CLASS_IDLE;
	     pclass++)
	{
	    currentStats[pclass].publish(&stats->smooth[pclass]);
	}
    }

    void statInit() {
	uval pclass;
	for (pclass = KernelScheduler::PRIORITY_CLASS_KERNEL;
	     pclass <= KernelScheduler::PRIORITY_CLASS_IDLE;
	     pclass++)
	{
	    currentStats[pclass].init();;
	}
    }

    void statPARunnable() { stats->incRunnableDispatcherCount(); }
    void statPANotRunnable() { stats->decRunnableDispatcherCount(); }
};

#endif /* #ifndef __DISPATCH_QUEUE_H_ */
