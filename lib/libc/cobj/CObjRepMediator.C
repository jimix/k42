/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepMediator.C,v 1.12 2002/10/30 17:35:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of the mediator rep for dyn-switching
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/COVTable.H>
#include <sync/Lock.H>
#include <scheduler/Scheduler.H>
#include "CObjRepMediator.H"
#include "mediateMethods.H"
#include "CObjRootMediator.H"
#include "MediatedThreadTable.H"

//#define _DO_ERR_TPRINTF_

#if 1  // My temp thread printf routine. FIXME remove this
#include "io/printfBuf.H"

sval
err_tprintf(const char *fmt, ...)
{
#ifdef _DO_ERR_TPRINTF_
    va_list ap;
    char buf[CONSOLE_BUF_MAX];

    va_start(ap, fmt);
    uval len = printfBuf(fmt, ap, buf, CONSOLE_BUF_MAX);
    va_end(ap);

    buf[len] = '\0';

    err_printf("[TID#%6.6lx] %s", Scheduler::GetCurThread(), buf);
#endif /* #ifdef _DO_ERR_TPRINTF_ */
    return 0;
}
#endif /* #if 1  // My temp thread printf ... */

#define SET_MEDIATE_METHOD(OP) \
 ((COVTable *)vtable)->vt[OP].setFunc((uval)&mediateMethod ## OP)

/* static */ void *CObjRepMediator::vtable = 0;
/* static */ CObjRepMediator::StaticLock *CObjRepMediator::VFTLock;

CObjRepMediator::CObjRepMediator(CObjRep *const r, Data *const data)
    : originalRep(r), publicData(data), mediatedCallCount(0),
      unblockThreadsTicket(1), hashTable(new MediatedThreadTable)
{
    tassert(data, ;);
    tassert(data->switchPhase != NORMAL, ;);

    VFTLock = new StaticLock();
    VFTLock->init();

    initVTable();
}

CObjRepMediator::~CObjRepMediator()
{
    delete hashTable;
}

SysStatus
CObjRepMediator::initVTable()
{
    AutoLock<StaticLock> al(VFTLock);

    // FIXME: remove these statics and do the entire thing in some ClassInit.
    // No lock needed then. Issue: at ClassInit time, we don't have an instance
    // of this object yet. How do I access the vtable without having an
    // instance?
    if (!vtable) {
#ifdef _ALLOC_VTBL
	// First time using the vtbl pointer. Allocate and initialize a full
	// vtable filled with our trampoline stubs (mediateMethod##N##).
	vtable = allocGlobal(sizeof(COVTable));
	tassert(vtable,
		err_printf("allocGlobal(%ld) failed.\n", sizeof(COVTable)));
#else /* #ifdef _ALLOC_VTBL */
	// I think we only need to overwrite the vtable once for all instances
	// of this class, as the vtable should be shared by all instances.
	// There is an assert at the end to verify this.
	vtable = *(COVTable **)this;
#endif /* #ifdef _ALLOC_VTBL */
	// FIXME: maybe I should copy the virtual d'tor and the other
	// cleanup methods here so that the mediator can be cleaned up
	// properly by the destruction code.
	SET_MEDIATE_METHOD(0);
		// Not sure we actually want this as the first vtable
		// seems empty or at least reserved for other uses,
		// but for the sake of symmetry
	SET_MEDIATE_METHOD(1);
	SET_MEDIATE_METHOD(2);
	SET_MEDIATE_METHOD(3);
	SET_MEDIATE_METHOD(4);
	SET_MEDIATE_METHOD(5);
	SET_MEDIATE_METHOD(6);
	SET_MEDIATE_METHOD(7);
	SET_MEDIATE_METHOD(8);
	SET_MEDIATE_METHOD(9);

	SET_MEDIATE_METHOD(10);
	SET_MEDIATE_METHOD(11);
	SET_MEDIATE_METHOD(12);
	SET_MEDIATE_METHOD(13);
	SET_MEDIATE_METHOD(14);
	SET_MEDIATE_METHOD(15);
	SET_MEDIATE_METHOD(16);
	SET_MEDIATE_METHOD(17);
	SET_MEDIATE_METHOD(18);
	SET_MEDIATE_METHOD(19);

	SET_MEDIATE_METHOD(20);
	SET_MEDIATE_METHOD(21);
	SET_MEDIATE_METHOD(22);
	SET_MEDIATE_METHOD(23);
	SET_MEDIATE_METHOD(24);
	SET_MEDIATE_METHOD(25);
	SET_MEDIATE_METHOD(26);
	SET_MEDIATE_METHOD(27);
	SET_MEDIATE_METHOD(28);
	SET_MEDIATE_METHOD(29);

	SET_MEDIATE_METHOD(30);
	SET_MEDIATE_METHOD(31);
	SET_MEDIATE_METHOD(32);
	SET_MEDIATE_METHOD(33);
	SET_MEDIATE_METHOD(34);
	SET_MEDIATE_METHOD(35);
	SET_MEDIATE_METHOD(36);
	SET_MEDIATE_METHOD(37);
	SET_MEDIATE_METHOD(38);
	SET_MEDIATE_METHOD(39);

	SET_MEDIATE_METHOD(40);
	SET_MEDIATE_METHOD(41);
	SET_MEDIATE_METHOD(42);
	SET_MEDIATE_METHOD(43);
	SET_MEDIATE_METHOD(44);
	SET_MEDIATE_METHOD(45);
	SET_MEDIATE_METHOD(46);
	SET_MEDIATE_METHOD(47);
	SET_MEDIATE_METHOD(48);
	SET_MEDIATE_METHOD(49);

	SET_MEDIATE_METHOD(50);
	SET_MEDIATE_METHOD(51);
	SET_MEDIATE_METHOD(52);
	SET_MEDIATE_METHOD(53);
	SET_MEDIATE_METHOD(54);
	SET_MEDIATE_METHOD(55);
	SET_MEDIATE_METHOD(56);
	SET_MEDIATE_METHOD(57);
	SET_MEDIATE_METHOD(58);
	SET_MEDIATE_METHOD(59);

	SET_MEDIATE_METHOD(60);
	SET_MEDIATE_METHOD(61);
	SET_MEDIATE_METHOD(62);
	SET_MEDIATE_METHOD(63);
	SET_MEDIATE_METHOD(64);
	SET_MEDIATE_METHOD(65);
	SET_MEDIATE_METHOD(66);
	SET_MEDIATE_METHOD(67);
	SET_MEDIATE_METHOD(68);
	SET_MEDIATE_METHOD(69);

	SET_MEDIATE_METHOD(70);
	SET_MEDIATE_METHOD(71);
	SET_MEDIATE_METHOD(72);
	SET_MEDIATE_METHOD(73);
	SET_MEDIATE_METHOD(74);
	SET_MEDIATE_METHOD(75);
	SET_MEDIATE_METHOD(76);
	SET_MEDIATE_METHOD(77);
	SET_MEDIATE_METHOD(78);
	SET_MEDIATE_METHOD(79);

	SET_MEDIATE_METHOD(80);
	SET_MEDIATE_METHOD(81);
	SET_MEDIATE_METHOD(82);
	SET_MEDIATE_METHOD(83);
	SET_MEDIATE_METHOD(84);
	SET_MEDIATE_METHOD(85);
	SET_MEDIATE_METHOD(86);
	SET_MEDIATE_METHOD(87);
	SET_MEDIATE_METHOD(88);
	SET_MEDIATE_METHOD(89);

	SET_MEDIATE_METHOD(90);
	SET_MEDIATE_METHOD(91);
	SET_MEDIATE_METHOD(92);
	SET_MEDIATE_METHOD(93);
	SET_MEDIATE_METHOD(94);
	SET_MEDIATE_METHOD(95);
	SET_MEDIATE_METHOD(96);
	SET_MEDIATE_METHOD(97);
	SET_MEDIATE_METHOD(98);
	SET_MEDIATE_METHOD(99);

	SET_MEDIATE_METHOD(100);
	SET_MEDIATE_METHOD(101);
	SET_MEDIATE_METHOD(102);
	SET_MEDIATE_METHOD(103);
	SET_MEDIATE_METHOD(104);
	SET_MEDIATE_METHOD(105);
	SET_MEDIATE_METHOD(106);
	SET_MEDIATE_METHOD(107);
	SET_MEDIATE_METHOD(108);
	SET_MEDIATE_METHOD(109);

	SET_MEDIATE_METHOD(110);
	SET_MEDIATE_METHOD(111);
	SET_MEDIATE_METHOD(112);
	SET_MEDIATE_METHOD(113);
	SET_MEDIATE_METHOD(114);
	SET_MEDIATE_METHOD(115);
	SET_MEDIATE_METHOD(116);
	SET_MEDIATE_METHOD(117);
	SET_MEDIATE_METHOD(118);
	SET_MEDIATE_METHOD(119);

	SET_MEDIATE_METHOD(120);
	SET_MEDIATE_METHOD(121);
	SET_MEDIATE_METHOD(122);
	SET_MEDIATE_METHOD(123);
	SET_MEDIATE_METHOD(124);
	SET_MEDIATE_METHOD(125);
	SET_MEDIATE_METHOD(126);
	SET_MEDIATE_METHOD(127);
	SET_MEDIATE_METHOD(128);
	SET_MEDIATE_METHOD(129);

	SET_MEDIATE_METHOD(130);
	SET_MEDIATE_METHOD(131);
	SET_MEDIATE_METHOD(132);
	SET_MEDIATE_METHOD(133);
	SET_MEDIATE_METHOD(134);
	SET_MEDIATE_METHOD(135);
	SET_MEDIATE_METHOD(136);
	SET_MEDIATE_METHOD(137);
	SET_MEDIATE_METHOD(138);
	SET_MEDIATE_METHOD(139);

	SET_MEDIATE_METHOD(140);
	SET_MEDIATE_METHOD(141);
	SET_MEDIATE_METHOD(142);
	SET_MEDIATE_METHOD(143);
	SET_MEDIATE_METHOD(144);
	SET_MEDIATE_METHOD(145);
	SET_MEDIATE_METHOD(146);
	SET_MEDIATE_METHOD(147);
	SET_MEDIATE_METHOD(148);
	SET_MEDIATE_METHOD(149);

	SET_MEDIATE_METHOD(150);
	SET_MEDIATE_METHOD(151);
	SET_MEDIATE_METHOD(152);
	SET_MEDIATE_METHOD(153);
	SET_MEDIATE_METHOD(154);
	SET_MEDIATE_METHOD(155);
	SET_MEDIATE_METHOD(156);
	SET_MEDIATE_METHOD(157);
	SET_MEDIATE_METHOD(158);
	SET_MEDIATE_METHOD(159);

	SET_MEDIATE_METHOD(160);
	SET_MEDIATE_METHOD(161);
	SET_MEDIATE_METHOD(162);
	SET_MEDIATE_METHOD(163);
	SET_MEDIATE_METHOD(164);
	SET_MEDIATE_METHOD(165);
	SET_MEDIATE_METHOD(166);
	SET_MEDIATE_METHOD(167);
	SET_MEDIATE_METHOD(168);
	SET_MEDIATE_METHOD(169);

	SET_MEDIATE_METHOD(170);
	SET_MEDIATE_METHOD(171);
	SET_MEDIATE_METHOD(172);
	SET_MEDIATE_METHOD(173);
	SET_MEDIATE_METHOD(174);
	SET_MEDIATE_METHOD(175);
	SET_MEDIATE_METHOD(176);
	SET_MEDIATE_METHOD(177);
	SET_MEDIATE_METHOD(178);
	SET_MEDIATE_METHOD(179);

	SET_MEDIATE_METHOD(180);
	SET_MEDIATE_METHOD(181);
	SET_MEDIATE_METHOD(182);
	SET_MEDIATE_METHOD(183);
	SET_MEDIATE_METHOD(184);
	SET_MEDIATE_METHOD(185);
	SET_MEDIATE_METHOD(186);
	SET_MEDIATE_METHOD(187);
	SET_MEDIATE_METHOD(188);
	SET_MEDIATE_METHOD(189);

	SET_MEDIATE_METHOD(190);
	SET_MEDIATE_METHOD(191);
	SET_MEDIATE_METHOD(192);
	SET_MEDIATE_METHOD(193);
	SET_MEDIATE_METHOD(194);
	SET_MEDIATE_METHOD(195);
	SET_MEDIATE_METHOD(196);
	SET_MEDIATE_METHOD(197);
	SET_MEDIATE_METHOD(198);
	SET_MEDIATE_METHOD(199);

	SET_MEDIATE_METHOD(200);
	SET_MEDIATE_METHOD(201);
	SET_MEDIATE_METHOD(202);
	SET_MEDIATE_METHOD(203);
	SET_MEDIATE_METHOD(204);
	SET_MEDIATE_METHOD(205);
	SET_MEDIATE_METHOD(206);
	SET_MEDIATE_METHOD(207);
	SET_MEDIATE_METHOD(208);
	SET_MEDIATE_METHOD(209);

	SET_MEDIATE_METHOD(210);
	SET_MEDIATE_METHOD(211);
	SET_MEDIATE_METHOD(212);
	SET_MEDIATE_METHOD(213);
	SET_MEDIATE_METHOD(214);
	SET_MEDIATE_METHOD(215);
	SET_MEDIATE_METHOD(216);
	SET_MEDIATE_METHOD(217);
	SET_MEDIATE_METHOD(218);
	SET_MEDIATE_METHOD(219);

	SET_MEDIATE_METHOD(220);
	SET_MEDIATE_METHOD(221);
	SET_MEDIATE_METHOD(222);
	SET_MEDIATE_METHOD(223);
	SET_MEDIATE_METHOD(224);
	SET_MEDIATE_METHOD(225);
	SET_MEDIATE_METHOD(226);
	SET_MEDIATE_METHOD(227);
	SET_MEDIATE_METHOD(228);
	SET_MEDIATE_METHOD(229);

	SET_MEDIATE_METHOD(230);
	SET_MEDIATE_METHOD(231);
	SET_MEDIATE_METHOD(232);
	SET_MEDIATE_METHOD(233);
	SET_MEDIATE_METHOD(234);
	SET_MEDIATE_METHOD(235);
	SET_MEDIATE_METHOD(236);
	SET_MEDIATE_METHOD(237);
	SET_MEDIATE_METHOD(238);
	SET_MEDIATE_METHOD(239);

	SET_MEDIATE_METHOD(240);
	SET_MEDIATE_METHOD(241);
	SET_MEDIATE_METHOD(242);
	SET_MEDIATE_METHOD(243);
	SET_MEDIATE_METHOD(244);
	SET_MEDIATE_METHOD(245);
	SET_MEDIATE_METHOD(246);
	SET_MEDIATE_METHOD(247);
	SET_MEDIATE_METHOD(248);
	SET_MEDIATE_METHOD(249);

	SET_MEDIATE_METHOD(250);
	SET_MEDIATE_METHOD(251);
	SET_MEDIATE_METHOD(252);
	SET_MEDIATE_METHOD(253);
	SET_MEDIATE_METHOD(254);
	SET_MEDIATE_METHOD(255);
    }
#ifdef _ALLOC_VTBL
    // Overwrite the hidden _pvft member of the object
    *(COVTable **)this = (COVTable *)vtable;
#else /* #ifdef _ALLOC_VTBL */
    tassert(*(COVTable **)this == (COVTable *)vtable,
	    err_printf("I thought the vtable is a static class member!\n"));
#endif /* #ifdef _ALLOC_VTBL */

    return 0;
}

inline uval
CObjRepMediator::localCallCounterIsZero()
{
    return mediatedCallCount == 0;
}

inline sval
CObjRepMediator::localCallCounterInc()
{
    return FetchAndAddSignedSynced(&mediatedCallCount, 1) + 1;
}

inline sval
CObjRepMediator::localCallCounterDec()
{
    return FetchAndAddSignedSynced(&mediatedCallCount, -1) - 1;
}

inline uval
CObjRepMediator::callCounterInc(const COSwitchPhase phase)
{
    tassert(phase != NORMAL && phase != SWITCH_COMPLETED, ;);
    if (phase > SWITCH_MEDIATE_FORWARD) {
	if (localCallCounterIsZero()) {
	    if (!swRoot()->incNumMediatorsWithInFlightCalls()) {
		return 0;
	    }
	}
    }
    localCallCounterInc();
    return 1;
}

// returns whether the global count is now zero
inline uval
CObjRepMediator::callCounterDec(const COSwitchPhase phase)
{
    tassert(phase != NORMAL && phase != SWITCH_COMPLETED, ;);
    const sval count = localCallCounterDec();

    if (phase == SWITCH_MEDIATE_FORWARD) return 0;

    tassert(phase == SWITCH_MEDIATE_BLOCK, ;);

    if (count == 0) {
	return swRoot()->decNumMediatorsWithInFlightCalls();
    }
    return 0;
}

// Get the rep for the CO to be switched to
CObjRep *
CObjRepMediator::getNewRep(uval methNum)
{
    tassert(switchPhase() == SWITCH_COMPLETED, ;);
    tassert(swRoot()->getPhase() == SWITCH_COMPLETED, ;);

    // since, at this moment, the LTE may not yet be flushed, we cannot
    // go to get it from the LTE directly...
    // FIXME: should somehow optimize this
    //        maybe cache the new rep pointer here
    CObjRep *nrep = swRoot()->getNewRep(methNum);
    err_tprintf("CObjRepMediator::getNewRep() returns %p\n", nrep);
    return nrep;
}

inline SysStatus
CObjRepMediator::forwardProlog(uval &ths, uval meth, uval ra, uval nvreg,
			       uval &pfunc, uval &doEpilog)
{
    err_tprintf("med=%p, rep=%p\n", this, originalRep);
    ThreadID curThread;
    CObjRep *rep = 0;
    // assume that we will be using the epilog
    doEpilog = 1;

    switchPhaseLock.acquire();
    const COSwitchPhase phase = switchPhase();
    switch (phase) {
    case SWITCH_MEDIATE_FORWARD:
	err_tprintf("...MFP: SWITCH_MEDIATE_FORWARD\n");
	// inc before releasing lock so that we won't switch phase prematurely
	callCounterInc(phase);
	switchPhaseLock.release();
	pushThreadData(ra, nvreg);
	// Get the rep that this call is supposed to be forwarded to
	rep = getOriginalRep();
	break;
    case SWITCH_MEDIATE_BLOCK:
	err_tprintf("...MFP: SWITCH_MEDIATE_BLOCK\n");
	curThread = Scheduler::GetCurThread();

	blockedListAdd(&curThread);
	switchPhaseLock.release();
	if (isKnownRecursingThread()) {
	    // A thread recursing (direct, same-thread recursion)
	    // Must not block this thread even if it is a PPC thread
	    err_tprintf("...MFP: know recusing thread!\n");
	    blockedListRemove();
	    const uval success = callCounterInc(phase);
	    tassert(success, ;);
	    pushThreadData(ra, nvreg);
	    // Get the rep that this call is supposed to be forwarded to
	    rep = getOriginalRep();
	} else if (!isPPCThread() && callCounterInc(phase)) {
	    // We allow it to go thru as long as there might still be
	    // any recursing thread (known or unknown).
	    // This is to avoid any recursive (non-PPC) threads
	    // blocking and causing a deadlock.
	    //
	    // If callCounterInc() fails then we had reached the point that
	    // the global call count is zero and that the phase is BLOCK.
	    // So we are guaranteed that the thread is not recursive and
	    // hence we should block it. Forwarding it would be incorrect
	    // since data transfer may be in progress.
	    blockedListRemove();
	    pushThreadData(ra, nvreg);
	    // Get the rep that this call is supposed to be forwarded to
	    rep = getOriginalRep();
	} else {
	    // block the thread in a loop to prevent being unblocked by
	    // previously queued up unblocks
	    do {
		err_tprintf("...MFP: BLOCKING\n");
		Scheduler::Block();
		err_tprintf("...MFP: Thread(%lx) CONTINUES\n", curThread);
	    } while (curThread != Scheduler::NullThreadID);
	    // set doEpilog = 0 for the threads which got unblocked
	    tassert(switchPhase() == SWITCH_COMPLETED, ;);
	    tassert(swRoot()->getPhase() == SWITCH_COMPLETED, ;);
	    doEpilog = 0;
	    rep = getNewRep(meth);
	}
	break;
    case SWITCH_COMPLETED:
	err_tprintf("...MFP: SWITCH_MEDIATE_COMPLETED\n");
	switchPhaseLock.release();
	doEpilog = 0;
	rep = getNewRep(meth);
	break;
    default:
	tassert(0, err_printf("Should be switching!\n"));
	switchPhaseLock.release();
	break;
    }

    // Now, rep should be the one that we are redirecting the call to
    tassert(rep, err_printf("rep shouldn't be 0 here!\n"));

    // To ensure restarting of method invocation locate method in rep
    COVTable *const repvtable=*((COVTable **)rep);
    pfunc = repvtable->vt[meth].getFunc();

    // Fixup the 'this' pointer of the vfunc to point at the rep
    ths=(uval)rep;

    return 0;
}

inline SysStatus
CObjRepMediator::forwardEpilog(uval &ra, uval &nvreg)
{
    // asserting that root phase is BLOCK => med phase is BLOCK
    tassert(swRoot()->getPhase() != SWITCH_MEDIATE_BLOCK ||
	    switchPhase() == SWITCH_MEDIATE_BLOCK, ;);

    switchPhaseLock.acquire();
    const COSwitchPhase phase = switchPhase();
    if (phase == SWITCH_MEDIATE_FORWARD) {
	callCounterDec(phase);
	switchPhaseLock.release();
    } else {
	tassert(phase == SWITCH_MEDIATE_BLOCK, ;);
	const uval isGloballyZero = callCounterDec(phase);
	switchPhaseLock.release();

	// No need to lock the root phase here since if the check fails,
	// then there is going to be a worker thread to execute the counter
	// check later.
	if (isGloballyZero) {
	    if (swRoot()->getPhase() == SWITCH_MEDIATE_BLOCK) {
		err_tprintf("Last in-flight call returning.\n");
		swRoot()->blockPhaseFini();
	    }
	}
    }

    tassert(phase != NORMAL, ;);
    tassert(phase != SWITCH_COMPLETED, ;);

    popThreadData(ra, nvreg);

    return 0;
}

inline SysStatus
CObjRepMediator::pushThreadData(uval ra, uval nvreg)
{
    hashTable->pushData(Scheduler::GetCurThread(), ra, nvreg);

    return 0;
}

inline SysStatus
CObjRepMediator::popThreadData(uval &ra, uval &nvreg)
{
    hashTable->popData(Scheduler::GetCurThread(), ra, nvreg);

    return 0;
}

inline uval
CObjRepMediator::isPPCThread()
{
    // FIXME
    return 1;
}

inline uval
CObjRepMediator::isKnownRecursingThread()
{
    return hashTable->queryThreadExists(Scheduler::GetCurThread());
}

inline SysStatus
CObjRepMediator::blockedListAdd(ThreadID *blocking)
{
    hashTable->addBlockedThread(blocking);
    return 0;
}

inline SysStatus
CObjRepMediator::blockedListRemove(ThreadID blocked)
{
    hashTable->removeBlockedThread(blocked);
    return 0;
}

SysStatus
CObjRepMediator::unblockThreads()
{
    uval ticket = FetchAndClearSynced(&unblockThreadsTicket);
    if (ticket) {
	err_tprintf("Unblocking threads for this mediator...\n");
	// must switch to COMPLETED before unblocking threads
	lockedPhaseChange(SWITCH_COMPLETED);
	hashTable->unblockThreads();
    }

    return 0;
}

void
CObjRepMediator::genCountNotification()
{
    switchPhaseLock.acquire();
    tassert(switchPhase() != NORMAL && switchPhase() != SWITCH_COMPLETED, ;);
    tassert(switchPhase() <= SWITCH_MEDIATE_BLOCK,
	    err_printf("Why are we switching backwards?\n"));
    if (switchPhase() == SWITCH_MEDIATE_FORWARD) {
	if (!localCallCounterIsZero()) {
	    const uval s = swRoot()->incNumMediatorsWithInFlightCalls();
	    tassert(s, ;);
	}
	err_tprintf("genCountNotification(): phase switch to BLOCK\n");
	switchPhase() = SWITCH_MEDIATE_BLOCK;
    }
    switchPhaseLock.release();
}

void
CObjRepMediator::lockedPhaseChange(COSwitchPhase phase)
{
    switchPhaseLock.acquire();
    tassert(switchPhase() <= phase,
	    err_printf("Why are we switching backwards?\n"));
    switchPhase() = phase;
    switchPhaseLock.release();
}

//---------------

extern "C" uval mediateForwardProlog(uval &ths, uval meth, uval ra, uval nvreg,
				     uval &doEpilog);
extern "C" uval mediateForwardEpilog(CObjRepMediator *med, uval &nvreg);

uval
mediateForwardProlog(uval &ths, uval meth, uval ra, uval nvreg, uval &doEpilog)
{
    uval pfunc = 0;

    err_tprintf("...MFP [in]: ths(%lx), meth#%ld, ra(%lx), nvreg(%lx)\n",
	        ths, meth, ra, nvreg);
    CObjRepMediator *const med = (CObjRepMediator *)ths;
    med->forwardProlog(ths, meth, ra, nvreg, pfunc, doEpilog);
    err_tprintf("...MFP [out]: ths(0x%lx) doEpilog(%ld)\n", ths, doEpilog);
    return pfunc;
}

uval
mediateForwardEpilog(CObjRepMediator *med, uval &nvreg)
{
    uval ra;

    err_tprintf("...MFE: med(%p)\n", med);

    med->forwardEpilog(ra, nvreg);

    err_tprintf("...MFE: return ra(%lx) nvreg(%lx)\n.\n", ra, nvreg);

    return ra;
}
