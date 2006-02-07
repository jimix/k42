#ifndef __THREAD_H_
#define __THREAD_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Thread.H,v 1.73 2004/06/28 17:01:26 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic Thread Definition
 * **************************************************************************/

#include <cobj/sys/ActiveThrdCnt.H>

struct GenerationRecord;

class Thread {
protected:
    friend class Scheduler;
    friend class DispatcherDefault;
    friend class DispatcherDefaultExp;
    friend class ProcessLinuxClient;
    friend class ExceptionExp;
    friend class DispatcherDefaultKern;
    friend void genConstants(void);

public:
   enum Group {
	GROUP_PTHREAD,
	GROUP_LINUX,
	GROUP_LINUX_SYSCALL,
	GROUP_LINUX_SOFTIRQ,
	GROUP_LINUX_INTERRUPT,
	GROUP_LINUX_TEST,
	NUM_GROUPS
    };

public:
    /*
     * The first word of the thread object is reserved for use by layered
     * thread implementations (such as the K42 implementation of linuxthreads).
     * It MUST be the first field because higher-level thread implementations
     * use it without knowing the full Thread structure.
     */
    void        *attachment;
protected:
    // dynamic properties of the thread
    Thread      *next;            // pointer to other thread in a list
                                  //  e.g. scheduler free
    CommID       targetID;        // id from which we expect a PPC return or 0
    uval         curSP;           // current stackpointer to continue thread
    uval8        wasUnblocked;    // flag to say if unblock already happened
    uval8        isBlocked;       // flag to say if currently blocked

    uval8        migratable;	  // flag indicating that the thread is derived
				  // from ThreadMigratable, and that the
				  // migration hooks should be used

    uval8        extensionID;	  // used to identify types of extensions to
				  // to the base thread structure.  A value
				  // of zero means no extension.  A non-zero
				  // value means the thread shouldn't be
				  // re-used at this base level.


    uval16	groups;           // bit vector of classes to which a thread
				  // belongs.  All threads belonging to
				  // specified classes can be temporarily
				  // barred from running.

    sval64      *activeCntP;      // pointer to location where this thread
				  // is counted;  NULL if not active

    uval         pgfltID;         // ID of the pgflt whose completion this
				  // thread awaits.  -1 if not blocked on pgflt

    // persistent properties
    uval         startSP;         // initial stack pointer
    ThreadID     threadID;        // this thread's identifier

    uval	 threadSpecificUval;	// a word of thread specific data, used
					// by servers that need a pointer to
					// thread specific state

    uval         bottomSP;	  // limit of stack growth (debug only)
				  // may be 0, in which case checks are skipped
    uval         truebottomSP;    // true stack bottom (debug only)
                                  // including kernel page fault reserve
    uval	 altStack;        // alternate stack for arbiter, mediator...

    uval         upcallNeeded;    // thread (which is a shepherd for a sandbox)
				  // needs to make a sandbox upcall the next
				  // time it returns to sandbox mode

    inline void yielding();
    inline void blocking();
    inline uval isMigratable();

public:

    // Initialization to be done once when the thread is allocated:
    void init(void *baseOfStack, void *bottomOfStack, void* truebottom);

    /*
     * We use the CommID format for ThreadIDs, substituting a thread key for
     * the CommID pid.
     */
    static ThreadID MakeID(DispatcherID dspid, uval key) {
	return ThreadID(SysTypes::COMMID(key, dspid));
    }
    static uval GetKey(ThreadID id) {
	return uval(SysTypes::PID_FROM_COMMID(CommID(id)));
    }
    static DispatcherID GetDspID(ThreadID id) {
	return SysTypes::DSPID_FROM_COMMID(CommID(id));
    }
    static RDNum GetRD(ThreadID id) {
	return SysTypes::RD_FROM_COMMID(CommID(id));
    }
    static VPNum GetVP(ThreadID id) {
	return SysTypes::VP_FROM_COMMID(CommID(id));
    }

    ThreadID getID() { return threadID; }
    void setID(ThreadID id) { threadID = id; }
    void setID(DispatcherID dspid, uval key) { threadID = MakeID(dspid, key); }

