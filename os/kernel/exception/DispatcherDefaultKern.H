#ifndef __DISPATCHER_DEFAULT_KERN_H_
#define __DISPATCHER_DEFAULT_KERN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultKern.H,v 1.11 2004/04/06 21:00:38 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic scheduler functionality.
 * **************************************************************************/

#include <scheduler/DispatcherDefault.H>

class DispatcherDefaultKern : private DispatcherDefault {
    friend class ExceptionExp;
    friend class ProcessAnnex;
    friend void KernelExit(uval, uval, uval);

public:

    Thread *exceptionCreateThread() {
	Thread *thread;
	tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
	/*
	 * We're running at exception level, still in the context of some
	 * random process whose disabled status has not yet been recorded.
	 * We need to set the disabled flag to avoid triggering assertions,
	 * so we save and restore the existing disabled state.  We don't
	 * need to check for software interrupts when we restore the state
	 * because we remain hardware-disabled the whole time.
	 */
	uval disabledSave = extRegsLocal.disabled;
	extRegsLocal.disabled = 1;
	thread = createThread();
	extRegsLocal.disabled = disabledSave;
	return thread;
    }

    void exceptionFreeThread(Thread *thread) {
	tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
	tassertMsg(!thread->isActive(), "Thread active.\n");
	tassertMsg(thread->groups == 0, "Thread still in a group.\n");
	tassertMsg(thread->extensionID == 0, "Thread not a base thread.\n");
	DEBUG_CHECK_STACK(thread);
	thread->next = freeList;
	freeList = thread;
    }

    void exceptionUnblock(Thread *thread) {
	tassertMsg(!hardwareInterruptsEnabled(), "Enabled!\n");
	DEBUG_CHECK_STACK(thread);
	if (thread->isBlocked) {
	    thread->isBlocked = 0;
	    makeReady(thread);
	} else {
	    thread->wasUnblocked = 1;
	}
    }
};

extern "C" Thread *
DispatcherDefaultKern_ExceptionCreateThread(DispatcherDefaultKern *dispatcher);

/*
 * We define DISPATCHER_KERN as a type-cheated variant of DISPATCHER
 * in order to gain access to protected features of DispatcherDefault.
 */
#define DISPATCHER_KERN ((DispatcherDefaultKern *) DISPATCHER)

#endif /* #ifndef __DISPATCHER_DEFAULT_KERN_H_ */
