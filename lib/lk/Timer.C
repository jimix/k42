/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Timer.C,v 1.33 2005/07/25 20:04:22 mostrows Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include "LinuxEnv.H"
#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerTimer.H>
#include <sys/TimerEvent.H>
#include <trace/traceLinux.h>
//#include <lk/Interrupt.H>
#include <sync/FairBLock.H>
#include <trace/traceLinux.h>
extern "C" {
#define private __C__private
#define typename __C__typename
#define virtual __C__virtual
#define new __C__new
#define class __C__class
#include <asm/param.h>
#include <asm/hardirq.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <asm/time.h>
#include <asm/machdep.h>
#undef private
#undef typename
#undef new
#undef class
#undef virtual
}

extern "C" int del_timer(struct timer_list *timer);
extern "C" int __mod_timer(struct timer_list *timer, unsigned long expires);
extern "C" unsigned long volatile __k42_jiffies();
extern "C" void add_timer_on(struct timer_list *timer, int cpu);
extern "C" int mod_timer(struct timer_list *timer, unsigned long expires);
extern "C" unsigned long __get_jiffies(void);

extern "C" struct timespec xtime;
struct timespec xtime __attribute__ ((aligned (16)));

extern "C" struct timespec current_kernel_time(void);
struct timespec
current_kernel_time(void)
{
    return xtime;
}

struct TimerEventXTime : public TimerEvent {
    DEFINE_PINNEDGLOBAL_NEW(TimerEventXTime);
    TimerEventXTime() {
	handleEvent();	// initialize xtime and schedule first timeout
    };
    virtual void handleEvent() {
	const SysTime now = Scheduler::SysTimeNow();
	const uval tps = Scheduler::TicksPerSecond();
	xtime.tv_sec = kernelInfoLocal.systemGlobal.epoch_sec + (now / tps);
	xtime.tv_nsec = (kernelInfoLocal.systemGlobal.epoch_usec * 1000) +
					    (((now % tps) * 1000000000) / tps);
	if (xtime.tv_nsec >= 1000000000) {
	    xtime.tv_sec += 1;
	    xtime.tv_nsec -= 1000000000;
	}
	(void) disabledScheduleEvent((tps*7)/8, TimerEvent::relative);
    }
};

static uval jiffyStart = 0;
static uval jiffyTicks = 0x100000;  // Initial good guesses at these values
static uval timerMask = ~0x3ffffULL;

unsigned long __get_jiffies(void)
{
    return Scheduler::SysTimeNow()/jiffyTicks;
}

struct TimerAnchor* timerBase[NR_CPUS] = {NULL,};

// This is a TimerEvent derived object, and thus includes an AutoList.
// We must be able to enforce the rule that no timer removals are done
// on different CPU's than the original registration.  Thus, we can't
// subvert the AutoList interfaces for other lists.  nextFree is used
// as a singly-linked list.  We use two different lists in TimerAnchor
// (expired, purge) to track timer's whose functions need to run and
// timers that need to be cleaned up.  In managing these two lists, we
// keep track of whether or not an object is on a list by virtue of
// the nextFree pointer, non-NULL means it's on a list.  But this also
// implies that we need a non-null value for the last node on the
// list, and thus we use LIST_END to mark that (instead of NULL)
#define LIST_END ((struct timer_base_s*)0xdeadbeefdeadbeefULL)
struct timer_base_s: public TimerEvent {
    enum TimerState { Reset, Waiting, Discarded, Fired };
    uval state;
    struct timer_list* timer;
    VPNum vp;
    timer_base_s *nextFree;

    DEFINE_PINNEDGLOBAL_NEW(timer_base_s);
    timer_base_s(VPNum v, struct timer_list *t):
	timer(t), state(Reset), vp(v), nextFree(NULL) {
    };
    virtual SysTime scheduleEvent(SysTime whenTime, Kind kind) {
	tassertMsg(nextFree==NULL,"freed event being registered\n");
	return TimerEvent::scheduleEvent(whenTime, kind);
    }
    virtual SysTime disabledScheduleEvent(SysTime whenTime, Kind kind) {
	tassertMsg(nextFree==NULL,"freed event being registered\n");
	return TimerEvent::disabledScheduleEvent(whenTime, kind);
    }
    SysStatus disabledRegister() {
	SysStatus rc;
	SysTime now = Scheduler::SysTimeNow();
	uval expire = timer->expires * jiffyTicks;

	if (expire < now) {
	    expire+=jiffyTicks;
	}
	tassertMsg(nextFree==NULL,"freed event being registered\n");
	/* Mark us as waiting since disabledScheduleEvent may actually
	 * run the timer, and handleEvent() expect this state. */
	state = Waiting;
	rc = disabledScheduleEvent(expire, TimerEvent::absolute);
	if (_FAILURE(rc)) {
	    state = Reset;
	}
	return rc;
    }
    virtual void handleEvent();
    static void Event(uval arg);
};

