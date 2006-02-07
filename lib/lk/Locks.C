/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Locks.C,v 1.13 2004/09/11 01:10:10 mostrows Exp $
 *****************************************************************************/
//#define K42_NO_LOCK_OVERRIDE
#include "lkIncs.H"
#include <trace/trace.H>
#include <trace/traceLock.h>
#include <sync/FairBLock.H>
#include <sync/BLock.H>
#include <scheduler/Scheduler.H>
#include <scheduler/Thread.H>
#include "LinuxEnv.H"

extern "C"
{
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/preempt.h>
#include <linux/thread_info.h>
#include <asm/spinlock.h>
}


//
//
// K42_NO_LOCK_OVERRIDE allows us to switch to default implementations
// of linux interfaces, implemented with linux macro's.  The
// implementations here attempt to expand the operations so as to
// better optimize calls to K42 interrfaces.  Using linux macros may
// result in multiple calls to the k42 layer to bar/unbar threads,
// plus extra calls to _raw_spin_[un]lock.
//
//

typedef FairBLock LockType;

extern "C" int _raw_spin_trylock(spinlock_t *lock);
int _raw_spin_trylock(spinlock_t *lock)
{
    LockType* bl = (LockType*)lock;
    return bl->tryAcquire();
}



extern "C" void _raw_spin_lock(spinlock_t *lock);
void _raw_spin_lock(spinlock_t *lock)
{
    LockType* bl = (LockType*)lock;
    bl->acquire();
}

extern "C" void _raw_spin_unlock(spinlock_t *lock);
void _raw_spin_unlock(spinlock_t *lock)
{
    LockType* bl = (LockType*)lock;
    bl->release();
}

extern "C" int spin_is_locked(spinlock_t *lock);
int spin_is_locked(spinlock_t *lock)
{
    LockType* bl = (LockType*)&lock->lock;
    return bl->isLocked();
}

extern "C" int is_read_locked(rwlock_t *rw);
int is_read_locked(rwlock_t *rw)
{
    LockType* bl = (LockType*)&rw->lock;
    return bl->isLocked();
}

extern "C" int is_write_locked(rwlock_t *rw);
int is_write_locked(rwlock_t *rw)
{
    LockType* bl = (LockType*)&rw->lock;
    return bl->isLocked();
}


extern "C" void __preempt_spin_lock(spinlock_t *lock);
void __preempt_spin_lock(spinlock_t *lock)
{
    ASSERTENV;

    if (preempt_count()>1) {
	++preempt_count();
	_raw_spin_lock(lock);
	return;
    }
    preempt_enable();
    _raw_spin_lock(lock);
    preempt_disable();

    ASSERTENV;
}


extern "C" void __preempt_write_lock(rwlock_t *lock);
void __preempt_write_lock(rwlock_t *lock)
{
    ASSERTENV;

    if (preempt_count()>1) {
	++preempt_count();
	_raw_write_lock(lock);
	return;
    }
    preempt_enable();
    _raw_write_lock(lock);
    preempt_disable();

    ASSERTENV;
}

extern "C" void spin_unlock_wait(spinlock_t *lock);
void
spin_unlock_wait(spinlock_t *lock)
{
    while (spin_is_locked(lock)) {
	Scheduler::Yield();
    }
}


extern "C" void __k42_spin_lock(spinlock_t *lock);
void
__k42_spin_lock(spinlock_t *lock)
{
#ifdef K42_NO_LOCK_OVERRIDE
    spin_lock(lock);
#else
    ASSERTENV;

    struct thread_info *ti = current_thread_info();
    if (((ti->flags & K42_IN_INTERRUPT) == 0) &&
	(ti->preempt_count & PREEMPT_MASK) == 0) {
	Scheduler::Disable();
	Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SYSCALL);
	LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_SYSCALL);
	Scheduler::Enable();
    }
    ++ti->preempt_count;
    LockType* bl = (LockType*)lock;
    bl->acquire();

    ASSERTENV;


#endif
}

extern "C" void __k42_spin_unlock(spinlock_t *lock);
void
__k42_spin_unlock(spinlock_t *lock)
{
#ifdef K42_NO_LOCK_OVERRIDE
    spin_unlock(lock);
#else
    ASSERTENV;

    struct thread_info *ti = current_thread_info();
    LockType* bl = (LockType*)lock;
    bl->release();
    tassertMsg((ti->preempt_count & PREEMPT_MASK) != 0,
	       "preempt count should be non-zero\n");
    --ti->preempt_count;
    if (((ti->flags & K42_IN_INTERRUPT) == 0) &&
	(ti->preempt_count & PREEMPT_MASK) == 0) {
	Scheduler::Disable();
	tassertMsg((ti->preempt_count & PREEMPT_MASK) == 0,
	       "non-zero preempt_count() %lx\n", ti->preempt_count);
	LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_SYSCALL);
	Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SYSCALL);
	Scheduler::Enable();
    }
    ASSERTENV;

#endif
}


