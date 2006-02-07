#ifndef __SIMPLE_THREAD_H_
#define __SIMPLE_THREAD_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simpleThread.H,v 1.15 2002/10/10 13:08:20 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple thread management code
 ***************************************************************************/

#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>

class SimpleThread {

public:
    typedef SysStatus (*function)(void *);

private:
    friend class GetRidofGCCWarning;	// gets rid of warning :-)

    SysStatus   rc;			// thread result
    volatile ThreadID joiner;		// thread waiting for join
    volatile uval     done;		// flag signaling thread is done
    function    func;			// thread function
    void       *arg;			// argument for thread

    ~SimpleThread() { /* empty body protected to prevent delete */ }
    SimpleThread() { /* empty body protected to prevent new */ }

    struct StartMsg : public MPMsgMgr::MsgAsync {
	SimpleThread *thread;

	virtual void handle() {
	    SimpleThread *const t = thread;
	    free();
	    Start(t);
	}
    };
    friend class StartMsg;

    // called on thread start side from createThread call
    static void Start(SimpleThread *thread) {
	thread->rc = thread->func(thread->arg);
	thread->done = 1;
	ThreadID j = thread->joiner;
	if (j != Scheduler::NullThreadID) {
	    Scheduler::Unblock(j);
	}
    }

public:

    DEFINE_GLOBAL_NEW(SimpleThread);

    static SimpleThread *Create(function func, void *arg,
				DispatcherID dspid = Scheduler::GetDspID()) {
	SysStatus rc;
	SimpleThread *t = new SimpleThread();
	t->func = func;
	t->arg  = arg;
	t->joiner = Scheduler::NullThreadID;
	t->done = 0;
	//err_printf("SimpleThread::Create: t=%p\n", t);
	if (dspid == Scheduler::GetDspID()) {
	    rc = Scheduler::ScheduleFunction(Scheduler::ThreadFunction(Start),
					     uval(t));
	} else {
	    StartMsg *const msg =
		new(Scheduler::GetEnabledMsgMgr()) StartMsg;
	    tassert(msg != NULL, err_printf("message allocate failed.\n"));
	    msg->thread = t;
	    rc = msg->send(dspid);
	}
	if (!_SUCCESS(rc)) {
	    delete t;
	    t = 0;
	}
	return t;
    }

    static SysStatus Join(SimpleThread *&t) {
	SysStatus val;
	t->joiner = Scheduler::GetCurThread();
	while (!t->done) {
	    //err_printf("[0x%lx] b...\n", Scheduler::GetCurThread());
	    Scheduler::Block();
	    //err_printf("[0x%lx] d.\n", Scheduler::GetCurThread());
	}
	val = t->rc;
	delete t;
	t = 0;
	return val;
    }
};

#endif /* #ifndef __SIMPLE_THREAD_H_ */
