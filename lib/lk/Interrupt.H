#ifndef __INTERRUPT_H_
#define __INTERRUPT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Interrupt.H,v 1.2 2003/04/05 12:45:14 mostrows Exp $
 *****************************************************************************/
#ifdef __cplusplus
#include <lk/modes.H>
#include <scheduler/Scheduler.H>

#include <sync/RWLock.H>
#include <sync/RecLock.H>
#include <sync/Sem.H>

extern SysStatusUval wrap_softirq();

extern "C" {
#endif /* #ifdef __cplusplus */
extern int smp_processor_id();

extern void __k42_global_cli(void);
extern void __k42_global_sti(void);
extern unsigned long __k42_global_save_flags(void);
extern void __k42_global_restore_flags(unsigned long);

extern int in_interrupt();
extern int in_irq();
extern int in_softirq();

extern void release_irqlock(int cpu);
extern void hardirq_enter(int cpu);
extern void hardirq_exit(int cpu);
extern int hardirq_trylock(int cpu);
extern void synchronize_irq(void);


#ifdef __cplusplus
} // extern "C"

#endif /* #ifdef __cplusplus */

class LinuxGlobalLocks{
public:
    RecBLock cpuLock[NR_CPUS];
    RWBLock globalCLILock;
};



#endif /* #ifndef __INTERRUPT_H_ */
