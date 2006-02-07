/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MultiProc.C,v 1.5 2004/09/17 12:54:25 mostrows Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include "LinuxEnv.H"
#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerTimer.H>
#include <sys/TimerEvent.H>
#include <trace/traceLinux.h>
//#include <lk/Interrupt.H>
#include <sync/FairBLock.H>
extern "C" {
#define private __C__private
#include <asm/param.h>
#include <asm/hardirq.h>
#include <linux/timer.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#undef private
}
#include <sync/MPMsgMgr.H>

#include <alloc/PageAllocatorDefault.H>

extern void LinuxEnvSuspend();
extern void LinuxEnvResume();

extern "C" int smp_call_function (void (*func) (void *info), void *info,
				  int retry, int wait);

extern "C" int __cpu_up(unsigned int cpu);

struct LinuxSMPMsg: public MPMsgMgr::MsgSync {
    void (*func) (void *info);
    void *info;
    virtual void handle() {
	LinuxEnv le(SysCall);
	(*func)(info);
	reply();
    }
    LinuxSMPMsg(void (*f) (void *info), void* i):func(f), info(i) {};
};

struct LinuxSMPMsgAsync: public MPMsgMgr::MsgAsync {
    void (*func) (void *info);
    void *info;
    virtual void handle() {
	free();
	LinuxEnv le(SysCall);
	(*func)(info);
    }
    LinuxSMPMsgAsync(void (*f) (void *info), void* i):func(f), info(i) {};
};

int
smp_call_function (void (*func) (void *info), void *info,
		   int retry, int wait)
{

    MPMsgMgr::MsgSpace msgSpace;
    MPMsgMgr::Msg *msg;

    LinuxEnvSuspend();
    if (!wait) {
	msg = new(Scheduler::GetEnabledMsgMgr()) LinuxSMPMsgAsync(func, info);
    } else {
	msg = new(Scheduler::GetEnabledMsgMgr(), msgSpace)
	    LinuxSMPMsg(func, info);
    }

    for (VPNum vp = 0 ; vp<DREFGOBJ(TheProcessRef)->ppCount(); ++vp) {
	if (vp == Scheduler::GetVP()) continue;
	if (!cpu_online(vp)) continue;
	SysStatus rc = msg->send(vp);
	tassertMsg(_SUCCESS(rc),"MPMSG failure: %lx\n");
    }
    LinuxEnvResume();
    return 0;
}


struct AsyncTBMPMsg: public MPMsgMgr::MsgAsync {
public:
    volatile SysTime status[2];
    virtual void handle() {
	free();
	err_printf("SyncTB second vp\n");
	status[1] = 1;
	status[0] = 1;
	SyncBeforeRelease();
	while (status[0]!=1);
	disableHardwareInterrupts();
	SysTime other = status[0];

	uval i = 100;
	while (i-->0) {
	    status[1] = getClock();
	    other = status[0];
//	    setClock(status[1] + ((other - status[1])>>1));
	}
	other = status[0];
	enableHardwareInterrupts();
	err_printf("Times: %lx %lx\n", other, status[0]);
	status[1] = 0;
	SyncBeforeRelease();
    }
    AsyncTBMPMsg() {status[0] = 0 ; status[1]=0;};
};

void
SyncTB()
{
    MPMsgMgr::MsgSpace msgSpace;
    AsyncTBMPMsg *msg;

    err_printf("SyncTB....\n");
    msg = new(Scheduler::GetEnabledMsgMgr()) AsyncTBMPMsg();
    msg->send(1);
    err_printf("SyncTB....\n");

    while (msg->status[0]==0);
    msg->status[0]=1;
    SyncBeforeRelease();
    disableHardwareInterrupts();
    while (msg->status[1]) {
	msg->status[0] = getClock();
	SyncBeforeRelease();
    }
    enableHardwareInterrupts();

}

cpumask_t cpu_online_map;
cpumask_t cpu_possible_map;
cpumask_t cpu_available_map;
cpumask_t cpu_present_at_boot;

extern cpumask_t cpu_present_map;

// This is supposed to bring the cpu up, but really this is already done by
// K42 code
static SysStatus (*starter)(uval arg);
int
__cpu_up(unsigned int cpu)
{
    SysStatus rc;
    SysStatus retRC;
    cpu_set(cpu, cpu_online_map);
    LinuxEnvSuspend();
    rc = MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
				SysTypes::DSPID(0, cpu),
				starter, cpu, retRC);
    LinuxEnvResume();
    tassertRC(rc, "oops\n");

    return 0;
}

extern unsigned char __per_cpu_start[], __per_cpu_end[];
long unsigned int __per_cpu_offset[NR_CPUS];
extern "C" void do_openpic_setup_cpu(void);


// vp is the vp to be started, but this code runs on the cpu that is
// doing the starting
void
LinuxStartVP(VPNum vp, SysStatus (*initfn)(uval arg))
{
    if (vp==0) return;

    starter = initfn;

    LinuxEnv sc(SysCall);
    // We need to have this bit set early so that interrupt controller
    // can do IPI, but cpu_up expects it to be clear.
    cpu_clear(vp, cpu_online_map);
    int ret = cpu_up(vp);
    tassertMsg(ret==0,"Can't bring up cpu %d\n",vp);
}

void
LinuxSMPInit(VPNum vp, PageAllocatorRef pa)
{
    SysStatus rc = 0;
    if (vp==0) {
        /* this is an evil hack, but i can't find a better way to do it */
        memset(&cpu_online_map, sizeof(cpumask_t), 0);
        memset(&cpu_possible_map, sizeof(cpumask_t), 0);
        memset(&cpu_available_map, sizeof(cpumask_t), 0);
        memset(&cpu_present_at_boot, sizeof(cpumask_t), 0);

	err_printf("Linux-per-cpu data: %016lx - %016lx\n",
		   (void*)__per_cpu_start,(void*)__per_cpu_end);
	uval max = DREFGOBJ(TheProcessRef)->ppCount();
	uval addr;
	uval size = uval(&__per_cpu_end[0] - &__per_cpu_start[0]);

	size = ALIGN_UP(size, SMP_CACHE_BYTES);
	rc = DREF(pa)->allocPages(addr, ALIGN_UP(size*max, PG_SIZE));
	tassertMsg(_SUCCESS(rc),"No memory for per-cpu data: %lx\n",rc);

	for (VPNum i = 0; i < max ; ++i) {
	    uval ptr = addr + i * size;
	    __per_cpu_offset[i] = ptr - uval(&__per_cpu_start[0]);
	    memcpy((void*)ptr, &__per_cpu_start[0],
		   uval(&__per_cpu_end[0] - &__per_cpu_start[0]));

	    cpu_set(i, cpu_possible_map);
	    cpu_set(i, cpu_available_map);
	    cpu_set(i, cpu_present_at_boot);
	    cpu_set(i, cpu_present_map);
	}
	cpu_set(vp, cpu_online_map);

    }
}
