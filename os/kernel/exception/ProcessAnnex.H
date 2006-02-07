#ifndef __PROCESS_ANNEX_H_
#define __PROCESS_ANNEX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessAnnex.H,v 1.31 2004/04/29 17:33:54 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the exception-level kernel structure that
 *                     represents a virtual processor.
 * **************************************************************************/

#include <sys/entryPoints.H>
#include <sys/VolatileState.H>
#include <sys/Dispatcher.H>
#include <sys/ppccore.H>
#include <scheduler/Scheduler.H>
#include "KernelTimer.H"
#include "FaultNotificationMgr.H"

class SegmentTable;	// forward declaration to avoid circularity
class CPUDomainAnnex;	// forward declaration to avoid circularity
class RemoteIPCBuffer;	// forward declaration to avoid circularity

#include __MINC(ProcessAnnex.H)

struct ProcessAnnex {
    VPNum           pp;			// physical processor this annex is on
    ProcessAnnex   *cpuDomainNext;	// next in queue; NULL means !runnable
    ProcessAnnex   *ipcTargetNext;	// next annex on ipcTarget hash chain
    ProcessRef      processRef;		// backpointer to process
    CommID          commID;
    uval8           userMode;		// 1 if user-mode process
    uval8           isKernel;		// 1 if kernel process
    Dispatcher     *dispatcherUser;	// user's dispatcher pointer
    Dispatcher     *dispatcher;		// kernel's dispatcher pointer
    SegmentTable   *segmentTable;
    uval            excStateOffset;	// current offsets from dispatcher
    uval            trapStateOffset;	//     to exc-level, trap, and user
    uval            userStateOffset;	//     dispatcher save areas
    Thread         *reservedThread;	// kernel thread (if any) currently
					//     responsible for this pa
    uval            awaitingDispatch;	// reserved thread awaiting scheduling?
    ThreadID        terminator;		// thread blocked on TERMINATE
    CPUDomainAnnex *cpuDomainAnnex;	// cpu domain this annex belongs to
    SysTime         dispatchTicks;	// time this annex has accumulated
    SysTime         pulseTicks;		// virtual time of next pulse
    SysTime         warningTicks;	// virtual time of hard-preempt
    CommID          ppcTargetID;	// CommID we're waiting for
    uval            ppcThreadID;	// thread we're waiting for
    KernelTimerEvent timerEvent;	// next timer pop for this VP
    uval64          ipcRetryTargets;	// bit-vector of PA's we're waiting for
    uval64          ipcRetrySources;	// bit-vector of PA's waiting for us
    uval64          ipcDeferredRetrySources;
					// bit-vector of deferred retries
    ProcessAnnex   *ipcRetryNext;	// next PA in list of waiting sources
    uval            ipcCallsEnabled;	// IPC_CALL entry point valid?
    RemoteIPCBuffer*pendingRemoteIPC;	// remote IPC awaiting delivery
    ProcessAnnexMachine machine;
    EntryPointLauncher launcher[NUM_ENTRY_POINTS];

    /*
     * Following fields are not accessed at exception level and so don't really
     * need to be pinned.
     */
    FaultNotificationMgr fnMgr;		// list of pgflt notification objects
    FCMRef          dispatcherFCMRef;	// FCM backing the dispatcher
    uval            dispatcherOffset;	// offset of dispatcher in FCM

public:
    static const uval NO_PHYS_PROC = VPNum(-1);

    DEFINE_PINNEDGLOBALPADDED_NEW(ProcessAnnex);
    DEFINE_PRIMITIVE_NEW(ProcessAnnex);

    VolatileState *excStatePtr() {
	return (VolatileState *) (((uval) dispatcher) + excStateOffset);
    }

    void swapTrapAndExcStates() {
	uval tmp = excStateOffset;
	excStateOffset = trapStateOffset;
	trapStateOffset = tmp;
	dispatcher->_trapStateOffset = tmp;
	machine.setExcStateOffset(excStateOffset);
	machine.switchExcStateSaveArea();
    }

    void swapUserAndExcStates() {
	uval tmp = excStateOffset;
	excStateOffset = userStateOffset;
	userStateOffset = tmp;
	dispatcher->_userStateOffset = tmp;
	machine.setExcStateOffset(excStateOffset);
	machine.switchExcStateSaveArea();
    }

