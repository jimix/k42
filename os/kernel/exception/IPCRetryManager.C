/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IPCRetryManager.C,v 1.6 2004/07/08 17:15:35 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines IPC retry mechanism.
 * **************************************************************************/

#include "kernIncs.H"
#include "exception/ProcessAnnex.H"
#include "exception/IPCRetryManager.H"
#include "trace/traceException.h"

inline uval
IPCRetryManager::getRetryID(ProcessAnnex *pa)
{
    return (SysTypes::PID_FROM_COMMID(pa->commID) %
			Dispatcher::NUM_IPC_RETRY_IDS);
}

void
IPCRetryManager::init()
{
    for (uval i = 0; i < 64; i++) {
	headSource[i] = NULL;
    }
}

void
IPCRetryManager::requestNotification(ProcessAnnex *source,
				     ProcessAnnex *target)
{
    uval sourceBit, targetBit;

    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCRetryManager::requestNotification: Enabled!\n");

    TraceOSExceptionReqIPCRetryNotif(target->commID);
    sourceBit = getRetryID(source);
    if (target->reservedThread != NULL) {
	target->ipcRetrySources |= (uval64(1) << sourceBit);
	// Because target is disabled, we don't need to do more than set
	// its preempt request bit.
	(void) target->dispatcher->interrupts.fetchAndSet(SoftIntr::PREEMPT);
    } else {
	// IPC is being refused because the entry point is NULL.  Defer the
	// retry until the entry point is initialized.
	target->ipcDeferredRetrySources |= (uval64(1) << sourceBit);
    }

    if (source->ipcRetryTargets == 0) {
	source->ipcRetryNext = headSource[sourceBit];
	headSource[sourceBit] = source;
    }
    targetBit = getRetryID(target);
    source->ipcRetryTargets |= (uval64(1) << targetBit);
    source->dispatcher->ipcRetryID = targetBit;
}

void
IPCRetryManager::internal_notify(uval64 sources, uval64 targets)
{
    uval sourceBit;
    uval64 matches;
    ProcessAnnex *source, *prev;

    sourceBit = 0;
    while (sources != 0) {
	if ((sources & 0xffffffff) == 0) {sources >>= 32; sourceBit += 32;}
	if ((sources &     0xffff) == 0) {sources >>= 16; sourceBit += 16;}
	if ((sources &       0xff) == 0) {sources >>=  8; sourceBit +=  8;}
	if ((sources &        0xf) == 0) {sources >>=  4; sourceBit +=  4;}
	if ((sources &        0x3) == 0) {sources >>=  2; sourceBit +=  2;}
	if ((sources &        0x1) == 0) {sources >>=  1; sourceBit +=  1;}
	prev = NULL;
	source = headSource[sourceBit];
	while (source != NULL) {
	    matches = (source->ipcRetryTargets & targets);
	    if (matches != 0) {
		TraceOSExceptionIPCRetryNotify(
				source->commID);
		source->dispatcher->ipcRetry |= matches;
		source->deliverInterrupt(SoftIntr::IPC_RETRY_NOTIFY);
		source->ipcRetryTargets &= ~matches;
		if (source->ipcRetryTargets == 0) {
		    // remove source from the list
		    source = source->ipcRetryNext;
		    if (prev == NULL) {
			headSource[sourceBit] = source;
		    } else {
			prev->ipcRetryNext = source;
		    }
		    continue;	// we've already advanced to the next source
		}
	    }
	    prev = source;
	    source = source->ipcRetryNext;
	}
	sources >>= 1;
	sourceBit++;
    }
}

void
IPCRetryManager::notify(ProcessAnnex *target)
{
    uval64 sources, targets;

    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCRetryManager::notify: Enabled!\n");

    sources = target->ipcRetrySources;
    target->ipcRetrySources = 0;

    targets = uval64(1) << getRetryID(target);

    internal_notify(sources, targets);
}

// FIXME:  This remoteIPC retry mechanism is a hack.  We simply keep all
// the ProcessAnnexes that have pending outgoing remote IPCs in the normal
// IPC retry lists, and we notify all of them whenever any ProcessAnnex
// yields.  The mechanism should be both more timely and more precise.
// To be more timely, it should be driven by the consumption of remote
// IPC buffers on other processors.  To be more precise, it should notify
// only the IPC sources whose targets (or at least whose targets'
// processors) have had a state change making successful remote IPC likely.

void
IPCRetryManager::requestNotificationRemote(ProcessAnnex *source,
					   CommID targetID)
{
    uval sourceBit, targetBit;

    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCRetryManager::requestNotificationRemote: Enabled!\n");

    TraceOSExceptionReqIPCRetryNotif(targetID);
    sourceBit = getRetryID(source);

    remoteIPCRetrySources |= (uval64(1) << sourceBit);

    if (source->ipcRetryTargets == 0) {
	source->ipcRetryNext = headSource[sourceBit];
	headSource[sourceBit] = source;
    }

    targetBit = REMOTE_IPC_RETRY_ID;
    source->ipcRetryTargets |= (uval64(1) << targetBit);
    source->dispatcher->ipcRetryID = targetBit;
}

void
IPCRetryManager::notifyRemote()
{
    uval64 sources, targets;

    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCRetryManager::notifyRemote: Enabled!\n");

    sources = remoteIPCRetrySources;
    remoteIPCRetrySources = 0;

    targets = uval64(1) << REMOTE_IPC_RETRY_ID;

    internal_notify(sources, targets);
}

void
IPCRetryManager::remove(ProcessAnnex *pa)
{
    ProcessAnnex *source, *prev;
    uval sourceBit;

    tassertMsg(!hardwareInterruptsEnabled(),
	       "IPCRetryManager::remove: Enabled!\n");

    if (pa->ipcRetrySources != 0) {
	notify(pa);
    }

    if (pa->ipcRetryTargets != 0) {
	pa->ipcRetryTargets = 0;

	sourceBit = getRetryID(pa);
	prev = NULL;
	source = headSource[sourceBit];
	while (source != pa) {
	    tassertMsg(source != NULL, "ProcessAnnex %p not found\n", pa);
	    prev = source;
	    source = source->ipcRetryNext;
	}
	if (prev != NULL) {
	    prev->ipcRetryNext = source->ipcRetryNext;
	} else {
	    headSource[sourceBit] = source->ipcRetryNext;
	}
    }
}

/*static*/ uval64
IPCRetryManager::GetIPCRetryIDs(ProcessAnnex *pa)
{
    return pa->ipcRetryTargets;
}