extern "C" void RunTimer(struct softirq_action *h);

struct TimerAnchor {
    FairBLock lock;
    struct timer_base_s* expired;
    struct timer_base_s* purge;
    struct timer_list *running_timer;
    friend void RunTimer(struct softirq_action *h);
    void runTimer();
    void cleanUp();
    int delTimer(struct timer_list* timer, uval sync);
    void locked_discard(timer_base_s *base);
    DEFINE_PINNEDGLOBAL_NEW(TimerAnchor);
    TimerAnchor():expired(LIST_END), purge(LIST_END), running_timer(NULL) {
	lock.init();
    }

protected:
    struct TimerAddMsg:public MPMsgMgr::MsgSync {
	struct timer_base_s *base;
	SysStatus rc;

	virtual void handle() {
	    VPNum vp = Scheduler::GetVP();
	    // Sync message, TimerAnchor is locked.

	    base->timer->entry.next = (struct list_head*)&timerBase[vp];
	    base->timer->entry.prev = base->timer->entry.next;
	    rc = base->disabledRegister();
	    reply();
	}
    };
public:
    SysStatus locked_timerAdd(struct timer_list *timer, VPNum vp)
    {
	MPMsgMgr::MsgSpace msgSpace;
	TimerAddMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr(), msgSpace) TimerAddMsg;
	msg->base = new timer_base_s(vp, timer);

	SysStatus rc = msg->send(SysTypes::DSPID(0, vp));
	tassertMsg(_SUCCESS(rc), "send failed\n");
	return msg->rc;
    };
    static void ClassInit(VPNum vp) {
	timerBase[vp] = new TimerAnchor;
	err_printf("timerBase[%ld] %p\n", (uval) vp, timerBase[vp]);
    };
} ____cacheline_aligned_in_smp;

void
RunTimer(struct softirq_action *h)
{
    VPNum vp = Scheduler::GetVP();
    timerBase[vp]->runTimer();
}

extern "C" void SimOS_calibrate_decr(void);
void
SimOS_calibrate_decr(void)
{
    struct div_result divres;
    unsigned long freq = Scheduler::TicksPerSecond();
    tb_ticks_per_jiffy = freq / HZ;
    tb_ticks_per_sec = tb_ticks_per_jiffy * HZ;
    tb_ticks_per_usec = freq / 1000000;
    tb_to_us = mulhwu_scale_factor(freq, 1000000);
    div128_by_32( 1024*1024, 0, tb_ticks_per_sec, &divres );
    tb_to_xs = divres.result_low;
}

extern "C" void SimOS_get_boot_time(struct rtc_time *rtc_tm);
void
SimOS_get_boot_time(struct rtc_time *rtc_tm)
{
    memset(rtc_tm, 0, sizeof(struct rtc_time));
}

extern "C" void HW_get_boot_time(struct rtc_time *rtc_tm);
extern "C" void HW_calibrate_decr(void);

void
ConfigureLinuxTimer(VPNum vp)
{
    if (vp==0) {
	jiffyStart = Scheduler::SysTimeNow();
	jiffyTicks = SchedulerTimer::TicksPerSecond() / HZ;
	uval tmp = 1;
	uval log = 0;
	while (tmp<jiffyTicks) { tmp = tmp<<1ULL; ++log; };
	log-=1;
	timerMask = ~((1ULL<<log)-1ULL);

	if (KernelInfo::OnSim()) {
	    ppc_md.calibrate_decr = SimOS_calibrate_decr;
	    ppc_md.get_boot_time  = SimOS_get_boot_time;
	} else {
	    ppc_md.calibrate_decr = HW_calibrate_decr;
	    ppc_md.get_boot_time  = HW_get_boot_time;
	}

	(void) new TimerEventXTime;

	open_softirq(TIMER_SOFTIRQ, RunTimer, NULL);
    }
    TimerAnchor::ClassInit(vp);
}

