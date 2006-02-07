/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Semaphore.C,v 1.1 2004/02/27 17:14:32 mostrows Exp $
 *****************************************************************************/

#include "lkIncs.H"
#include <sys/KernelInfo.H>
extern "C" {
#include <asm/semaphore.h>
}
#include <scheduler/Scheduler.H>
#include <sync/Sem.H>
#include "LinuxEnv.H"
void
down(struct semaphore * sem)
{
    Semaphore* s = (Semaphore*)sem;
    LinuxEnv *sc = getLinuxEnv();
    if (sc->mode == LongThread) {
	Scheduler::DeactivateSelf();
    }
    s->P();
    if (sc->mode == LongThread) {
	Scheduler::ActivateSelf();
    }
}

int
down_interruptible(struct semaphore * sem)
{
    Semaphore* s = (Semaphore*)sem;
    LinuxEnv *sc = getLinuxEnv();
    if (sc->mode == LongThread) {
	Scheduler::DeactivateSelf();
    }
    s->P();
    if (sc->mode == LongThread) {
	Scheduler::ActivateSelf();
    }
    return 0;
}

int
down_trylock(struct semaphore * sem)
{
    Semaphore* s = (Semaphore*)sem;
    //Linux success == 0
    return !s->tryP();
}

void
up(struct semaphore * sem)
{
    Semaphore* s = (Semaphore*)sem;
    s->V();
}

