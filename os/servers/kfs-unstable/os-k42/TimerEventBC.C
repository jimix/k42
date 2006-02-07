/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TimerEventBC.C,v 1.3 2004/03/01 17:51:25 dilma Exp $
 *****************************************************************************/

#include "kfsIncs.H"
#include <sys/TimerEvent.H>
#include "TimerEventBC.H"
#include "BlockCacheK42.H"

/* static */ void
TimerEventBC::Event(uval arg)
{
    TimerEventBC *te = (TimerEventBC *)arg;
    (void) te->blkCache->flush();

    // re-schedule event here, rather than disabled, in case this took a
    // long time to run
    TimerEventBC::ScheduleEvent(te);
}

/* static */ void
TimerEventBC::ScheduleEvent(TimerEventBC *te)
{
    te->scheduleEvent(te->interval, TimerEvent::relative);
}

/* virtual */ void
TimerEventBC::handleEvent()
{
    SysStatus rc = Scheduler::DisabledScheduleFunction(TimerEventBC::Event,
						       (uval)this);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);
}