/* virtual */ void
timer_base_s::handleEvent()
{
    timer_base_s *popped;

    if (!CompareAndStoreSynced(&state, Waiting, Fired)) {
	/* We're not expected to fire */
	tassertMsg(state == Discarded,
		   "Expecting to have been discarded, got %ld\n", state);
	return;
    }

    tassertMsg(head() == NULL && next()==NULL,
	       "TimerEventLinux on list: %p\n", this);
    tassertMsg(timer == NULL || nextFree == NULL, "already on popped/free list\n");

    // Prepend ourselved atomically to front of free list
    do {
	popped = timerBase[vp]->expired;
	nextFree = popped;
    } while (!CompareAndStoreSynced((uval*)&timerBase[vp]->expired,
				    uval(popped), uval(this)));

    tassertMsg(head() == NULL && next()==NULL,
	       "TimerEventLinux on list: %p\n", this);
    tassertMsg(popped==LIST_END ||
	       (popped->head() == NULL && popped->next()==NULL),
	       "TimerEventLinux on list: %p\n", popped);

    tassertMsg(this!=nextFree, "list loop\n");

    // Use this to indicate we've popped and are waiting on TimerList
    when = 0;

    // First one to insert starts a thread
    if (popped == LIST_END) {
	SysStatus rc = Scheduler::DisabledScheduleFunction(Event,
							   (uval)this);
	tassertSilent( _SUCCESS(rc), BREAKPOINT );
    }
}

void
timer_base_s::Event(uval arg)
{
    LinuxEnv le(Interrupt);

    raise_softirq(TIMER_SOFTIRQ);

    // Destructor now runs softIRQ code marked above
}

int
mod_timer(struct timer_list *timer, unsigned long expires)
{
    if (timer->expires==expires && timer_pending(timer)) {
	return 1;
    }
    return __mod_timer(timer, expires);
}

void
add_timer_on(struct timer_list *timer, int cpu)
{
    if (VPNum(cpu)!=Scheduler::GetVP()) {
	TimerAnchor *ta = timerBase[cpu];
	LinuxEnvSuspend();
	ta->lock.acquire();
	ta->locked_timerAdd(timer,VPNum(cpu));
	ta->lock.release();
	LinuxEnvResume();
    } else {
	__mod_timer(timer, timer->expires);
    }
}

int
del_timer(struct timer_list *timer)
{
    TimerAnchor* ta = (TimerAnchor*)timer->entry.next;
    if (ta) {
	return ta->delTimer(timer, 0);
    }
    return 0;
}

int
del_timer_sync(struct timer_list *timer)
{
    TimerAnchor* ta = (TimerAnchor*)timer->entry.next;
    if (ta) {
	return ta->delTimer(timer,1);
    }
    return 0;
}

int
TimerAnchor::delTimer(struct timer_list *timer, uval sync)
{
    uval running = 0;
    do {
	if (timer->base && sync) {
	    while (running_timer==timer) {
		Scheduler::Yield();
	    }
	}
	if (timer->base) {
	    LinuxEnvSuspend();
	    lock.acquire();
	    if (timer->base) {
		locked_discard(timer->base);
		timer->base = NULL;
		running = 1;
	    }
	    lock.release();
	    LinuxEnvResume();
	}
    } while (timer->base && sync);
    return running;
}

