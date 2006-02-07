/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ThinWireMgr.C,v 1.49 2005/02/09 18:45:42 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A on-thread interface to ThinWire facility
 * **************************************************************************/
#include "kernIncs.H"
#include "ThinWireMgr.H"
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
#include <misc/hardware.H>
#include <trace/traceIO.h>
#include "defines/sim_bugs.H"
// FIXME! This shouldn't be including arch-specific code
#include "arch/powerpc/BootInfo.H" // for SIM_MAMBO
#include <sys/IOChan.H>
#include "exception/ExceptionLocal.H"
ThinWireMgr *ThinWireMgr::thinWireMgr;


uval
TWMgrChan::read(char* buf, uval length, uval block)
{
    uval ret;
    if (block) {
	ret = ThinWireMgr::thinWireMgr->blockingRead(id, buf, length);
    } else {
	ret = ThinWireMgr::thinWireMgr->nonBlockingRead(id, buf, length);
    }
    return ret;
}

uval
TWMgrChan::write(const char* buf, uval length, uval block)
{
    sval ret;
    if (block) {
	ret = ThinWireMgr::thinWireMgr->blockingWrite(id, buf, length);
    } else {
	ret = ThinWireMgr::thinWireMgr->nonBlockingWrite(id, buf, length);
    }
    return ret;
}

uval
TWMgrChan::isReadable()
{
    return ThinWireMgr::thinWireMgr->testAvailable(id);
}

inline sval
ThinWireMgr::locked_read(uval channel, char *buf, uval length)
{
    _ASSERT_HELD(lock);
    uval rc;

#if defined(TARGET_powerpc)
    InterruptState is;
    disableHardwareInterrupts(is);
    rc = ThinWireChan::twChannels[channel]->read(buf, length);
    enableHardwareInterrupts(is);
#elif defined(TARGET_mips64)
    extern uval enabledThinwireRead(uval channel, char * buf, uval length);
    rc = enabledThinwireRead(channel,buf,length);
#elif defined(TARGET_amd64)
    rc = 0;
    passertMsg(0, "woops not for amd64\n");
#elif defined(TARGET_generic64)
    extern uval enabledThinwireRead(uval channel, char * buf, uval length);
    rc = enabledThinwireRead(channel,buf,length);
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
    return rc;
}

inline uval
ThinWireMgr::locked_testAvailable(uval channel)
{
    _ASSERT_HELD(lock);

    if (readAvailable & (1<<channel)) return 1;
    return 0;
}

inline void
ThinWireMgr::locked_clearAvailable(uval channel)
{
    _ASSERT_HELD(lock);
    readAvailable &= ~(1<<channel);
}

inline uval
ThinWireMgr::locked_testPending(uval channel)
{
    _ASSERT_HELD(lock);
    if (readPending & (1<<channel)) return 1;
    return 0;
}

inline void
ThinWireMgr::locked_clearPending(uval channel)
{
    _ASSERT_HELD(lock);
    readPending &= ~(1<<channel);
}

inline void
ThinWireMgr::locked_setPending(uval channel)
{
    _ASSERT_HELD(lock);
    readPending |= 1<<channel;
}

inline sval
ThinWireMgr::locked_updateAvailable()
{
    _ASSERT_HELD(lock);

#if defined(TARGET_powerpc)
    InterruptState is;
    sval rc;
    disableHardwareInterrupts(is);
    rc = ThinWireChan::thinwireSelect();
    if (_FAILURE(rc)) {
	enableHardwareInterrupts(is);
	return rc;
    }
    readAvailable |= rc;
    enableHardwareInterrupts(is);
#elif defined(TARGET_mips64)
    extern uval32 enabledThinwireSelect();
    readAvailable |= enabledThinwireSelect();
#elif defined(TARGET_amd64)
    InterruptState is; // why not XXX, this one more like powerpc this time
    disableHardwareInterrupts(is);
    readAvailable |= thinwireSelect();
    enableHardwareInterrupts(is);
#elif defined(TARGET_generic64)
    extern uval32 enabledThinwireSelect();
    readAvailable |= enabledThinwireSelect();
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

    return 0;
}