    void init(ProcessRef pref, ProcessID pid,
	      uval userModeArg, uval isKernelArg,
	      Dispatcher *dspUser, Dispatcher *dsp,
	      FCMRef dspFCMRef, uval dspOffset,
	      SegmentTable *segTable, DispatcherID dspid);
    
    void attach(CPUDomainAnnex *cda);
    void detach();
    uval isAttached(CPUDomainAnnex *cda) {
	return (cpuDomainAnnex == cda);
    }

    void setEntryPoint(EntryPointNumber entryPoint, EntryPointDesc entry);

    void switchProcess();
    void switchAddressSpaceUser();	// target is known to be user
    void switchAddressSpaceKernel();	// target is known to be kernel
    void switchAddressSpace();		// target may be user or kernel

    void switchContextUser() {		// target is known to be user
	switchProcess();
	machine.switchExcStateSaveArea();
	switchAddressSpaceUser();
    }

    void switchContextKernel() {	// target is known to be kernel
	switchProcess();
	machine.switchExcStateSaveArea();
	switchAddressSpaceKernel();
    }

    void switchContext() {		// target may be user or kernel
	switchProcess();
	machine.switchExcStateSaveArea();
	switchAddressSpace();
    }

    // Called for a ProcessAnnex which is no longer on the ipc queue to
    // make sure there is not a thread poised to resume this process.
    // Called hardware disabled but can enable to block.
    void waitForTerminate();

    void waitForDispatch();

    void awaitAndFreeAllNotifications() {
	fnMgr.awaitAndFreeAllNotifications();
    }

    // Releases the pinned memory backing this pa's dispatcher.
    void releaseDispatcherMemory();

    void makeReady();
    void makeNotReady();

    uval isReady() { return cpuDomainNext != NULL; }

    uval stillRunnable() {
	return (reservedThread == NULL) &&
		    (dispatcher->hasWork || dispatcher->interrupts.pending());
    }

    uval terminationPending() {
	return (terminator ^ Scheduler::NullThreadID); // pseudo-boolean
	// NOTE: '^' is probably faster than '!='.
    }

    void resumeTermination();

    uval awaitingDispatchInKernel() {
	return awaitingDispatch;
    }

    void signalDispatch();

    /*
     * Called at exception level on a ProcessAnnex that is known to be local.
     * Just sets the bit.  Doesn't worry about making the target runnable.
     */
    void setInterrupt(SoftIntr::IntrType intr) {
	tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
	(void) dispatcher->interrupts.fetchAndSet(intr);
    }

    /*
     * Called at exception level on a ProcessAnnex that is known to be local.
     * Sets the bit and makes the target runnable if necessary.
     */
    void deliverInterrupt(SoftIntr::IntrType intr) {
	tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
	SoftIntr priorInts;
	priorInts = dispatcher->interrupts.fetchAndSet(intr);
	if (!priorInts.pending()) {
	    makeReady();
	}
    }

    /*
     * Called at non-exception level on a ProcessAnnex that may or may
     * not be local.
     */
    struct MakeReadyMsg;
    void sendInterrupt(SoftIntr::IntrType intr);

    void notifyFaultCompletion(PageFaultNotification *pn);

    CPUDomainAnnex *getCDA() { return cpuDomainAnnex; }
    void addTime(SysTime delta) { dispatchTicks += delta; }

    void setWarning(SysTime warningQuantum) {
	warningTicks = dispatchTicks + warningQuantum;
    }

    void clearWarning() {
	warningTicks = SysTime(-1);
    }

    uval hasWarning() {
	return (warningTicks != SysTime(-1));
    }

    uval warningExpired() {
	return (dispatchTicks >= warningTicks);
    }

    uval pulseNeeded() {
	return (dispatchTicks >= pulseTicks);
    }

    void resetPulse(SysTime pulseInterval) {
	pulseTicks = dispatchTicks + pulseInterval;
    }

    void reschedulePulse(SysTime pulseInterval) {
	do {
	    pulseTicks += pulseInterval;
	} while (pulseTicks < dispatchTicks);
    }

    SysTime getTimeout() {
	SysTime warning, pulse;

	tassertMsg(dispatchTicks <= warningTicks, "missed warning.\n");
	tassertMsg(dispatchTicks <= pulseTicks, "missed pulse.\n");
	warning = warningTicks - dispatchTicks;
	pulse = pulseTicks - dispatchTicks;
	return MIN(warning, pulse);
    }
};

#define PPC_PRIMITIVE_MARKER ((Thread *)(-1))

#endif /* #ifndef __PROCESS_ANNEX_H_ */