void
TimerAnchor::runTimer()
{
    timer_base_s *list = (timer_base_s*)SwapVolatile((uval*)&expired,
						       (uval)LIST_END);

    timer_base_s *newList = LIST_END;

    LinuxEnvSuspend();
    lock.acquire();
    // Reverse the order of the list, since "list" stores most
    // recently popped timer first and we should execute them in
    // the right order. (Note: we assume this is a short list.)
    while (list!=LIST_END) {
	tassertMsg(list->head()==NULL,
		   "Timer on list %p\n",list);
	timer_base_s *n = list->nextFree;
	tassertMsg(n==LIST_END || n->head()==NULL,
		   "Timer on list %p\n",n);
	list->nextFree = newList;
	newList = list;
	list = n;

    }
    list = newList;

    while (list!=LIST_END) {
	timer_base_s *curr = list;
	list = list->nextFree;

	curr->nextFree = NULL;
	if (curr->state != timer_base_s::Fired) {
	    tassertMsg(curr->state == timer_base_s::Discarded,
		       "Only expecting a discarded state\n");
	    delete curr;
	    continue;
	}

	tassertMsg(curr->head()==NULL,
		   "Timer on list %p\n",curr);
	if (curr->timer) {
	    running_timer = curr->timer;
	    void (*fn)(unsigned long) = curr->timer->function;
	    unsigned long data = curr->timer->data;
	    curr->timer->base = NULL;

	    lock.release();
	    LinuxEnvResume();
	    TraceOSLinuxTimer(uval(fn), data);
	    (*fn)(data);
	    KillMemory();
	    SyncBeforeRelease();
	    running_timer = NULL;
	    LinuxEnvSuspend();
	    lock.acquire();
	    tassertMsg(curr->head()==NULL,
		       "Timer on list %p\n",curr);
	}
	delete curr;
    }

    lock.release();
    LinuxEnvResume();

    cleanUp();
}

void
TimerAnchor::locked_discard(timer_base_s *base)
{
    tassertMsg(purge==LIST_END || purge!=base->nextFree, "list loop\n");
    base->timer = NULL;

    uval old;
    do {
	old = base->state;
    } while (!CompareAndStoreSynced(&base->state, old,
				    timer_base_s::Discarded));


    // If the timer was in a waiting state, it should be put on the free list.
    // If it had already expired (handleEvent()), RunTimer will do this.
    if (old == timer_base_s::Waiting) {
	base->nextFree = purge;
	purge = base;
    }
}

void
TimerAnchor::cleanUp() {
    if (purge==LIST_END) return;

    LinuxEnvSuspend();
    lock.acquire();
    timer_base_s *list = purge;
    purge = LIST_END;
    lock.release();
    LinuxEnvResume();

    while (list!=LIST_END) {
	timer_base_s *n = list;
	tassertMsg(!list || list!=list->nextFree, "list loop\n");
	list = list->nextFree;

	n->nextFree = NULL;
	n->scheduleEvent(uval(-1), TimerEvent::reset);
	n->state = timer_base_s::Reset;
	tassertMsg(n->head() == NULL, "base still on list %p\n", n);
	delete n;
    }

}

#define k42_lock(linuxLock) ((FairBLock*)&(linuxLock)->lock[0])
int
__mod_timer(struct timer_list * timer, unsigned long expires)
{
    TimerAnchor *old_anchor;
    TimerAnchor *new_anchor;
    TimerAnchor *first = NULL;
    TimerAnchor *second = NULL;
    VPNum vp = Scheduler::GetVP();
    if (!in_irq()) {
	local_bh_disable();
    }

    LinuxEnvSuspend();
    uval old = preempt_count();

    if (!timer->base) {
	timer->entry.next = NULL;
    }

  repeat:
    old_anchor = (TimerAnchor*)timer->entry.next;
    new_anchor = timerBase[vp];
    if (!old_anchor || new_anchor == old_anchor) {
	first = new_anchor;
    } else if (new_anchor<old_anchor) {
	first = new_anchor;
	second = old_anchor;
    } else {
	first = old_anchor;
	second = new_anchor;
    }

    first->lock.acquire();
    if (second) {
	second->lock.acquire();
    }

    if ( old_anchor != (TimerAnchor*)timer->entry.next ) {
	if (second) second->lock.release();
	first->lock.release();
	goto repeat;
    }

    int ret = 0;
    if (old_anchor) {
	old_anchor->locked_discard(timer->base);
	timer->base = NULL;
	ret = 1;
    }
    timer->expires = expires;
    timer->base = new timer_base_s(vp, timer);

    timer->entry.next = (struct list_head*)new_anchor;
    timer->entry.prev = timer->entry.next;

    Scheduler::Disable();
    timer->base->disabledRegister();
    Scheduler::Enable();

    if (second) {
	second->lock.release();
    }
    first->lock.release();

    LinuxEnvResume();

    if (!in_irq()) {
	local_bh_enable();
    }

    return ret;
}