inline sval
ThinWireMgr::locked_write(uval channel, const char *buf, uval length)
{
    _ASSERT_HELD(lock);
    sval rc;

#if defined(TARGET_powerpc)
    InterruptState is;
    disableHardwareInterrupts(is);
    rc = ThinWireChan::twChannels[channel]->write(buf, length);
    enableHardwareInterrupts(is);
    return rc;
#elif defined(TARGET_mips64)
    extern void enabledThinwireWrite(uval channel, const char * buf,
				     uval length);
    enabledThinwireWrite(channel,buf,length);
#elif defined(TARGET_amd64)
    passertMsg(0, "woops not for amd64\n");
#elif defined(TARGET_generic64)
    rc = ThinWireChan::twChannels[channel]->write(buf, length);
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

}

/* static */ void
ThinWireMgr::ClassInit(VPNum vp)
{
#ifndef FAST_REGRESS_ON_SIM
    if (vp==0) {
	// pinned allocator initialized, not paged
	thinWireMgr = new ThinWireMgr();

	thinWireMgr->lock.init();
	thinWireMgr->readAvailable = 0;
	thinWireMgr->readPending   = 0;
	for (uval i=0;i<NUM_CHANNELS; i++) {
	    thinWireMgr->pending[i].id = Scheduler::NullThreadID;
	}
 	thinWireMgr->suspendDaemon = 0;
 	thinWireMgr->runDaemon = 1;

	if (KernelInfo::OnSim() == SIM_MAMBO) {
	    // bogus made up number - 30000 instructions 1/20/2004 MAA
	    // max 900000 or about 1/2 second
	    SetPollDelay(100, 3000);
	} else {
	    // old standard 13msec each
	    // existing bogus (its 130 ms) numbers
	    SetPollDelay(130000, 130000);
	}

#if defined(TARGET_mips64)
	// on mips on hardware gizmo is very slow ~54ms to do write/read
	// sequence, so reduce frequency
	if (!KernelInfo::OnSim()) {
	    SetPollDelay(500000, 10000000);
	}
#endif /* #if defined(TARGET_mips64) */
	thinWireMgr->currDelay = thinWireMgr->maxDelay;

	SysStatus rc = Scheduler::ScheduleFunction(BeAsynchronous, 0,
						       thinWireMgr->daemon);
	passertMsg(_SUCCESS(rc), "woops\n");

    }

#endif
}

/* static */ void
ThinWireMgr::SetPollDelay(uval minDelay, uval maxDelay)
{
#ifndef FAST_REGRESS_ON_SIM
    thinWireMgr->minDelay = minDelay;
    thinWireMgr->maxDelay = maxDelay;
#else
    breakpoint();
#endif
}

/*static*/ sval
ThinWireMgr::DoPolling()
{
#ifndef FAST_REGRESS_ON_SIM
    uval rc;

    thinWireMgr->lock.acquire();
    rc = thinWireMgr->locked_updateAvailable();
    if (_FAILURE(rc)) {
	return rc;
    }
    if ((thinWireMgr->readAvailable & thinWireMgr->readPending) != 0) {
	uval32 towake = thinWireMgr->readAvailable &
	    thinWireMgr->readPending;
	for (uval i=0;;i++) {
	    if (towake & 0x1) {
		rc = 1;
		Scheduler::Unblock(thinWireMgr->pending[i].id);
		thinWireMgr->pending[i].id = Scheduler::NullThreadID;
		thinWireMgr->locked_clearPending(i);
	    }
	    towake = towake>>1;
	    if (!towake) break;
	}
    }
    thinWireMgr->lock.release();
    return rc;
#else
    breakpoint();
    return 0;
#endif
}

/* static */ void
ThinWireMgr::BeAsynchronous(uval arg)
{
    uval gotSomething = 0;
    while (thinWireMgr->runDaemon) {
	if (KernelInfo::ControlFlagIsSet(KernelInfo::SLOW_THINWIRE_POLLING)) {
	    thinWireMgr->currDelay = 1000000;	// 1 second, unconditional
	} else {
	    if (gotSomething == 0) {
		thinWireMgr->currDelay = thinWireMgr->currDelay * 2;
		if (thinWireMgr->currDelay > thinWireMgr->maxDelay) {
		    thinWireMgr->currDelay = thinWireMgr->maxDelay;
		}
	    } else {
		thinWireMgr->currDelay = thinWireMgr->minDelay;
	    }
	}
	gotSomething = 0;
	Scheduler::DeactivateSelf();
	Scheduler::DelayMicrosecs(thinWireMgr->currDelay);
	Scheduler::ActivateSelf();

 	while (thinWireMgr->suspendDaemon) {
	    err_printf("---- thinwire daemon blocked\n");
	    Scheduler::DeactivateSelf();
 	    Scheduler::Block();
	    Scheduler::ActivateSelf();
	    if (!thinWireMgr->suspendDaemon)
		err_printf("---- thinwire daemon awake\n");
 	}
	gotSomething = DoPolling();
    }

    err_printf("---- thinwire daemon exiting\n");
}

