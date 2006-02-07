/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Process.C,v 1.48 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include <Process.H>
#include <mem/Region.H>
#include <meta/MetaProcessServer.H>
#include <cobj/XHandleTrans.H>
#include "mem/FR.H"
#include "mem/FCM.H"
#include <sys/ProcessSet.H>
#include "mem/HATDefault.H"
#include "mem/PMLeaf.H"
#include "bilge/PerfMon.H"

#include "defines/MLSStatistics.H"
#include "exception/ExceptionLocal.H"
#include <bilge/LazyState.H>

#ifdef DO_MLS_STATS
/*static*/ void
MLSStatistics::StartTimer(uval i)
{
    exceptionLocal.mls.stime[i] = Scheduler::SysTimeNow();
}

/*static*/ void
MLSStatistics::DoneTimer(uval i)
{
    sval tmp = Scheduler::SysTimeNow()-exceptionLocal.mls.stime[i];
    if (tmp >0)
	exceptionLocal.mls.time[i] += tmp;
}

/*static*/ void
MLSStatistics::PrintAndZeroTimers(uval doPrint)
{
    VPNum vp = Scheduler::GetVP();

    if (doPrint) {
	err_printf("pf %4ld %10ld %10ld %10ld %10ld %10ld %10ld\n",
		   vp,
		   exceptionLocal.mls.time[0],
		   exceptionLocal.mls.time[1],
		   exceptionLocal.mls.time[2],
		   exceptionLocal.mls.time[3],
		   exceptionLocal.mls.time[4],
		   exceptionLocal.mls.time[5]
	    );
	err_printf("ov %4ld %10ld %10ld %10ld %10ld %10ld %10ld\n",
		   vp,
		   exceptionLocal.mls.time[10],
		   exceptionLocal.mls.time[11],
		   exceptionLocal.mls.time[12],
		   exceptionLocal.mls.time[13],
		   exceptionLocal.mls.time[14],
		   exceptionLocal.mls.time[15]
	    );
    }

    for (uval i=0; i<20;i++) {
	if (exceptionLocal.mls.time[i]) {
#if 0
	    if (doPrint) {
		err_printf("vp %x counter %x is %x\n",
			   vp, i, exceptionLocal.mls.time[i]);
	    }
#endif
	    exceptionLocal.mls.time[i] = 0;
	}
    }
}

#endif /* DO_MLS_STATS */

#include <stub/StubSchedulerService.H>
/*virtual*/ SysStatus
Process::callBreakpoint()
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    SysStatus rc;

    schedServ.initOHWithPID(
	getPID(),
	XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));
    rc = schedServ._doBreakpoint();
    return rc;
}

/*virtual*/ SysStatus
Process::dumpCObjTable()
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    SysStatus rc;

    schedServ.initOHWithPID(
	getPID(),
	XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));
    rc = schedServ._dumpCObjTable();
    return rc;
}

/*virtual*/ SysStatus
Process::dumpLeakInfo()
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    SysStatus rc;

    schedServ.initOHWithPID(
	getPID(),
	XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));
    rc = schedServ._dumpLeakInfo();
    return rc;
}

/*virtual*/ SysStatus
Process::resetLeakInfo()
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    SysStatus rc;

    schedServ.initOHWithPID(
	getPID(),
	XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));
    rc = schedServ._resetLeakInfo();
    return rc;
}

/*virtual*/ SysStatus
Process::perfMon(uval action, uval ids)
{
    if (action == START_MLS_STATS) {
	MLSStatistics::PrintAndZeroTimers(0);
    } else if (action == DUMP_ZERO_MLS_STATS) {
	MLSStatistics::PrintAndZeroTimers(1);
    } else if (action == START_PERF_MON) {
	PerfMon::Start();
    } else if (action == STOP_PERF_MON) {
	PerfMon::Stop();
    } else if (action == COLLECT_PERF_MON) {
	PerfMon::Collect();
    } else if (action == ZERO_PERF_MON) {
	PerfMon::Zero();
    } else if (action == PRINT_PERF_MON) {
	PerfMon::Print();
    } else if (action == BEGIN_PERF_MON) {
	PerfMon::Collect();
	PerfMon::Start();
    } else if (action == END_PERF_MON) {
	PerfMon::Stop();
	PerfMon::Print();
    } else {
	err_printf("unknown action requested in perfMon (1)\n");
	return _SERROR(1239, 0, EINVAL);
    }
    return 0;
}

/*
 * for debuggin ONLY - this is not reliable
 * gets the current VMapsR address of a virtual address
 * but does not pin - so it may not stay the same
 */
SysStatus
Process::getVMapsRAddr(uval vaddr, uval& vMapRaddr)
{
    RegionRef regionRef;
    SysStatus rc;
    rc = vaddrToRegion(vaddr, regionRef);
    if (_FAILURE(rc)) return rc;
    FCMRef fcmRef;
    uval offset;
    rc = DREF(regionRef)->
	vaddrToFCM(Scheduler::GetVP(), vaddr, 0, fcmRef, offset);
    if (_FAILURE(rc)) return rc;
    void *vmapsRaddr;
    rc = DREF(fcmRef)->getPage(offset, vmapsRaddr, 0);
    if (_FAILURE(rc)) return rc;
    DREF(fcmRef)->releasePage(offset);
    return 0;
}

/* virtual */ SysStatus
Process::_lazyReOpen(__CALLER_PID caller, __in sval file, __out uval &type,
		     __out ObjectHandle &oh,
		     __outbuf(dataLen:512) char *data, __out uval &dataLen)
{
    tassertMsg((caller==(uval)getPID()), "reopen by other process??\n");
    return lazyReOpen(file, type, oh, data, dataLen);
}

/* virtual */ SysStatus
Process::_lazyCopyState(__CALLER_PID caller, XHandle target)
{
    SysStatus rc;
    ProcessRef tpref;
    LazyState *ls;
    ObjRef tmp;
    TypeID type;

    rc = XHandleTrans::XHToInternal(target, caller, MetaObj::attach,
				    tmp, type);
    tassertWrn( _SUCCESS(rc), "woops\n");
    if (!_SUCCESS(rc)) return rc;

    // verify that type is cool
    if (!MetaProcessServer::isBaseOf(type)) {
	tassertWrn(0, "woops\n");
	return _SERROR(1510, 0, EINVAL);
    }
    tpref = (ProcessRef)tmp;

    getAddrLazyState(ls);		// get my lazy state
    return DREF(tpref)->lazyCopyState(ls);
}