extern "C" void __k42_spin_lock_irq(spinlock_t *lock);
void
__k42_spin_lock_irq(spinlock_t *lock)
{
#ifdef K42_NO_LOCK_OVERRIDE
    spin_lock_irq(lock);
#else

    ASSERTENV;

    struct thread_info *ti = current_thread_info();

    if ( (ti->flags & (K42_IRQ_DISABLED|K42_IN_INTERRUPT)) == 0 ) {
	Scheduler::Disable();
	Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
	Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SYSCALL);
	LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_INTERRUPT);
	LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_SYSCALL);
	Scheduler::Enable();
    }
    ti->flags |= K42_IRQ_DISABLED;
    ti->preempt_count += (HARDIRQ_OFFSET+1);

    LockType* bl = (LockType*)lock;
    bl->acquire();

    ASSERTENV;

#endif
}

extern "C" void __k42_spin_lock_irqsave(spinlock_t *lock, uval *flags);
void
__k42_spin_lock_irqsave(spinlock_t *lock, uval *flags)
{
#ifdef K42_NO_LOCK_OVERRIDE
    spin_lock_irq(lock);
#else

    ASSERTENV;

    struct thread_info *ti = current_thread_info();

    Scheduler::Disable();
    *flags = hardirq_count();

    if ( (ti->flags & (K42_IRQ_DISABLED|K42_IN_INTERRUPT)) == 0 ) {
	Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
	Scheduler::DisabledLeaveGroupSelf(Thread::GROUP_LINUX_SYSCALL);
	LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_INTERRUPT);
	LinuxEnv::DisabledBarGroup(Thread::GROUP_LINUX_SYSCALL);
    }

    ti->flags |= K42_IRQ_DISABLED;
    ti->preempt_count += (HARDIRQ_OFFSET+1);

    Scheduler::Enable();

    LockType* bl = (LockType*)lock;
    bl->acquire();

    ASSERTENV;

#endif
}

extern "C" void __k42_spin_unlock_irqrestore(spinlock_t *lock, uval flags);
void
__k42_spin_unlock_irqrestore(spinlock_t *lock, uval flags)
{
#ifdef K42_NO_LOCK_OVERRIDE
    spin_unlock_irqrestore(lock, flags);
#else

    ASSERTENV;

    LockType* bl = (LockType*)lock;
    bl->release();

    struct thread_info *ti = current_thread_info();
    uval old = hardirq_count();

    // Reset hardirq bits, and subtract one from preempt count
    ti->preempt_count = flags | (ti->preempt_count & ~HARDIRQ_MASK);

    tassertMsg((ti->preempt_count & PREEMPT_MASK) != 0,
	       "preempt count should be non-zero\n");
    --ti->preempt_count;

    if (flags == 0) {
	ti->flags &= ~K42_IRQ_DISABLED;
    }

    // If in interrupt, we never touch any group bits
    if (ti->flags & K42_IN_INTERRUPT) {
	ASSERTENV;
    } else {
	Scheduler::Disable();
	if (! (ti->flags & K42_IRQ_DISABLED)) {
	    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_INTERRUPT);
	    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
	}
	if ((ti->preempt_count & PREEMPT_MASK) == 0) {
	    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_SYSCALL);
	    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SYSCALL);
	}
	ASSERTENV;
	Scheduler::Enable();
    }



#endif
}

extern "C" void __k42_spin_unlock_irq(spinlock_t *lock);
void
__k42_spin_unlock_irq(spinlock_t *lock)
{
#ifdef K42_NO_LOCK_OVERRIDE
    spin_unlock_irq(lock);
#else

    ASSERTENV;

    LockType* bl = (LockType*)lock;
    bl->release();

    struct thread_info *ti = current_thread_info();
    ti->preempt_count &= (PREEMPT_MASK|SOFTIRQ_MASK);
    --ti->preempt_count;
    ti->flags &= ~K42_IRQ_DISABLED;

    // If in interrupt, we never touch any group bits
    if ( ((ti->flags & K42_IN_INTERRUPT) == 0) &&
	((ti->preempt_count & HARDIRQ_MASK) == 0 ||
	 (ti->preempt_count & PREEMPT_MASK) == 0) ) {
	Scheduler::Disable();
	if ((ti->preempt_count & HARDIRQ_MASK) == 0) {
	    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_INTERRUPT);
	    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_INTERRUPT);
	}
	if ((ti->preempt_count & PREEMPT_MASK) == 0) {
	    LinuxEnv::DisabledUnbarGroup(Thread::GROUP_LINUX_SYSCALL);
	    Scheduler::DisabledJoinGroupSelf(Thread::GROUP_LINUX_SYSCALL);
	}
	Scheduler::Enable();
    }

    ASSERTENV;

#endif
}

extern "C" void lock_kernel(void);
extern "C" void unlock_kernel(void);
static uval global_lock_depth = 0;
static Thread* owner = NULL;
static spinlock_t *globalLock = NULL;
void GlobalLockInit(VPNum vp) {
    if (vp) return;
    globalLock = (spinlock_t*)allocGlobal(sizeof(spinlock_t));
    spin_lock_init(globalLock);
}

void
lock_kernel(void)
{
    if (owner==Scheduler::GetCurThreadPtr()) {
	++global_lock_depth;
	return;
    }
    spin_lock(globalLock);
    ++global_lock_depth;
    owner = Scheduler::GetCurThreadPtr();
}

void
unlock_kernel(void)
{
    --global_lock_depth;
    if (global_lock_depth==0) {
	owner = NULL;
	spin_unlock(globalLock);
    }
}
