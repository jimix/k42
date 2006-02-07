/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCM.C,v 1.33 2004/10/29 16:30:30 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <kernIncs.H>
#include <FCM.H>
#include <FCMDefault.H>
#include <sys/ProcessSet.H>
#include <mem/Region.H>
#include <misc/HashSimple.H>

/* virtual */ SysStatus
FCM::resumeIO() 
{
    passertMsg(0, "woops\n");
    return 0;
}

/* virtual */ SysStatus
FCM::ioComplete(uval offset, SysStatus rc)
{
    (void) offset, (void)rc;
    passert(0, err_printf("this FCM doesn't support ioComplete\n"));
    return 0;
}

/* static */ SysStatus
FCM::PrintStatus(uval kind)
{
    ProcessID pid;
    BaseProcessRef pref;
    switch (kind) {
    case PendingFaults:
	pid = ProcessID(-1);		// start value for scan
	while (DREFGOBJ(TheProcessSetRef)->getNextPID(pid, pref)) {
	    uval nextVaddr = 0;
	    RegionRef reg;
	    while (DREF(pref)->getNextRegion(nextVaddr, reg)) {
		FCMRef fcm;
		uval regionStart, offset;
		DREF(reg)->getVaddr(regionStart);
		if (_SUCCESS(DREF(reg)->
			    vaddrToFCM(0, regionStart, 0, fcm, offset))&&
		   // some valid regions like RegionRedZone return 0 for fcm
		   fcm) {
		    if (DREF(fcm)->printStatus(PendingFaults)) {
			err_printf("Process %lx, Region Start %lx, FCM %p\n",
				   pid, regionStart, fcm);
		    }
		}
	    }
	}
	break;
    case MemoryUse:
    {
	HashSimple<uval, uval, AllocGlobal, 4> fcmHash;
	PM::Summary sum;
	uval dummy;
	uval total=0;
	uval pages;
	pid = ProcessID(-1);		// start value for scan
	while (DREFGOBJ(TheProcessSetRef)->getNextPID(pid, pref)) {
	    pages = 0;
	    uval nextVaddr = 0;
	    RegionRef reg;
	    while (DREF(pref)->getNextRegion(nextVaddr, reg)) {
		FCMRef fcm;
		uval regionStart, offset;
		DREF(reg)->getVaddr(regionStart);
		if (_SUCCESS(DREF(reg)->
			    vaddrToFCM(0, regionStart, 0, fcm, offset))&&
		   // some valid regions like RegionRedZone return 0 for fcm
		   fcm) {
		    if (0 == fcmHash.find(uval(fcm), dummy)) {
			fcmHash.add(uval(fcm), 0);
			DREF(fcm)->getSummary(sum);
			pages += sum.total;
		    }
		}
	    }
	    err_printf("Process 0x%lx using %ld pages\n",
		       pid, pages);
	    total+=pages;
	}
	err_printf("Total pages allocated to processes: %ld\n",total);
	//N.B. this code only reports each fcm once, even if it is in
	//     multiple address spaces.  Move the clear of hash above
	//     to report fcm in each process.
	uval restart = 0;
	while (0 != fcmHash.removeNext(dummy, dummy, restart));
	//N.B.  no matter what, only destroy once here.
	fcmHash.destroy();
    }
    break;
    default:
	break;
    }
    return 0;
}

/* virtual */ SysStatus
FCM::attachFR(FRRef frRef)
{
    passert(0, err_printf("should never be called"));
    return 0;				// not reached
}

