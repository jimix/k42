/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Util.C,v 1.3 2004/02/27 17:14:18 mostrows Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: misc pty functionality functions
 ****************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/XHandleTrans.H>
#include <cobj/CObjRootSingleRep.H>
#include <sys/ProcessLinux.H>


extern "C" {
#include <asm/param.h>
}

extern "C" void HW_calibrate_decr(void);
extern "C" void HW_get_boot_time(struct rtc_time *rtc_time);
extern "C" void time_init(void);
void HW_calibrate_decr(void) {
}

void HW_get_boot_time(struct rtc_time *rtc_time) {
}


extern "C" void configureLinuxNonKernel(void);

extern unsigned long tb_ticks_per_jiffy;
extern unsigned long tb_ticks_per_sec;

void
ConfigureLinuxNonKernel(void)
{
    tb_ticks_per_jiffy = Scheduler::TicksPerSecond() / HZ;
    tb_ticks_per_sec   = Scheduler::TicksPerSecond();
    configureLinuxNonKernel();
}


extern "C" int k42_session_of_pgrp(int pgrp);
int
k42_session_of_pgrp(int pgrp)
{

    //FIXME: it may be possible that there is no pid to match pgrp,
    // but that doesn't mean that there's no process belonging to pgrp.
    ProcessLinux::LinuxInfo info;
    SysStatus rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(pgrp, info);
    if (_SUCCESS(rc)) {
	//There exists process with pid "pgrp" --- use its session
	return info.session;
    }
    return -1;
}

extern "C" int kill_pg(pid_t pgrp, int sig, int priv);



int
kill_pg(pid_t pgrp, int sig, int priv)
{
    err_printf("Kill pgrp: %d %d\n",pgrp, sig);
    DREFGOBJ(TheProcessLinuxRef)->kill(-pgrp, sig);
    // this will fail if there is a race between kill and termination
    // ignore it
    return 0;
}


extern "C" int is_orphaned_pgrp(int pgrp);
int is_orphaned_pgrp(int pgrp)
{
    return 0;
}

extern "C" void do_openpic_setup_cpu(void);
void do_openpic_setup_cpu(void) {};
