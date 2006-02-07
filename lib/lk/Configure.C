/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Configure.C,v 1.7 2004/09/17 12:54:25 mostrows Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include "LinuxEnv.H"
#include "InitCalls.H"
#include <alloc/PageAllocatorDefault.H>
extern "C" {
#define private __C__private
#define typename __C__typename
#define virtual __C__virtual
#define new __C__new
#define class __C__class
#include <linux/init.h>
#include <asm/machdep.h>
#undef typename
#undef private
#undef new
#undef class
#undef virtual
}
extern "C" void kmem_cache_init(void);
extern "C" void softirq_init(void);
extern void LinuxEnvInit(VPNum vp);
extern void LinuxSMPInit(VPNum vp, PageAllocatorRef pa);
extern void LinuxMemoryInit(VPNum vp, PageAllocatorRef pa);
extern void ConfigureLinuxTimer(VPNum vp);
extern "C" void rand_initialize(void);
extern "C" void time_init(void);
extern void RCUCollectorInit(VPNum vp);
extern "C" void init_workqueues(void);
void (*calibrate_delay)(void);
extern "C" void generic_calibrate_delay(void);
extern "C" void k42devfsInit();
extern "C" void unnamed_dev_init(void);
extern "C" void radix_tree_init(void);
extern void  SysFSNode_ClassInit();
extern void GlobalLockInit(VPNum vp);

void calibrate_decr()
{
}

void
ConfigureLinuxGeneric(VPNum vp)
{
    if (vp) return;

    // Provide default value
    if (!ppc_md.calibrate_decr)
	ppc_md.calibrate_decr = &calibrate_decr;

    SysFSNode_ClassInit();
    {
	LinuxEnv le(SysCall);
	time_init();
	calibrate_delay();
	unnamed_dev_init();
	rand_initialize();
    }
    k42devfsInit();
    INITCALL(cpucache_init);
}

void
ConfigureLinuxFinal(VPNum vp)
{
    if (vp) return;
    LinuxEnv le(SysCall);
    init_workqueues();

    RunInitCalls();
}

void
ConfigureLinuxDelayedFinal(VPNum vp)
{
    if (vp) return;
    LinuxEnv le(SysCall);
    RunInitCalls();
}


void
ConfigureLinuxEnv(VPNum vp,PageAllocatorRef pa)
{
    if (vp==0)
	calibrate_delay = &generic_calibrate_delay;

    LinuxEnvInit(vp);
    RCUCollectorInit(vp);
    LinuxMemoryInit(vp, pa);
    LinuxSMPInit(vp,pa);
    ConfigureLinuxTimer(vp);
    GlobalLockInit(vp);
    if (vp==0) {
	LinuxEnv le(SysCall);
	kmem_cache_init();
	softirq_init();
	radix_tree_init();
    }
}
