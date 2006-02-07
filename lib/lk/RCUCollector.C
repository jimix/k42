/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RCUCollector.C,v 1.3 2004/09/29 15:06:03 rosnbrg Exp $
 *****************************************************************************/
// Interface between Linux rcu and K42 garbage collection

#include "lkIncs.H"
#include "LinuxEnv.H"
#include <cobj/BaseObj.H>
#include <cobj/Obj.H>
#include <cobj/CObjRootSingleRep.H>
#include <cobj/CObjRoot.H>
#include <alloc/alloc.H>
#include <sync/atomic.h>
extern "C" {
#include <linux/rcupdate.h>
#include <linux/interrupt.h>
}

struct RCUCollector;
struct RCUCollector:public TimerEvent{
    friend void RCUCollectorInit(VPNum vp);
    struct list_head *delList;
    static RCUCollector* collectors[Scheduler::VPLimit];
    DEFINE_PINNEDGLOBAL_NEW(RCUCollector);
    COSMgr::ThreadMarker marker;
    static void Cleanup(uval arg) {
	if (((RCUCollector*)arg)->cleanup()) {
	    delete ((RCUCollector*)arg);
	}
    }
    uval cleanup();
    virtual void handleEvent() {
	when = SysTime(-1);
	Scheduler::DisabledScheduleFunction(Cleanup,(uval)this);
    }
    static void ClassInit(VPNum vp);
    void addItem(struct rcu_head *head);
    RCUCollector():delList(NULL) {};
};

RCUCollector* RCUCollector::collectors[Scheduler::VPLimit] = {NULL,};


uval
RCUCollector::cleanup() {
    Scheduler::Disable();
    COSMgr::MarkerState stat;

    DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(marker, stat);

    if (stat == COSMgr::ACTIVE) {

	disabledScheduleEvent(Scheduler::TicksPerSecond(),
			      TimerEvent::relative);
	Scheduler::Enable();
	return 0 ;
    }

    if (collectors[Scheduler::GetVP()] == this) {
	collectors[Scheduler::GetVP()] = NULL;

    }
    Scheduler::Enable();


    LinuxEnv le(SysCall);
    local_bh_disable();
    struct list_head* rcu = delList;
    while (rcu) {
	struct rcu_head* curr;
	curr = list_entry(rcu, struct rcu_head, list);
	rcu = rcu->next;

	if (curr->func) {
	    (*curr->func)(curr->arg);
	}
    }
    local_bh_enable();
    return 1;
}

void
RCUCollectorInit(VPNum vp)
{
}

void
RCUCollector::addItem(struct rcu_head *head)
{
    head->list.next = delList;
    delList = &head->list;

    if (when==0) {
	DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(marker);

	disabledScheduleEvent(Scheduler::TicksPerSecond(),
			      TimerEvent::relative);
    }
}

void
call_rcu(struct rcu_head* head, void (*func)(void *arg), void *arg)
{
    VPNum vp = Scheduler::GetVP();
    RCUCollector* cachedNew = NULL;
    RCUCollector* rcu;

    // Can't allocate while disabled so if we need to allocate a new
    // list, we fail first then try again after optimisitcally
    // pre-allocating

    head->func = func;
    head->arg = arg;
  retry:
    rcu = RCUCollector::collectors[vp];
    Scheduler::Disable();
    if (!rcu) {
	if (!cachedNew)
	    goto get_new_RCU;

	rcu = cachedNew;
	cachedNew = NULL;
    } else {
	COSMgr::MarkerState stat;
	DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(rcu->marker, stat);
	if (stat != COSMgr::ACTIVE) {
	    if (!cachedNew)
		goto get_new_RCU;

	    rcu = cachedNew;
	    cachedNew = NULL;
	}
    }

    rcu->addItem(head);
    RCUCollector::collectors[vp] = rcu;
    Scheduler::Enable();

    if (cachedNew) delete cachedNew;
    return;

  get_new_RCU:
    Scheduler::Enable();
    cachedNew = new RCUCollector;
    goto retry;

}


extern "C" void synchronize_kernel(void);

void
synchronize_kernel(void)
{
    COSMgr::ThreadMarker marker;
    DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(marker);
    Scheduler::DeactivateSelf();

    // Wait until gen count increments
    do {
	COSMgr::MarkerState stat;
	DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(marker, stat);
	if (stat != COSMgr::ACTIVE) break;
	Scheduler::DelayMicrosecs(50);
    } while (1);

    Scheduler::ActivateSelf();

}