sval
ThinWireMgr::blockingRead(uval channel, char *buf, uval length)
{
    sval rc;

    rc = ThinWireMgr::DoPolling();
    if (_FAILURE(rc)) {
	return rc;
    }

    lock.acquire();
    tassert( !locked_testPending(channel),
	     err_printf("pending on channel %lx was set by someone"
			"\t id is %lx (myid %lx)\n",
			channel, pending[channel].id,
			Scheduler::GetCurThread()));
    while (1) {
	if (locked_testAvailable(channel)) {
	    locked_clearAvailable(channel);
	    uval rc = locked_read(channel, buf, length);
	    lock.release();
	    return rc;
	}
	locked_setPending(channel);
	pending[channel].id = Scheduler::GetCurThread();
	lock.release();
	Scheduler::Block();
	lock.acquire();
    }
}

sval
ThinWireMgr::nonBlockingWrite(uval channel, const char *buf, uval length)
{
#ifndef FAST_REGRESS_ON_SIM
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    return locked_write(channel, buf, length);
#else
    breakpoint();
#endif
}

sval
ThinWireMgr::blockingWrite(uval channel, const char *buf, uval length)
{
    return nonBlockingWrite(channel, buf, length);
}

sval
ThinWireMgr::nonBlockingRead(uval channel, char *buf, uval length)
{
    uval rc=0;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    if (locked_testAvailable(channel)) {
	locked_clearAvailable(channel);
	rc = locked_read(channel, buf, length);
    }
    return rc;
}

uval
ThinWireMgr::TestAvailable(uval channel)
{
    return thinWireMgr->testAvailable(channel);
}

uval
ThinWireMgr::testAvailable(uval channel)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    return locked_testAvailable(channel);
}


// FIXME: move back to .H
/* static */ void
ThinWireMgr::NonBlockingWrite(uval channel, const char *buf, uval length)
{
    thinWireMgr->nonBlockingWrite(channel, buf, length);
}

/* static */ uval
ThinWireMgr::NonBlockingRead(uval channel, char *buf, uval length)
{
#ifndef FAST_REGRESS_ON_SIM
    return thinWireMgr->nonBlockingRead(channel, buf, length);
#else
    breakpoint();
    return 0;
#endif
}

/* static */ sval
ThinWireMgr::BlockingRead(uval channel, char *buf, uval length)
{
#ifndef FAST_REGRESS_ON_SIM
    return thinWireMgr->blockingRead(channel,buf,length);
#else
    breakpoint();
    return 0;
#endif
}

/* static */ sval
ThinWireMgr::BlockingWrite(uval channel, const char *buf, uval length)
{
#ifndef FAST_REGRESS_ON_SIM
    return thinWireMgr->blockingWrite(channel, buf, length);
#else
    thinWireMgr->blockingWrite(channel, buf, length);
    //    breakpoint();
#endif
}

/* static */ uval
ThinWireMgr::KillDaemon()
{
    thinWireMgr->runDaemon = 0;
    // Wake the daemon so that it will notice the change quickly.
    Scheduler::Unblock(thinWireMgr->daemon);
    return 0;
}

/* static */ uval
ThinWireMgr::SuspendDaemon()
{
#ifndef FAST_REGRESS_ON_SIM
    uval wasRunning = thinWireMgr->suspendDaemon == 0;
    if (wasRunning) {
	thinWireMgr->suspendDaemon = 1;
	// Wake the daemon so that it will notice the change quickly.
	Scheduler::Unblock(thinWireMgr->daemon);
    }
    return wasRunning;
#else
    return 0;
#endif
}

/* static */ uval
ThinWireMgr::RestartDaemon()
{
#ifndef FAST_REGRESS_ON_SIM
    uval wasRunning = thinWireMgr->suspendDaemon == 0;
    if (!wasRunning) {
	thinWireMgr->suspendDaemon = 0;
	Scheduler::Unblock(thinWireMgr->daemon);
    }
    return wasRunning;
#else
    return 0;
#endif
}
