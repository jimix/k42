#ifndef __COBJ_REP_MEDIATOR_H_
#define __COBJ_REP_MEDIATOR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepMediator.H,v 1.11 2002/08/08 16:14:28 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definition of the mediator rep for dyn-switching
 * **************************************************************************/

#include <cobj/CObjRep.H>
#include <scheduler/Scheduler.H>
#include <misc/ListSimpleKey.H>

//FIXME get rid of this
extern sval err_tprintf(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

class MediatedThreadTable;
class CObjRootMediator;

enum COSwitchPhase {
    NORMAL = 0,
    SWITCH_MEDIATE_FORWARD = 1,
    SWITCH_MEDIATE_BLOCK = 2,
    SWITCH_COMPLETED = 3
};

// The original method is to simply allocate our own vtable and overwrite
// the hidden pointer member of each newly created object.
// The other method is have the compiler code do the vtable allocation
// and I overwrite the content of the vtable. This might not work if the
// vtable is allocated on read-only (text) pages.
//
#define _ALLOC_VTBL

#ifdef _ALLOC_VTBL
class CObjRepMediator : public Obj {
#else /* #ifdef _ALLOC_VTBL */
#include <cobj/sys/COSBreakpointObject.H>
class CObjRepMediator : public COSBreakpointObject {
#endif /* #ifdef _ALLOC_VTBL */
    CObjRepMediator();
public:
    typedef ListSimpleKey<uval, CObjRepMediator *, AllocGlobal> List;

    // The root keeps the instances of the Data, so that they can be accessed
    // in the absence of the corresponding mediators.
    struct Data {
	DEFINE_GLOBAL_NEW(Data);

	Data(COSwitchPhase medPhase, COSMgr::SwitchDispDir disp) : 
	    switchPhase(medPhase), disposition(disp), workerThreadCount(0) {
	    /* empty body */
	}

	// switch phase of the mediator (even if it is absent)
	COSwitchPhase switchPhase;
        COSMgr::SwitchDispDir disposition;
	sval workerThreadCount;
    };

    static uval fullMask(VPNum vp, uval csize) {
	switch(csize) {
	case 0: // == N
	    return uval(-1);
	case 1:
	    return (1 << vp);
	default:
	    tassert(0, err_printf("unimplemented cluster size"));
	    return (1 << vp);
	}
    }

    // FIXME: which new should we use
    DEFINE_GLOBAL_NEW(CObjRepMediator);

    CObjRepMediator(CObjRep *const r, Data *const data);
    ~CObjRepMediator();

    void genCountNotification();
    void lockedPhaseChange(COSwitchPhase phase);
    SysStatus unblockThreads();
    SysStatus forwardProlog(uval &ths, uval meth, uval ra, uval nvreg,
			    uval &pfunc, uval &doEpilog);
    SysStatus forwardEpilog(uval &ra, uval &nvreg);
protected:
    // The rep that this object is responsible for mediating
    // CONSIDER: currently it always has the originalRep, it may be better to
    // get at it on demand. That way, if the mediator is just to block new
    // calls, it doesn't really need to get at the originalRep, and it might
    // save some unneeded rep creation
    CObjRep *originalRep;
    // Data that need to exist regardless of the existence of a mediator
    Data *publicData;
    // Lock for phase change; prevents related race conditions
    BLock switchPhaseLock;
    sval mediatedCallCount;
    uval unblockThreadsTicket;
    MediatedThreadTable *hashTable;

    CObjRootMediator * swRoot() { return (CObjRootMediator *)myRoot; }

    SysStatus initVTable();
    COSwitchPhase &switchPhase() { return publicData->switchPhase; }
    CObjRep *getOriginalRep() { return originalRep; }
    CObjRep *getNewRep(uval methNum);
    sval localCallCounterInc();
    sval localCallCounterDec();
    uval localCallCounterIsZero();
    uval callCounterInc(const COSwitchPhase phase);
    uval callCounterDec(const COSwitchPhase phase);
    SysStatus pushThreadData(uval ra, uval nvreg);
    SysStatus popThreadData(uval &ra, uval &nvreg);
    uval isKnownRecursingThread();
    uval isPPCThread();
    SysStatus blockedListAdd(ThreadID *blocking);
    SysStatus blockedListRemove(ThreadID blocked = Scheduler::GetCurThread());

    // Pointer to our vtable - 0 if haven't installed our own trampolines
    static void *vtable;

private:
    class StaticLock : public BLock {
    public:
	DEFINE_GLOBAL_NEW(StaticLock);
    };

    // class lock for the static vtable initialization
    static StaticLock *VFTLock;
};

#endif /* #ifndef __COBJ_REP_MEDIATOR_H_ */
