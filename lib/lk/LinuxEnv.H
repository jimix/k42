#ifndef __LINUXENV_H_
#define __LINUXENV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxEnv.H,v 1.7 2004/09/17 12:54:25 mostrows Exp $
 *****************************************************************************/
#include <scheduler/Scheduler.H>
#include <misc/AutoList.H>
extern "C" {
#include <linux/thread_info.h>
}

#ifndef NDEBUG
#define LINUXENV_DEBUG
#endif

typedef enum { Interrupt=0x1,
	       SoftIRQ=0x2,
	       SysCall=0x4,
	       LongThread=0x8,//Thread is special - means that it is long-lived
	       Undefined=0x10,
} ThreadMode;
class Dispatcher;
struct LinuxEnv {
    struct task_struct *task;
    ThreadMode mode;
    void* oldAttachment;
    uval oldTVal;
    uval msgVal; // for sending info to the thread (e.g. kthread_stop())
    uval suspendGroups;
#ifdef LINUXENV_DEBUG
    uval oldCount;
    uval oldGroups;
    uval bars;
    Dispatcher *disp;
    struct DebugLinuxEnv:public AutoListNode {
	LinuxEnv* le;
	Thread* thr;
	DebugLinuxEnv(LinuxEnv* l):le(l),thr(Scheduler::GetCurThreadPtr()) {};
    };
    DebugLinuxEnv attach;
#endif /* LINUXENV_DEBUG */
    LinuxEnv(ThreadMode m = SysCall, struct task_struct *task= NULL);
    void destroy();
    ~LinuxEnv() { destroy(); };

#ifdef LINUXENV_DEBUG
    void assertClean();
    static void DisabledBarGroup(Thread::Group grp);
    static void DisabledUnbarGroup(Thread::Group grp);
#else
    static inline void DisabledBarGroup(Thread::Group grp) {
	Scheduler::DisabledBarGroup(grp);
    }
    static inline void DisabledUnbarGroup(Thread::Group grp) {
	Scheduler::DisabledUnbarGroup(grp);
    }
#endif

};

// Temporarily leave all groups, to make PPC call
extern void LinuxEnvSuspend();
// Return to groups
extern void LinuxEnvResume();


static inline struct LinuxEnv* getLinuxEnv() {
    return (LinuxEnv*)Scheduler::GetThreadSpecificUvalSelf();
}

#ifdef LINUXENV_DEBUG
#define ASSERTENV  do { getLinuxEnv()->assertClean();	        } while (0)
#else
#define ASSERTENV
#endif

#endif /* __LINUXENV_H_ */