    uval getKey() { return GetKey(threadID); }
    DispatcherID getDspID() { return GetDspID(threadID); }
    RDNum getRD() { return GetRD(threadID); }
    VPNum getVP() { return GetVP(threadID); }

    void setThreadSpecificUval(uval n) { threadSpecificUval = n; }
    uval getThreadSpecificUval()       { return threadSpecificUval; }

    uval isActive() {
	return uval(activeCntP);	// pseudo-boolean
    }

    uval getAltStack() {
        return altStack;
    }

    sval generation() {
        sval retvalue;
	retvalue = isActive() ?
		sval(ActiveThrdCnt::Generation(activeCntP)) : -1;
	return (retvalue);
    }

    void activate() {
	tassertMsg(!isActive(), "Thread already active.\n");
	tassertMsg(!isMigratable(), "Thread migratable!.\n");
	activeCntP = ActiveThrdCnt::Activate();
    }

    void deactivate() {
	tassertMsg(isActive(), "Not active.\n");
	tassertMsg(!isMigratable(), "Thread migratable!.\n");
	ActiveThrdCnt::Deactivate(activeCntP);
	activeCntP = NULL;
    }

    /*
     * DispatcherDefault reclaims base threads (those that have extensionID 0)
     * following a fork.  The thread that actually called fork() is typically
     * not a base thread and is therefore safe from reclamation, but a few of
     * our basic servers call fork on base threads.  These servers have to make
     * their fork threads safe from reclamation explicitly.
     */
    uval isForkSafe() {
	return (extensionID != 0);
    }

    void makeForkSafe() {
	tassertMsg(extensionID == 0, "Thread is already fork safe.\n");
	extensionID = 0xff;	// any non-zero value will do
    }

    void joinGroup(Group g) {
	tassertMsg(g < NUM_GROUPS, "Invalid group number.\n");
	groups |= (uval16(1) << g);
    }

    void leaveGroup(Group g) {
	tassertMsg(g < NUM_GROUPS, "Invalid group number.\n");
	groups &= ~(uval16(1) << g);
    }

    uval getGroups() { return groups; }
    uval setGroups(uval newGroups) {
	uval oldGroups = groups;
	groups = newGroups;
	return oldGroups;
    }

    void requestUpcall() { upcallNeeded = 1; }
    void clearUpcallRequest() { upcallNeeded = 0; }
    uval upcallRequested() { return upcallNeeded; }

    struct Status {
	enum {CALL_CHAIN_DEPTH = 24};
	typedef enum {UNKNOWN, FREE, RUNNING, READY,
		      BLOCKED, PPC_BLOCKED, PGFLT_BLOCKED,
		      IPC_RETRY_BLOCKED, BARRED} State;

	ThreadID id;
	uval ptr;
	uval attachment;
	State state;
	sval generation;
	CommID targetID;
	uval callChain[CALL_CHAIN_DEPTH];
	uval threadData;
	uval groups;
	void print();
    };

#ifndef NDEBUG
    void createStackFence() {
	int i;
	uval64 *p = (uval64 *)bottomSP;
	if (p != 0) {
	    for (i = 0; i < 16; i++) {
		*p = 0xbfbfbfbfbfbfbfbf;
		p++;
	    }
	}
    }

    void checkForStackOverflow() {
	int i;
	uval64 *p = (uval64 *)bottomSP;
	if (p != 0) {
	    for (i = 0; i < 16; i++) {
		if (*p != 0xbfbfbfbfbfbfbfbf) {
		    // Someone has corrupted the bottom of our stack:
		    breakpoint();
		}
		p++;
	    }
	}
    }
#endif /* #ifndef NDEBUG */
};

class ThreadMigratable : public Thread {
public:
    virtual uval isCurrentlyMigratable()=0;
    virtual void blocked()=0;
    virtual void yielded()=0;
};

inline uval
Thread::isMigratable()
{
    return (migratable &&
		((ThreadMigratable *) this)->isCurrentlyMigratable());
}

inline void
Thread::blocking()
{
    if (migratable) {
	((ThreadMigratable *) this)->blocked();
    }
}

inline void
Thread::yielding()
{
    if (migratable) {
	((ThreadMigratable *) this)->yielded();
    }
}

#endif /* #ifndef __THREAD_H_ */
